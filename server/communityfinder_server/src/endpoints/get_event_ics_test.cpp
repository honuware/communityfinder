#include "get_event_ics.h"

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

// EV2 §1.3 — GET /api/events/<int>/ical: one approved event as a download.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

crow::response GetICal(EndpointTestHelper& helper, int64_t eventId) {
    return Request(helper, crow::HTTPMethod::Get,
                   "/api/events/" + StringFromInt(eventId) + "/ical");
}

TEST(GetEventICalTest, DownloadsOneApprovedEvent) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DownloadsOneApprovedEvent", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        EventSpec spec{ "download", tomorrow };
        spec.venueId = world.sunsetTavern;
        int64_t eventId = AddEvent(transaction, databaseHelper, world, spec);
        AddEvent(transaction, databaseHelper, world, { "another", tomorrow });
        helper.GetSecretsHelper()->AddSecret(
            transaction, Secrets::kWebsiteAddressLogin, "https://cf.example");

        crow::response resp = GetICal(helper, eventId);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(resp.get_header_value("Content-Type"), "text/calendar; charset=utf-8");
        EXPECT_EQ(resp.get_header_value("Content-Disposition"),
                  "attachment; filename=\"event-" + StringFromInt(eventId) + ".ics\"");
        EXPECT_EQ(resp.body.rfind("BEGIN:VCALENDAR\r\n", 0), 0u);

        // Only this event, not the other approved one.
        size_t first = resp.body.find("BEGIN:VEVENT");
        ASSERT_NE(first, std::string::npos);
        EXPECT_EQ(resp.body.find("BEGIN:VEVENT", first + 1), std::string::npos);
        EXPECT_NE(resp.body.find("UID:" + Events::BuildEventUid(eventId, App::kICalUidDomain)
                                 + "\r\n"),
                  std::string::npos);
        EXPECT_NE(resp.body.find("SUMMARY:Event download\r\n"), std::string::npos);
        EXPECT_NE(resp.body.find("LOCATION:Sunset Tavern\r\n"), std::string::npos);
        EXPECT_NE(resp.body.find("DESCRIPTION:https://cf.example/events/" + StringFromInt(eventId)
                                 + "\r\n"),
                  std::string::npos);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetEventICalTest, AnythingButApprovedIsNotFound) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnythingButApprovedIsNotFound", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        int64_t pending = AddEvent(transaction, databaseHelper, world, { "pending", tomorrow, "pending" });
        int64_t rejected = AddEvent(transaction, databaseHelper, world, { "rejected", tomorrow, "rejected" });

        EXPECT_EQ(GetICal(helper, pending).code, 404);
        EXPECT_EQ(GetICal(helper, rejected).code, 404);
        EXPECT_EQ(GetICal(helper, 999999).code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

// Unlike the detail page, there is no curator preview: a calendar file is only
// ever for a public event.
TEST(GetEventICalTest, ACuratorGetsNoPendingDownloadEither) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ACuratorGetsNoPendingDownloadEither", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        int64_t pending = AddEvent(transaction, databaseHelper, world, { "pending", tomorrow, "pending" });
        int64_t approved = AddEvent(transaction, databaseHelper, world, { "approved", tomorrow });
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        EXPECT_EQ(GetICal(helper, pending).code, 404);
        EXPECT_EQ(GetICal(helper, approved).code, 200);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
