#include "get_upcoming_events.h"

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/types.h"

// EV1 §5.1 — GET /api/events/upcoming over HTTP, signed out throughout.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

Json::Value GetUpcoming(EndpointTestHelper& helper, const std::string& query = "") {
    crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/events/upcoming", "", query);
    EXPECT_EQ(resp.code, 200) << query << ": " << resp.body;
    return Json::Value::FromText(resp.body);
}

std::set<int64_t> IdSet(const Json::Value& body) {
    std::vector<int64_t> ids = IdsOf(body["items"].GetArray());
    return std::set<int64_t>(ids.begin(), ids.end());
}

TEST(GetUpcomingEventsTest, ApprovedEventsInTheDefaultWindowOnly) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ApprovedEventsInTheDefaultWindowOnly", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        int64_t approved = AddEvent(transaction, databaseHelper, world, { "approved", tomorrow });
        AddEvent(transaction, databaseHelper, world, { "pending", tomorrow, "pending" });
        AddEvent(transaction, databaseHelper, world, { "rejected", tomorrow, "rejected" });
        AddEvent(transaction, databaseHelper, world,
                 { "in-two-months", tomorrow + 60 * Events::kUsPerDay });

        Json::Value body = GetUpcoming(helper);
        EXPECT_EQ(IdSet(body), std::set<int64_t>{ approved });
        // The window served: now → +42 days.
        EXPECT_EQ(body["to_us"].Get<int64_t>() - body["from_us"].Get<int64_t>(),
                  Events::kUpcomingDefaultWindowDays * Events::kUsPerDay);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetUpcomingEventsTest, TheWindowIsCappedAndEchoed) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheWindowIsCappedAndEchoed", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t nowUs = NowUs(transaction);
        int64_t inTwoMonths = AddEvent(transaction, databaseHelper, world,
                                       { "in-two-months", nowUs + 60 * Events::kUsPerDay });
        AddEvent(transaction, databaseHelper, world,
                 { "in-four-months", nowUs + 120 * Events::kUsPerDay });

        Json::Value body = GetUpcoming(
            helper, "from_us=" + StringFromInt(nowUs) +
                    "&to_us=" + StringFromInt(nowUs + 365 * Events::kUsPerDay));
        EXPECT_EQ(IdSet(body), std::set<int64_t>{ inTwoMonths });
        EXPECT_EQ(body["from_us"].Get<int64_t>(), nowUs);
        EXPECT_EQ(body["to_us"].Get<int64_t>(), nowUs + Events::kUpcomingMaxWindowDays * Events::kUsPerDay);

        // An unparseable number counts as absent, not an error.
        EXPECT_TRUE(GetUpcoming(helper, "from_us=soon&venue_id=x")["items"].GetArray().empty());
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetUpcomingEventsTest, EachFilter) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachFilter", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + Events::kUsPerDay;
        EventSpec dragAtTheCuff{ "drag", startsAtUs };
        dragAtTheCuff.venueId = world.theCuff;
        dragAtTheCuff.categorySlugs = std::vector<std::string>{ "drag" };
        dragAtTheCuff.sceneTagSlugs = std::vector<std::string>{ "bears" };
        EventSpec barAtSunset{ "bar", startsAtUs + kUsPerHour };
        barAtSunset.venueId = world.sunsetTavern;
        barAtSunset.categorySlugs = std::vector<std::string>{ "bar" };
        EventSpec t4tNight{ "t4t", startsAtUs + 2 * kUsPerHour };
        t4tNight.seriesSlug = "t4t";  // no own assignments: inherits dance + trans
        int64_t drag = AddEvent(transaction, databaseHelper, world, dragAtTheCuff);
        int64_t bar = AddEvent(transaction, databaseHelper, world, barAtSunset);
        int64_t t4t = AddEvent(transaction, databaseHelper, world, t4tNight);

        using Ids = std::set<int64_t>;
        EXPECT_EQ(IdSet(GetUpcoming(helper)), (Ids{ drag, bar, t4t }));
        EXPECT_EQ(IdSet(GetUpcoming(helper, "category=drag")), Ids{ drag });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "category=dance")), Ids{ t4t });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "scene=bears")), Ids{ drag });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "scene=trans")), Ids{ t4t });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "neighborhood=ballard")), Ids{ bar });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "venue_id=" + StringFromInt(world.theCuff))),
                  Ids{ drag });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "series_id=" + StringFromInt(world.t4t))), Ids{ t4t });
        EXPECT_EQ(IdSet(GetUpcoming(helper, "category=drag&neighborhood=ballard")), Ids{});
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetUpcomingEventsTest, ItemsCarryThePublicShapeOnly) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ItemsCarryThePublicShapeOnly", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventSpec spec{ "t4t", NowUs(transaction) + Events::kUsPerDay };
        spec.venueId = world.theCuff;
        spec.seriesSlug = "t4t";
        int64_t eventId = AddEvent(transaction, databaseHelper, world, spec);

        Json::Value body = GetUpcoming(helper);
        const Json::Value* item = FindById(body["items"].GetArray(), eventId);
        ASSERT_NE(item, nullptr);
        EXPECT_EQ((*item)["title"].Get<std::string>(), "Event t4t");
        EXPECT_EQ((*item)["venue_name"].Get<std::string>(), "The Cuff");
        EXPECT_EQ((*item)["neighborhood_slug"].Get<std::string>(), "capitol-hill");
        EXPECT_EQ((*item)["series_slug"].Get<std::string>(), "t4t");
        EXPECT_EQ((*item)["series_id"].Get<int64_t>(), world.t4t);
        const Json::JsonArray& categories = (*item)["category_slugs"].GetArray();
        ASSERT_EQ(categories.size(), 1u);
        EXPECT_EQ(categories[0].Get<std::string>(), "dance");
        EXPECT_EQ((*item)["scene_tag_slugs"].GetArray()[0].Get<std::string>(), "trans");
        // Nothing from the review workflow reaches a public list.
        for (const char* hidden : { "status", "review_notes", "reviewed_by_person_id",
                                    "reviewed_us", "source_id", "external_key" }) {
            EXPECT_FALSE(item->HasChild(hidden, static_cast<const Json::Value**>(nullptr)))
                << hidden;
        }
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
