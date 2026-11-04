#include "jobs_endpoint_test_support.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_service_account_config.h"
#include "business_logic/create_app_tables.h"
#include "db_schema/people.h"
#include "db_schema/permissions.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/people.h"
#include "sql_util/table_helpers/permissions.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper_test_util.h"

namespace Endpoints::JobsEndpointTest {

int64_t SignInAsScheduler(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper) {
    DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
    auto secrets = helper.GetSecretsHelper();
    // Without a session length, a fresh token reads as expired.
    if (secrets->LookupSecret(transaction, Secrets::kAuthSessionMaxDuractioninMicros).empty()) {
        secrets->AddSecret(transaction, Secrets::kAuthSessionMaxDuractioninMicros,
                           std::to_string(30LL * 60LL * 1000000LL));
    }

    TableHelpers::Permissions(databaseHelper).AddPermission(
        transaction, DbSchema::kPermissionRunScheduledJobs, "Run the scheduled jobs");
    EXPECT_TRUE(ProvisionSchedulerAccount(transaction, databaseHelper, "Scheduler-test-password-1"));
    EventsEndpointTest::SwitchTo(transaction, testDb, helper, App::kSchedulerServiceAccountEmail);
    return std::stoll(TableHelpers::People(databaseHelper)
                          .LookupPersonByEmail(transaction, App::kSchedulerServiceAccountEmail)
                          .at(std::string(DbSchema::kPeopleId)));
}

crow::response RunJob(EndpointTestHelper& helper, std::string_view job) {
    return EventsEndpointTest::Request(helper, crow::HTTPMethod::Post, "/api/admin/" + std::string(job));
}

}  // namespace Endpoints::JobsEndpointTest
