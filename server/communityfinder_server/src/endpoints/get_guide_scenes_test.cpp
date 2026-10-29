#include "get_guide_scenes.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/scene_tags.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.1–4.2 — GET /api/guide/scenes and /api/guide/scenes/<slug>, signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

TEST(GetGuideScenesTest, TheSceneGridListsEnabledScenesWithTheirCounts) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheSceneGridListsEnabledScenesWithTheirCounts", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/scenes");
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(resp.get_header_value("Content-Type"), "application/json");
        Json::Value body = Json::Value::FromText(resp.body);
        const Json::Value& items = body["items"];
        EXPECT_EQ(SlugsOf(items), (Slugs{ "leather", "bears" }));  // display order

        const Json::Value* leather = FindBySlug(items, "leather");
        ASSERT_NE(leather, nullptr);
        EXPECT_EQ((*leather)["display_name"].Get<std::string>(), "Name of leather");
        EXPECT_EQ((*leather)["coverage"].Get<std::string>(), DbSchema::kSceneTagsCoverageFull);
        // The closed venue doesn't count.
        EXPECT_EQ((*leather)["venue_count"].Get<int64_t>(), 1);
        EXPECT_EQ((*leather)["organization_count"].Get<int64_t>(), 1);
        EXPECT_EQ((*leather)["place_count"].Get<int64_t>(), 1);

        const Json::Value* bears = FindBySlug(items, "bears");
        ASSERT_NE(bears, nullptr);
        EXPECT_EQ((*bears)["coverage"].Get<std::string>(), DbSchema::kSceneTagsCoverageLinkOut);
        EXPECT_EQ((*bears)["venue_count"].Get<int64_t>(), 0);
        EXPECT_EQ((*bears)["organization_count"].Get<int64_t>(), 1);
        EXPECT_EQ((*bears)["place_count"].Get<int64_t>(), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuideScenesTest, ASceneLandingCarriesItsListingsLinksAndIntro) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASceneLandingCarriesItsListingsLinksAndIntro", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/scenes/leather");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["scene"]["slug"].Get<std::string>(), "leather");
        EXPECT_EQ(body["scene"]["venue_count"].Get<int64_t>(), 1);
        // The closed venue is in the graveyard, not here.
        ASSERT_EQ(SlugsOf(body["venues"]), Slugs{ "example-bar" });
        EXPECT_EQ(SlugsOf(body["organizations"]), Slugs{ "example-league" });
        EXPECT_EQ(SlugsOf(body["places"]), Slugs{ "example-beach" });
        ASSERT_EQ(body["links"].GetArray().size(), 1u);
        EXPECT_EQ(body["links"].GetArray()[0]["label"].Get<std::string>(), "scene-chat");

        const Json::Value& venue = body["venues"].GetArray()[0];
        EXPECT_EQ(venue["venue_id"].Get<int64_t>(), world.exampleBar);
        EXPECT_EQ(Strings(venue["scene_tag_slugs"]), Slugs{ "leather" });

        const Json::Value& intro = body["intro_page"];
        EXPECT_EQ(intro["id"].Get<int64_t>(), world.leatherIntro);
        EXPECT_EQ(intro["body_markdown"].Get<std::string>(), "Body of leather");
        EXPECT_EQ(intro["published"].Get<bool>(), true);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuideScenesTest, ASceneWithoutAnIntroHasANullOneAndUnknownScenesAre404) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASceneWithoutAnIntroHasANullOneAndUnknownScenesAre404", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/scenes/bears");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["scene"]["coverage"].Get<std::string>(), DbSchema::kSceneTagsCoverageLinkOut);
        EXPECT_EQ(SlugsOf(body["organizations"]), Slugs{ "example-chorus" });
        EXPECT_TRUE(body["venues"].GetArray().empty());
        EXPECT_TRUE(body["links"].GetArray().empty());
        EXPECT_TRUE(body["intro_page"].IsNull());

        EXPECT_EQ(Get(helper, "/api/guide/scenes/no-such-scene").code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
