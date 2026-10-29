#include "get_guide_neighborhoods.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/neighborhoods.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.7 — GET /api/guide/neighborhoods and /api/guide/neighborhoods/<slug>,
// signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

TEST(GetGuideNeighborhoodsTest, TheIndexCountsEachNeighborhoodsActiveListings) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheIndexCountsEachNeighborhoodsActiveListings", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/neighborhoods");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);

        const Json::Value* capitolHill = FindBySlug(body["items"], "capitol-hill");
        ASSERT_NE(capitolHill, nullptr);
        EXPECT_EQ((*capitolHill)["display_name"].Get<std::string>(), "Name of capitol-hill");
        EXPECT_EQ((*capitolHill)["region"].Get<std::string>(),
                  DbSchema::kNeighborhoodsRegionSeattleCore);
        EXPECT_EQ((*capitolHill)["venue_count"].Get<int64_t>(), 1);
        EXPECT_EQ((*capitolHill)["organization_count"].Get<int64_t>(), 1);
        EXPECT_EQ((*capitolHill)["place_count"].Get<int64_t>(), 1);

        // The closed venue doesn't count.
        const Json::Value* ballard = FindBySlug(body["items"], "ballard");
        ASSERT_NE(ballard, nullptr);
        EXPECT_EQ((*ballard)["venue_count"].Get<int64_t>(), 0);
        EXPECT_EQ((*ballard)["organization_count"].Get<int64_t>(), 1);
        EXPECT_EQ((*ballard)["place_count"].Get<int64_t>(), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuideNeighborhoodsTest, ANeighborhoodPageListsItsListingsAndRundown) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ANeighborhoodPageListsItsListingsAndRundown", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/neighborhoods/capitol-hill");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["neighborhood"]["slug"].Get<std::string>(), "capitol-hill");
        EXPECT_EQ(SlugsOf(body["venues"]), Slugs{ "example-bar" });
        EXPECT_EQ(SlugsOf(body["organizations"]), Slugs{ "example-league" });
        EXPECT_EQ(SlugsOf(body["places"]), Slugs{ "example-beach" });
        EXPECT_EQ(body["rundown_page"]["id"].Get<int64_t>(), world.capitolHillRundown);
        EXPECT_EQ(body["rundown_page"]["section"].Get<std::string>(), "new-to-seattle");

        crow::response ballard = Get(helper, "/api/guide/neighborhoods/ballard");
        ASSERT_EQ(ballard.code, 200) << ballard.body;
        Json::Value ballardBody = Json::Value::FromText(ballard.body);
        EXPECT_TRUE(ballardBody["venues"].GetArray().empty());  // only the closed bar is there
        EXPECT_EQ(SlugsOf(ballardBody["organizations"]), Slugs{ "example-chorus" });
        EXPECT_TRUE(ballardBody["rundown_page"].IsNull());

        EXPECT_EQ(Get(helper, "/api/guide/neighborhoods/no-such-neighborhood").code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
