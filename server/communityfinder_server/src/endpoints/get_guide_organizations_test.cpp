#include "get_guide_organizations.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/freshness.h"
#include "db_schema/organizations.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.4 — GET /api/guide/orgs and /api/guide/orgs/<slug>, signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

Slugs DirectorySlugs(EndpointTestHelper& helper, const std::string& query) {
    crow::response resp = Get(helper, "/api/guide/orgs", query);
    EXPECT_EQ(resp.code, 200) << query << ": " << resp.body;
    return SlugsOf(Json::Value::FromText(resp.body)["items"]);
}

TEST(GetGuideOrganizationsTest, TheDirectoryFiltersByKindSceneAndNeighborhood) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheDirectoryFiltersByKindSceneAndNeighborhood", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeWorld(transaction, testDb.GetDatabaseHelper());

        // By name; the closed club is in the graveyard instead.
        EXPECT_EQ(DirectorySlugs(helper, ""), (Slugs{ "example-chorus", "example-league" }));
        EXPECT_EQ(DirectorySlugs(helper, "kind=sports-league"), Slugs{ "example-league" });
        EXPECT_EQ(DirectorySlugs(helper, "kind=club-hobby"), Slugs{});
        EXPECT_EQ(DirectorySlugs(helper, "scene=bears"), Slugs{ "example-chorus" });
        EXPECT_EQ(DirectorySlugs(helper, "neighborhood=capitol-hill"), Slugs{ "example-league" });
        EXPECT_EQ(DirectorySlugs(helper, "kind=chorus-performance&neighborhood=capitol-hill"), Slugs{});
        EXPECT_EQ(DirectorySlugs(helper, "kind=no-such-kind"), Slugs{});
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuideOrganizationsTest, AnOrganizationPageServesAnyStatusWithItsLinks) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnOrganizationPageServesAnyStatusWithItsLinks", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response open = Get(helper, "/api/guide/orgs/example-league");
        ASSERT_EQ(open.code, 200) << open.body;
        Json::Value openBody = Json::Value::FromText(open.body);
        const Json::Value& league = openBody["organization"];
        EXPECT_EQ(league["id"].Get<int64_t>(), world.league);
        EXPECT_EQ(league["org_kind"].Get<std::string>(), DbSchema::kOrganizationsOrgKindSportsLeague);
        EXPECT_EQ(league["neighborhood_slug"].Get<std::string>(), "capitol-hill");
        EXPECT_EQ(Strings(league["scene_tag_slugs"]), Slugs{ "leather" });
        // Organizations carry the 18+ flag too (GD2 OQ11 (b)); the client gates.
        EXPECT_EQ(league["adult"].Get<bool>(), false);
        EXPECT_EQ(league["status"].Get<std::string>(), DbSchema::kFreshnessStatusOpen);
        EXPECT_EQ(league["last_verified_at_us"].Get<std::string>(), "");
        ASSERT_EQ(openBody["links"].GetArray().size(), 1u);
        EXPECT_EQ(openBody["links"].GetArray()[0]["label"].Get<std::string>(), "league-chat");

        crow::response closed = Get(helper, "/api/guide/orgs/closed-club");
        ASSERT_EQ(closed.code, 200) << closed.body;
        Json::Value closedBody = Json::Value::FromText(closed.body);
        EXPECT_EQ(closedBody["organization"]["status"].Get<std::string>(),
                  DbSchema::kFreshnessStatusClosed);
        EXPECT_EQ(closedBody["organization"]["closure_note"].Get<std::string>(),
                  "The club's closure note.");

        EXPECT_EQ(Get(helper, "/api/guide/orgs/no-such-org").code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
