#include "admin_archive_past_events.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "db_schema/events.h"
#include "endpoints/events_endpoint_test_support.h"
#include "endpoints/jobs_endpoint_test_support.h"
#include "sql_util/table_helpers/events.h"
#include "util/types.h"

// EV4 §5 — POST /api/admin/archive_past_events. The sweep's rules are tested in
// event_lifecycle_jobs_test; this is the endpoint around it.
namespace Endpoints {
namespace {

using namespace JobsEndpointTest;

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

int64_t AddEvent(Transaction& transaction, DatabaseHelper databaseHelper, std::string_view title,
                 std::string_view status, int64_t startsAtUs) {
    return TableHelpers::Events(databaseHelper).AddEvent(transaction, {
        { std::string(DbSchema::kEventsTitle), std::string(title) },
        { std::string(DbSchema::kEventsStatus), std::string(status) },
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs) },
    });
}

std::string StatusOf(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId)
        .at(std::string(DbSchema::kEventsStatus));
}

TEST(AdminArchivePastEventsTest, ArchivesWhatEndedAndARepeatFindsNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ArchivesWhatEnded", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        const int64_t nowUs = EventsEndpointTest::NowUs(transaction);
        // No end: it ended two hours after its start, seven hours ago.
        const int64_t past = AddEvent(transaction, databaseHelper, "Past", "approved", nowUs - 9 * kUsPerHour);
        const int64_t tonight = AddEvent(transaction, databaseHelper, "Tonight", "approved", nowUs + kUsPerHour);
        const int64_t undecided = AddEvent(transaction, databaseHelper, "Undecided", "pending", nowUs - 9 * kUsPerHour);
        SignInAsScheduler(transaction, testDb, helper);

        crow::response first = RunJob(helper, "archive_past_events");
        ASSERT_EQ(first.code, 200) << first.body;
        EXPECT_EQ(Json::Value::FromText(first.body)["archived"].Get<int64_t>(), 1);
        EXPECT_EQ(StatusOf(transaction, databaseHelper, past), "archived");
        EXPECT_EQ(StatusOf(transaction, databaseHelper, tonight), "approved");
        // Expiry's job, not this one.
        EXPECT_EQ(StatusOf(transaction, databaseHelper, undecided), "pending");

        crow::response again = RunJob(helper, "archive_past_events");
        ASSERT_EQ(again.code, 200) << again.body;
        EXPECT_EQ(Json::Value::FromText(again.body)["archived"].Get<int64_t>(), 0);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
