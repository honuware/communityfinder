#include "get_guide_services.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/service_listings.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.6 — GET /api/guide/services, signed out.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

TEST(GetGuideServicesTest, ServicesListByKindWithTheHealthFlag) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ServicesListByKindWithTheHealthFlag", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/services");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(SlugsOf(body["items"]), (Slugs{ "example-clinic", "example-dry-cleaner" }));

        const Json::Value* clinic = FindBySlug(body["items"], "example-clinic");
        ASSERT_NE(clinic, nullptr);
        EXPECT_EQ((*clinic)["id"].Get<int64_t>(), world.clinic);
        EXPECT_EQ((*clinic)["service_kind"].Get<std::string>(),
                  DbSchema::kServiceListingsServiceKindStiTesting);
        EXPECT_EQ((*clinic)["is_health"].Get<bool>(), true);
        // An all-digit phone number stays a string.
        EXPECT_EQ((*clinic)["phone"].Get<std::string>(), "2065550100");
        EXPECT_EQ((*clinic)["last_verified_at_us"].Get<std::string>(), "");

        const Json::Value* dryCleaner = FindBySlug(body["items"], "example-dry-cleaner");
        ASSERT_NE(dryCleaner, nullptr);
        EXPECT_EQ((*dryCleaner)["is_health"].Get<bool>(), false);

        crow::response byKind = Get(helper, "/api/guide/services", "kind=sti-testing");
        ASSERT_EQ(byKind.code, 200) << byKind.body;
        EXPECT_EQ(SlugsOf(Json::Value::FromText(byKind.body)["items"]), Slugs{ "example-clinic" });

        crow::response none = Get(helper, "/api/guide/services", "kind=dental");
        ASSERT_EQ(none.code, 200) << none.body;
        EXPECT_TRUE(Json::Value::FromText(none.body)["items"].GetArray().empty());
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
