#include "get_guide_venues.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/freshness.h"
#include "db_schema/guide_links.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.3 — GET /api/guide/venues and /api/guide/venues/<slug>, signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

Slugs DirectorySlugs(EndpointTestHelper& helper, const std::string& query) {
    crow::response resp = Get(helper, "/api/guide/venues", query);
    EXPECT_EQ(resp.code, 200) << query << ": " << resp.body;
    return SlugsOf(Json::Value::FromText(resp.body)["items"]);
}

TEST(GetGuideVenuesTest, TheDirectoryFiltersBySceneNeighborhoodAndClosed) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheDirectoryFiltersBySceneNeighborhoodAndClosed", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeWorld(transaction, testDb.GetDatabaseHelper());

        EXPECT_EQ(DirectorySlugs(helper, ""), Slugs{ "example-bar" });
        // By name: Closed Bar, then Example Bar.
        EXPECT_EQ(DirectorySlugs(helper, "include_closed=true"), (Slugs{ "closed-bar", "example-bar" }));
        EXPECT_EQ(DirectorySlugs(helper, "include_closed=1"), (Slugs{ "closed-bar", "example-bar" }));
        EXPECT_EQ(DirectorySlugs(helper, "include_closed=false"), Slugs{ "example-bar" });
        EXPECT_EQ(DirectorySlugs(helper, "scene=leather"), Slugs{ "example-bar" });
        EXPECT_EQ(DirectorySlugs(helper, "scene=bears"), Slugs{});
        EXPECT_EQ(DirectorySlugs(helper, "neighborhood=ballard"), Slugs{});
        EXPECT_EQ(DirectorySlugs(helper, "neighborhood=ballard&include_closed=true"),
                  Slugs{ "closed-bar" });
        EXPECT_EQ(DirectorySlugs(helper, "scene=no-such-scene"), Slugs{});
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuideVenuesTest, AVenueItemCarriesTheJoinedShapeAndFreshness) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AVenueItemCarriesTheJoinedShapeAndFreshness", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/venues");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        const Json::Value* venue = FindBySlug(body["items"], "example-bar");
        ASSERT_NE(venue, nullptr);
        // The venue row's facts...
        EXPECT_EQ((*venue)["venue_id"].Get<int64_t>(), world.exampleBar);
        EXPECT_EQ((*venue)["name"].Get<std::string>(), "Example Bar");
        EXPECT_EQ((*venue)["neighborhood_slug"].Get<std::string>(), "capitol-hill");
        // ...the profile's...
        EXPECT_TRUE((*venue)["profile_id"].Is<int64_t>());
        EXPECT_EQ((*venue)["crowd_vibe"].Get<std::string>(), "The profile's crowd and vibe.");
        EXPECT_EQ((*venue)["price_band"].Get<std::string>(), "$$");
        EXPECT_EQ((*venue)["adult"].Get<bool>(), false);
        EXPECT_EQ(Strings((*venue)["scene_tag_slugs"]), Slugs{ "leather" });
        // ...and the freshness fields, never verified yet.
        EXPECT_EQ((*venue)["status"].Get<std::string>(), DbSchema::kFreshnessStatusOpen);
        EXPECT_EQ((*venue)["last_verified_at_us"].Get<std::string>(), "");
        EXPECT_EQ((*venue)["venue_last_verified_at_us"].Get<std::string>(), "");
        EXPECT_EQ((*venue)["profile_last_verified_at_us"].Get<std::string>(), "");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuideVenuesTest, AVenuePageServesAnyStatusWithItsLinks) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AVenuePageServesAnyStatusWithItsLinks", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response open = Get(helper, "/api/guide/venues/example-bar");
        ASSERT_EQ(open.code, 200) << open.body;
        Json::Value openBody = Json::Value::FromText(open.body);
        EXPECT_EQ(openBody["venue"]["venue_id"].Get<int64_t>(), world.exampleBar);
        ASSERT_EQ(openBody["links"].GetArray().size(), 1u);
        const Json::Value& link = openBody["links"].GetArray()[0];
        EXPECT_EQ(link["label"].Get<std::string>(), "bar-site");
        EXPECT_EQ(link["url"].Get<std::string>(), "https://example.org/bar-site");
        EXPECT_EQ(link["platform"].Get<std::string>(), DbSchema::kGuideLinksPlatformDiscord);

        // A closed venue keeps its page: its graveyard entry.
        crow::response closed = Get(helper, "/api/guide/venues/closed-bar");
        ASSERT_EQ(closed.code, 200) << closed.body;
        Json::Value closedBody = Json::Value::FromText(closed.body);
        EXPECT_EQ(closedBody["venue"]["status"].Get<std::string>(), DbSchema::kFreshnessStatusClosed);
        EXPECT_EQ(closedBody["venue"]["closure_note"].Get<std::string>(), "The venue's closure note.");
        EXPECT_TRUE(closedBody["links"].GetArray().empty());

        EXPECT_EQ(Get(helper, "/api/guide/venues/no-such-venue").code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
