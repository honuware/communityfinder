#include <iostream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "business_logic/app_secret_values.h"
#include "db_schema/make_database_info.h"
#include "db_schema/tenants.h"
#include "endpoints/endpoint_registrations.h"
#include "sql_util/schema/database_info.h"
#include "test/src/util/global_database_test_support.h"
#include "util/secrets/secrets_helper_test_util.h"

namespace {
// App-side test database BASE name. CommunityFinder owns its own name so the
// three suites' databases never collide on the shared server.
//
// Named kAppTestDatabaseName (not kTestDatabaseName) because honuware's global
// `kTestDatabaseName` (global_database_test_support.h) is unqualified and would
// be ambiguous with a same-named constant here. That global used to default to
// "test_knottyyoga" — the framework naming one app's database; as of honuware
// Phase 10.2b it is honuware's own "honuware_test", and every app names itself.
//
// The PHYSICAL name adds a compile-time platform token via
// ComposeTestDatabaseName — "test_communityfinder_windows" /
// "test_communityfinder_linux" — so a Linux gate and a Windows run of this repo
// no longer collide.
constexpr std::string_view kAppTestDatabaseName = "test_communityfinder";
}  // namespace

int main(int argc, char** argv)
{
    // Anchor the app endpoint registration translation unit into the test link (see
    // endpoints/endpoint_registrations.h). The reusable endpoint_test_helper
    // (honuware_testing) only calls RoutingBase::AddRoutes; pulling in the endpoint
    // TUs so their self-registering routes exist is an app concern, and this test
    // main is the app-side entry point that owns it.
    Endpoints::RegisterAllEndpoints();

    // Inject the app's default secret VALUES (brand defaults) into the framework
    // test secrets double. The harness (honuware_testing) is app-agnostic and loads
    // only Secrets::Values framework defaults itself; this app-side entry point
    // layers the app defaults on top — the same injection pattern as
    // MakeDatabaseInfo below.
    Secrets::Test::RegisterAppSecretDefaults(App::FillInAppSecretDefaultsString);

    // Inject the app's composed schema (framework + app tables). The harness is
    // app-agnostic and never calls MakeDatabaseInfo itself. The control-plane
    // `tenants` table is composed in on top so the tenancy table-helper / resolver
    // tests (which live in honuware_tests) run against the ordinary test database.
    const std::string testDatabaseName = ComposeTestDatabaseName(kAppTestDatabaseName);
    DbSchema::DatabaseInfo databaseInfo = DbSchema::MakeDatabaseInfo(testDatabaseName);
    DbSchema::MakeTenantsTable(databaseInfo);
    if(!GlobalDatabaseTestSupport::Initialize(databaseInfo)) {
        std::cout << "Failed to initialize GlobalDatabaseTestSupport." << std::endl;
        return -1;
    }
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    GlobalDatabaseTestSupport::Shutdown();
    return ret;
}
