#include "get_past_events.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

// EV4 §10.2 — GET /api/events/past over HTTP, signed out throughout.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

Json::Value GetPast(EndpointTestHelper& helper, const std::string& query = "") {
    crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/events/past", "", query);
    EXPECT_EQ(resp.code, 200) << query << ": " << resp.body;
    return Json::Value::FromText(resp.body);
}

std::vector<int64_t> Ids(const Json::Value& body) {
    return IdsOf(body["items"].GetArray());
}

// Newest first, a page at a time; never an upcoming, pending or rejected one.
TEST(GetPastEventsTest, NewestFirstAndPaged) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NewestFirstAndPaged", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        const int64_t nowUs = NowUs(transaction);
        auto daysAgo = [&](int64_t days) { return nowUs - days * Events::kUsPerDay; };
        int64_t threeDaysAgo = AddEvent(transaction, databaseHelper, world, { "three", daysAgo(3), "archived" });
        int64_t oneDayAgo = AddEvent(transaction, databaseHelper, world, { "one", daysAgo(1), "archived" });
        int64_t twoDaysAgo = AddEvent(transaction, databaseHelper, world, { "two", daysAgo(2), "archived" });
        // Ended but not swept yet: still approved, and still past.
        int64_t thisMorning = AddEvent(transaction, databaseHelper, world,
                                       { "this-morning", nowUs - 6 * 3'600LL * 1'000'000LL });
        AddEvent(transaction, databaseHelper, world, { "tomorrow", nowUs + Events::kUsPerDay });
        AddEvent(transaction, databaseHelper, world, { "pending", daysAgo(1), "pending" });
        AddEvent(transaction, databaseHelper, world, { "rejected", daysAgo(1), "rejected" });

        Json::Value first = GetPast(helper, "limit=3");
        EXPECT_EQ(Ids(first), (std::vector<int64_t>{ thisMorning, oneDayAgo, twoDaysAgo }));
        EXPECT_TRUE(first["has_more"].Get<bool>());
        Json::Value second = GetPast(helper, "limit=3&offset=3");
        EXPECT_EQ(Ids(second), (std::vector<int64_t>{ threeDaysAgo }));
        EXPECT_FALSE(second["has_more"].Get<bool>());
        EXPECT_TRUE(GetPast(helper, "offset=10")["items"].GetArray().empty());

        // The default page holds them all; the window is the default year.
        Json::Value all = GetPast(helper);
        EXPECT_EQ(Ids(all).size(), 4u);
        EXPECT_FALSE(all["has_more"].Get<bool>());
        EXPECT_GE(all["since_us"].Get<int64_t>(), daysAgo(365));
        EXPECT_LT(all["since_us"].Get<int64_t>(), daysAgo(364));
    });
    Auth::ServerConfig::Shutdown();
}

// Only the window's events: one that ended longer ago is hidden.
TEST(GetPastEventsTest, TheWindowEdge) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheWindowEdge", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kArchivedEventsVisibleDays, "30");
        const int64_t nowUs = NowUs(transaction);
        // No end, so each ends two hours after it starts.
        int64_t inside = AddEvent(transaction, databaseHelper, world,
                                  { "inside", nowUs - 30 * Events::kUsPerDay + 3 * 3'600LL * 1'000'000LL,
                                    "archived" });
        AddEvent(transaction, databaseHelper, world,
                 { "outside", nowUs - 30 * Events::kUsPerDay - 3 * 3'600LL * 1'000'000LL, "archived" });

        Json::Value body = GetPast(helper);
        EXPECT_EQ(Ids(body), std::vector<int64_t>{ inside });
        EXPECT_GE(body["since_us"].Get<int64_t>(), nowUs - 30 * Events::kUsPerDay);
    });
    Auth::ServerConfig::Shutdown();
}

// The upcoming list's filters, including the inherited category and the
// venue's neighborhood, and an oversized page is capped.
TEST(GetPastEventsTest, TheListsFiltersApply) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheListsFiltersApply", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        const int64_t lastWeek = NowUs(transaction) - 7 * Events::kUsPerDay;
        EventSpec atTheCuff{ "at-the-cuff", lastWeek, "archived" };
        atTheCuff.venueId = world.theCuff;
        int64_t cuff = AddEvent(transaction, databaseHelper, world, atTheCuff);
        EventSpec inTheSeries{ "t4t-night", lastWeek + 3'600LL * 1'000'000LL, "archived" };
        inTheSeries.seriesSlug = "t4t";
        inTheSeries.venueId = world.sunsetTavern;
        int64_t t4t = AddEvent(transaction, databaseHelper, world, inTheSeries);

        EXPECT_EQ(Ids(GetPast(helper, "category=dance")), std::vector<int64_t>{ t4t });
        EXPECT_EQ(Ids(GetPast(helper, "neighborhood=capitol-hill")), std::vector<int64_t>{ cuff });
        EXPECT_EQ(Ids(GetPast(helper, "venue_id=" + StringFromInt(world.sunsetTavern))),
                  std::vector<int64_t>{ t4t });
        EXPECT_EQ(Ids(GetPast(helper, "series_id=" + StringFromInt(world.t4t))),
                  std::vector<int64_t>{ t4t });
        EXPECT_TRUE(GetPast(helper, "scene=bears")["items"].GetArray().empty());
        // Paging counts matches, not candidates.
        Json::Value page = GetPast(helper, "category=dance&offset=1");
        EXPECT_TRUE(page["items"].GetArray().empty());
        EXPECT_FALSE(page["has_more"].Get<bool>());
        EXPECT_EQ(Ids(GetPast(helper, "limit=1000")).size(), 2u);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
