#pragma once

#include <cstdint>
#include <string_view>

#include <crow.h>

#include "endpoints/endpoint_test_helper.h"
#include "test/src/util/database_test_helper.h"

// Shared setup for the EV4 scheduled-job endpoint tests (admin_archive_past_
// events_test, admin_expire_unreviewed_events_test and
// scheduled_job_routes_test). Plain helpers each test calls at its top — not a
// fixture.
namespace Endpoints::JobsEndpointTest {

// The scheduler service account, provisioned the way the seed does it
// (ProvisionSchedulerAccount: its own role, holding only run_scheduled_jobs),
// then signed in on the helper's cookie manager, so later requests are the
// helper's. The harness doesn't run the framework seed, so the permission row
// is added first. At most once per test. Returns its person id.
int64_t SignInAsScheduler(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper);

// POST /api/admin/<job> with an empty body, as the scheduler sends it.
crow::response RunJob(EndpointTestHelper& helper, std::string_view job);

}  // namespace Endpoints::JobsEndpointTest
