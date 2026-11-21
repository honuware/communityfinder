#include "admin_event_search.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "endpoints/events_endpoint_test_support.h"

// CM3 Phase 2 §9 — GET /api/admin/events/search: the curator's event finder.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr const char* kSearchUrl = "/api/admin/events/search";

crow::response Search(EndpointTestHelper& helper, const std::string& query = "") {
    return Request(helper, crow::HTTPMethod::Get, kSearchUrl, "", query);
}

Json::JsonArray ItemsOf(const crow::response& resp) {
    EXPECT_EQ(resp.code, 200) << resp.body;
    if (resp.code != 200) return {};
    return Json::Value::FromText(resp.body)["items"].GetArray();
}

TEST(AdminEventSearchTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        EXPECT_EQ(Search(helper).code, 401);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(Search(helper, "q=night").code, 403);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventSearchTest, SearchShowsEachDateWithItsVenueSeriesAndStatus) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SearchShowsEachDateWithItsVenueSeriesAndStatus", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        const int64_t now = NowUs(transaction);

        EventSpec tomorrow;
        tomorrow.externalKey = "board-1";
        tomorrow.title = "Board Game Night";
        tomorrow.startsAtUs = now + Events::kUsPerDay;
        tomorrow.venueId = world.theCuff;
        tomorrow.seriesSlug = "t4t";
        int64_t tomorrowId = AddEvent(transaction, databaseHelper, world, tomorrow);

        EventSpec nextWeek = tomorrow;
        nextWeek.externalKey = "board-2";
        nextWeek.startsAtUs = now + 7 * Events::kUsPerDay;
        nextWeek.status = "pending";
        int64_t nextWeekId = AddEvent(transaction, databaseHelper, world, nextWeek);

        EventSpec other;
        other.externalKey = "other";
        other.startsAtUs = now + Events::kUsPerDay;
        AddEvent(transaction, databaseHelper, world, other);

        Json::JsonArray items = ItemsOf(Search(helper, "q=board%20GAME"));
        EXPECT_EQ(IdsOf(items), std::vector<int64_t>({ tomorrowId, nextWeekId }));
        const Json::Value* first = FindById(items, tomorrowId);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ((*first)["title"].Get<std::string>(), "Board Game Night");
        EXPECT_EQ((*first)["starts_at_us"].Get<int64_t>(), tomorrow.startsAtUs);
        EXPECT_TRUE((*first)["ends_at_us"].IsNull());
        EXPECT_EQ((*first)["venue_name"].Get<std::string>(), "The Cuff");
        EXPECT_EQ((*first)["location_text"].Get<std::string>(), "");
        EXPECT_EQ((*first)["series_title"].Get<std::string>(), "T4T");
        EXPECT_EQ((*first)["status"].Get<std::string>(), "approved");
        const Json::Value* second = FindById(items, nextWeekId);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ((*second)["status"].Get<std::string>(), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventSearchTest, NoSearchListsWhatIsComingUpAndTheLimitIsClamped) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NoSearchListsWhatIsComingUpAndTheLimitIsClamped", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        const int64_t now = NowUs(transaction);

        // 55 coming up and one past: the past never shows without a search.
        std::vector<int64_t> comingUp;
        for (int i = 1; i <= 55; ++i) {
            EventSpec spec;
            spec.externalKey = "day-" + std::to_string(i);
            spec.startsAtUs = now + i * Events::kUsPerDay;
            comingUp.push_back(AddEvent(transaction, databaseHelper, world, spec));
        }
        EventSpec past;
        past.externalKey = "past";
        past.startsAtUs = now - Events::kUsPerDay;
        AddEvent(transaction, databaseHelper, world, past);

        Json::JsonArray defaults = ItemsOf(Search(helper));
        EXPECT_EQ(IdsOf(defaults),
                  std::vector<int64_t>(comingUp.begin(), comingUp.begin() + Events::kCuratorFinderDefaultLimit));
        auto countFor = [&](const std::string& query) {
            return static_cast<int64_t>(ItemsOf(Search(helper, query)).size());
        };
        EXPECT_EQ(countFor("q=%20%20"), Events::kCuratorFinderDefaultLimit);
        EXPECT_EQ(countFor("limit=3"), 3);
        EXPECT_EQ(countFor("limit=500"), Events::kCuratorFinderMaxLimit);
        EXPECT_EQ(countFor("limit=nope"), Events::kCuratorFinderDefaultLimit);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
