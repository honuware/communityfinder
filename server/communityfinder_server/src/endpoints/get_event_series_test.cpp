#include "get_event_series.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/freshness.h"
#include "db_schema/series.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/series.h"

// EV1 §5.3 — GET /api/event_series and /api/event_series/<slug>, signed out.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

int64_t AddClosedSeries(Transaction& transaction, DatabaseHelper databaseHelper) {
    return TableHelpers::Series(databaseHelper).AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), "Closed Night" },
        { std::string(DbSchema::kSeriesSlug), "closed-night" },
        { std::string(DbSchema::kSeriesStatus), std::string(DbSchema::kFreshnessStatusClosed) },
    });
}

std::vector<std::string> Strings(const Json::Value& array) {
    std::vector<std::string> strings;
    for (const Json::Value& value : array.GetArray()) {
        strings.push_back(value.Get<std::string>());
    }
    return strings;
}

TEST(GetEventSeriesTest, TheIndexListsActiveSeriesWithTheirOwnSlugs) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheIndexListsActiveSeriesWithTheirOwnSlugs", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t closed = AddClosedSeries(transaction, databaseHelper);

        crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/event_series");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        const Json::JsonArray& items = body["items"].GetArray();
        EXPECT_EQ(FindById(items, closed), nullptr);
        const Json::Value* t4t = FindById(items, world.t4t);
        ASSERT_NE(t4t, nullptr);
        EXPECT_EQ((*t4t)["slug"].Get<std::string>(), "t4t");
        EXPECT_EQ((*t4t)["status"].Get<std::string>(), DbSchema::kFreshnessStatusOpen);
        EXPECT_EQ(Strings((*t4t)["category_slugs"]), std::vector<std::string>{ "dance" });
        EXPECT_EQ(Strings((*t4t)["scene_tag_slugs"]), std::vector<std::string>{ "trans" });
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetEventSeriesTest, ASeriesPageCarriesItsApprovedUpcomingInstances) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASeriesPageCarriesItsApprovedUpcomingInstances", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        EventSpec approvedSpec{ "t4t-1", tomorrow };
        approvedSpec.seriesSlug = "t4t";
        EventSpec pendingSpec{ "t4t-2", tomorrow, "pending" };
        pendingSpec.seriesSlug = "t4t";
        int64_t approved = AddEvent(transaction, databaseHelper, world, approvedSpec);
        AddEvent(transaction, databaseHelper, world, pendingSpec);
        AddEvent(transaction, databaseHelper, world, { "unrelated", tomorrow });

        crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/event_series/t4t");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["series"]["id"].Get<int64_t>(), world.t4t);
        EXPECT_EQ(body["series"]["title"].Get<std::string>(), "T4T");
        EXPECT_EQ(Strings(body["series"]["category_slugs"]), std::vector<std::string>{ "dance" });
        const Json::JsonArray& upcoming = body["upcoming"].GetArray();
        ASSERT_EQ(IdsOf(upcoming), std::vector<int64_t>{ approved });
        // Each instance carries the series' slugs it inherits.
        EXPECT_EQ(Strings(upcoming[0]["category_slugs"]), std::vector<std::string>{ "dance" });
        EXPECT_EQ(upcoming[0]["series_slug"].Get<std::string>(), "t4t");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetEventSeriesTest, AClosedSeriesKeepsItsPageAndUnknownSlugsAre404) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AClosedSeriesKeepsItsPageAndUnknownSlugsAre404", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        AddClosedSeries(transaction, databaseHelper);

        crow::response closed = Request(helper, crow::HTTPMethod::Get, "/api/event_series/closed-night");
        ASSERT_EQ(closed.code, 200) << closed.body;
        Json::Value body = Json::Value::FromText(closed.body);
        EXPECT_EQ(body["series"]["status"].Get<std::string>(), DbSchema::kFreshnessStatusClosed);
        EXPECT_TRUE(body["upcoming"].GetArray().empty());

        EXPECT_EQ(Request(helper, crow::HTTPMethod::Get, "/api/event_series/no-such-series").code,
                  404);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
