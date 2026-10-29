#include "get_guide_places.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/freshness.h"
#include "db_schema/places.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.5 — GET /api/guide/places and /api/guide/places/<slug>, signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

TEST(GetGuidePlacesTest, ThePlacesListCarriesTheAdultFlag) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ThePlacesListCarriesTheAdultFlag", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/places");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        ASSERT_EQ(SlugsOf(body["items"]), Slugs{ "example-beach" });
        const Json::Value& beach = body["items"].GetArray()[0];
        EXPECT_EQ(beach["id"].Get<int64_t>(), world.beach);
        EXPECT_EQ(beach["place_kind"].Get<std::string>(), DbSchema::kPlacesPlaceKindParkBeach);
        // 18+ places serve to everyone; the client gates rendering.
        EXPECT_EQ(beach["adult"].Get<bool>(), true);
        EXPECT_EQ(beach["status"].Get<std::string>(), DbSchema::kFreshnessStatusOpen);
        EXPECT_EQ(beach["last_verified_at_us"].Get<std::string>(), "");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuidePlacesTest, APlacePageCarriesItsLinksAndUnknownPlacesAre404) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("APlacePageCarriesItsLinksAndUnknownPlacesAre404", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/places/example-beach");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["place"]["id"].Get<int64_t>(), world.beach);
        EXPECT_EQ(body["place"]["adult"].Get<bool>(), true);
        EXPECT_EQ(Strings(body["place"]["scene_tag_slugs"]), Slugs{ "leather" });
        ASSERT_EQ(body["links"].GetArray().size(), 1u);
        EXPECT_EQ(body["links"].GetArray()[0]["label"].Get<std::string>(), "beach-group");

        EXPECT_EQ(Get(helper, "/api/guide/places/no-such-place").code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
