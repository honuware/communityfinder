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
// App-side test database name. Deliberately NOT honuware's lingering
// kTestDatabaseName default ("test_knottyyoga") — CommunityFinder owns its own
// test database name so the two suites' databases never collide on the shared
// server.
// Named kAppTestDatabaseName (not kTestDatabaseName) because honuware's global
// default `kTestDatabaseName = "test_knottyyoga"` (global_database_test_support.h)
// is unqualified and would be ambiguous with a same-named constant here. Removing
// that legacy global default is honuware hygiene item 0.4.
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
    DbSchema::DatabaseInfo databaseInfo = DbSchema::MakeDatabaseInfo(kAppTestDatabaseName);
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
