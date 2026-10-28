#include "get_events_ics_feed.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_ical_config.h"
#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/event_ical.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

// EV2 §1.2 — GET /api/events/feed.ics: the public subscribe feed.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

crow::response GetFeed(EndpointTestHelper& helper, const std::string& query = "") {
    return Request(helper, crow::HTTPMethod::Get, "/api/events/feed.ics", "", query);
}

// Whether the calendar has the entry for `eventId`, found by its UID line.
bool HasEvent(const std::string& ics, int64_t eventId) {
    std::string uidLine = "UID:" + Events::BuildEventUid(eventId, App::kICalUidDomain) + "\r\n";
    return ics.find(uidLine) != std::string::npos;
}

size_t EntryCount(const std::string& ics) {
    size_t count = 0;
    for (size_t at = ics.find("BEGIN:VEVENT"); at != std::string::npos;
         at = ics.find("BEGIN:VEVENT", at + 1)) {
        ++count;
    }
    return count;
}

TEST(GetEventsICalFeedTest, ServesApprovedEventsAsACalendar) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ServesApprovedEventsAsACalendar", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t nowUs = NowUs(transaction);
        EventSpec tomorrowSpec{ "tomorrow", nowUs + Events::kUsPerDay };
        tomorrowSpec.venueId = world.theCuff;
        int64_t tomorrow = AddEvent(transaction, databaseHelper, world, tomorrowSpec);
        // Past the list's 42-day default, inside the feed's 60.
        int64_t inFiftyDays = AddEvent(transaction, databaseHelper, world,
                                       { "in-fifty-days", nowUs + 50 * Events::kUsPerDay });
        int64_t pending = AddEvent(transaction, databaseHelper, world,
                                   { "pending", nowUs + Events::kUsPerDay, "pending" });
        int64_t rejected = AddEvent(transaction, databaseHelper, world,
                                    { "rejected", nowUs + Events::kUsPerDay, "rejected" });

        crow::response resp = GetFeed(helper);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(resp.get_header_value("Content-Type"), "text/calendar; charset=utf-8");
        EXPECT_EQ(resp.get_header_value("X-PUBLISHED-TTL"), "PT1H");
        EXPECT_EQ(resp.get_header_value("Cache-Control"), "max-age=3600");
        EXPECT_EQ(resp.body.rfind("BEGIN:VCALENDAR\r\n", 0), 0u);
        EXPECT_NE(resp.body.find("END:VCALENDAR\r\n"), std::string::npos);

        EXPECT_TRUE(HasEvent(resp.body, tomorrow));
        EXPECT_TRUE(HasEvent(resp.body, inFiftyDays));
        EXPECT_FALSE(HasEvent(resp.body, pending));
        EXPECT_FALSE(HasEvent(resp.body, rejected));
        EXPECT_EQ(EntryCount(resp.body), 2u);
        EXPECT_NE(resp.body.find("SUMMARY:Event tomorrow\r\n"), std::string::npos);
        EXPECT_NE(resp.body.find("LOCATION:The Cuff\r\n"), std::string::npos);
    });
    Auth::ServerConfig::Shutdown();
}

// The same filters as /api/events/upcoming, so a "subscribe to this view" link
// is the list's own query string.
TEST(GetEventsICalFeedTest, FiltersIncludeAndExclude) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("FiltersIncludeAndExclude", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        EventSpec dragSpec{ "drag", tomorrow };
        dragSpec.venueId = world.theCuff;
        dragSpec.categorySlugs = std::vector<std::string>{ "drag" };
        int64_t drag = AddEvent(transaction, databaseHelper, world, dragSpec);
        // Inherits dance + trans from the series.
        EventSpec t4tSpec{ "t4t", tomorrow };
        t4tSpec.venueId = world.sunsetTavern;
        t4tSpec.seriesSlug = "t4t";
        int64_t t4t = AddEvent(transaction, databaseHelper, world, t4tSpec);

        auto onlyHas = [&](const std::string& query, int64_t eventId) {
            crow::response resp = GetFeed(helper, query);
            ASSERT_EQ(resp.code, 200) << query << ": " << resp.body;
            EXPECT_EQ(EntryCount(resp.body), 1u) << query;
            EXPECT_TRUE(HasEvent(resp.body, eventId)) << query;
        };
        onlyHas("category=drag", drag);
        onlyHas("category=dance", t4t);
        onlyHas("scene=trans", t4t);
        onlyHas("neighborhood=capitol-hill", drag);
        onlyHas("series_id=" + StringFromInt(world.t4t), t4t);

        // Nothing matches: still a valid, empty calendar.
        crow::response none = GetFeed(helper, "category=bar");
        ASSERT_EQ(none.code, 200) << none.body;
        EXPECT_EQ(EntryCount(none.body), 0u);
        EXPECT_NE(none.body.find("BEGIN:VCALENDAR\r\n"), std::string::npos);
        EXPECT_NE(none.body.find("END:VCALENDAR\r\n"), std::string::npos);
    });
    Auth::ServerConfig::Shutdown();
}

// Each entry links back to its event page on the site, whose address is the
// website_address_login setting (honuware's "the site" for mail links too).
TEST(GetEventsICalFeedTest, EntriesLinkToTheirEventPage) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EntriesLinkToTheirEventPage", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "linked", NowUs(transaction) + Events::kUsPerDay });

        helper.GetSecretsHelper()->AddSecret(
            transaction, Secrets::kWebsiteAddressLogin, "https://cf.example/");
        crow::response linked = GetFeed(helper);
        ASSERT_EQ(linked.code, 200) << linked.body;
        EXPECT_NE(linked.body.find("DESCRIPTION:https://cf.example/events/" + StringFromInt(eventId)
                                   + "\r\n"),
                  std::string::npos);

        // No address configured: no link — and with no description or link of
        // its own, no DESCRIPTION line at all.
        helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kWebsiteAddressLogin, "");
        crow::response unlinked = GetFeed(helper);
        ASSERT_EQ(unlinked.code, 200) << unlinked.body;
        EXPECT_EQ(unlinked.body.find("DESCRIPTION:"), std::string::npos);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
