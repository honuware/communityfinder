#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/jobs/scheduled_jobs.h"
#include "db_schema/permissions.h"
#include "endpoints/events_endpoint_test_support.h"
#include "endpoints/jobs_endpoint_test_support.h"

// EV4 §5/§9 — every job the helper calls is served by this app: its own two
// (archive, expire) and honuware's five (the cleanups and the digest), all
// behind the one run_scheduled_jobs gate. Each app job's own file tests its
// counts; honuware tests its five.
namespace Endpoints {
namespace {

using namespace JobsEndpointTest;

TEST(ScheduledJobRoutesTest, EveryJobIs401SignedOutAnd403WithoutThePermission) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EveryJobIs401SignedOutAnd403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        for (std::string_view job : Jobs::ScheduledJobNames()) {
            EXPECT_EQ(RunJob(helper, job).code, 401) << job;
        }

        // The permission exists (the scheduler holds it); this member doesn't.
        SignInAsScheduler(transaction, testDb, helper);
        EventsEndpointTest::SignIn(transaction, testDb, helper, "member@example.com", false);
        for (std::string_view job : Jobs::ScheduledJobNames()) {
            EXPECT_EQ(RunJob(helper, job).code, 403) << job;
        }
    });
    Auth::ServerConfig::Shutdown();
}

// Every route is registered and answers the service account, even with nothing
// to do.
TEST(ScheduledJobRoutesTest, EveryJobAnswersTheSchedulerOnAnEmptyDatabase) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EveryJobAnswersTheScheduler", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignInAsScheduler(transaction, testDb, helper);
        for (std::string_view job : Jobs::ScheduledJobNames()) {
            crow::response resp = RunJob(helper, job);
            EXPECT_EQ(resp.code, 200) << job << ": " << resp.body;
            EXPECT_EQ(resp.get_header_value("Content-Type"), "application/json") << job;
        }
    });
    Auth::ServerConfig::Shutdown();
}

// Administrator holds run_scheduled_jobs too, so a job can be run by hand.
TEST(ScheduledJobRoutesTest, AnyoneHoldingThePermissionCanRunAJobByHand) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnyoneHoldingThePermission", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        int64_t adminId = EventsEndpointTest::SignIn(transaction, testDb, helper, "admin@example.com", false);
        helper.GrantPermissionToPerson(transaction, adminId, DbSchema::kPermissionRunScheduledJobs);

        crow::response resp = RunJob(helper, Jobs::kJobArchivePastEvents);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(Json::Value::FromText(resp.body)["archived"].Get<int64_t>(), 0);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
