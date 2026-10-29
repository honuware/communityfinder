#include "get_guide_graveyard.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/freshness.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.8 — GET /api/guide/graveyard, signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

TEST(GetGuideGraveyardTest, TheGraveyardIsTheClosedVenuesAndOrganizationsWithTheirNotes) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheGraveyardIsTheClosedVenuesAndOrganizationsWithTheirNotes", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/graveyard");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);

        ASSERT_EQ(SlugsOf(body["venues"]), Slugs{ "closed-bar" });
        const Json::Value& venue = body["venues"].GetArray()[0];
        EXPECT_EQ(venue["venue_id"].Get<int64_t>(), world.closedBar);
        EXPECT_EQ(venue["status"].Get<std::string>(), DbSchema::kFreshnessStatusClosed);
        EXPECT_EQ(venue["closure_note"].Get<std::string>(), "The venue's closure note.");
        EXPECT_EQ(venue["last_verified_at_us"].Get<std::string>(), "");

        ASSERT_EQ(SlugsOf(body["organizations"]), Slugs{ "closed-club" });
        const Json::Value& organization = body["organizations"].GetArray()[0];
        EXPECT_EQ(organization["id"].Get<int64_t>(), world.closedClub);
        EXPECT_EQ(organization["status"].Get<std::string>(), DbSchema::kFreshnessStatusClosed);
        EXPECT_EQ(organization["closure_note"].Get<std::string>(), "The club's closure note.");
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
