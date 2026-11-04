#include "admin_expire_unreviewed_events.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/jobs/event_lifecycle_jobs.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "endpoints/events_endpoint_test_support.h"
#include "endpoints/jobs_endpoint_test_support.h"
#include "sql_util/table_helpers/event_submissions.h"
#include "sql_util/table_helpers/events.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/types.h"

// EV4 §5 — POST /api/admin/expire_unreviewed_events. The sweep's rules are
// tested in event_lifecycle_jobs_test; this is the endpoint around it, and the
// decision email it sends.
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

KeyValueTable EventRow(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
}

TEST(AdminExpireUnreviewedEventsTest, RejectsWhatStartedUnreviewedAndARepeatFindsNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RejectsWhatStartedUnreviewed", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        const int64_t nowUs = EventsEndpointTest::NowUs(transaction);
        const int64_t started = AddEvent(transaction, databaseHelper, "Started", "pending", nowUs - kUsPerHour);
        const int64_t later = AddEvent(transaction, databaseHelper, "Later", "pending", nowUs + kUsPerHour);
        const int64_t listed = AddEvent(transaction, databaseHelper, "Listed", "approved", nowUs - kUsPerHour);
        SignInAsScheduler(transaction, testDb, helper);

        crow::response first = RunJob(helper, "expire_unreviewed_events");
        ASSERT_EQ(first.code, 200) << first.body;
        Json::Value body = Json::Value::FromText(first.body);
        EXPECT_EQ(body["expired"].Get<int64_t>(), 1);
        EXPECT_EQ(body["decision_emails_sent"].Get<int64_t>(), 0);

        KeyValueTable row = EventRow(transaction, databaseHelper, started);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "rejected");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewNotes)), std::string(Jobs::kReviewNoteDatePassed));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedByPersonId)), "");
        EXPECT_EQ(EventRow(transaction, databaseHelper, later).at(std::string(DbSchema::kEventsStatus)), "pending");
        EXPECT_EQ(EventRow(transaction, databaseHelper, listed).at(std::string(DbSchema::kEventsStatus)), "approved");

        crow::response again = RunJob(helper, "expire_unreviewed_events");
        ASSERT_EQ(again.code, 200) << again.body;
        EXPECT_EQ(Json::Value::FromText(again.body)["expired"].Get<int64_t>(), 0);
    });
    Auth::ServerConfig::Shutdown();
}

// A submission whose only date passes unreviewed: the submitter hears, once,
// and the submission records why.
TEST(AdminExpireUnreviewedEventsTest, ASubmissionsLastDateExpiringSendsItsEmailOnce) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASubmissionsLastDateExpiring", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t submitterId =
            EventsEndpointTest::SignIn(transaction, testDb, helper, "submitter@example.com", false);
        EventsEndpointTest::SampleSubmission submission =
            EventsEndpointTest::AddSubmission(transaction, testDb, helper, submitterId, 1);
        ASSERT_EQ(submission.eventIds.size(), 1u);
        // Its date can only be made in the future; age it past its start.
        TableHelpers::Events(databaseHelper).UpdateEvent(transaction, submission.eventIds[0], {
            { std::string(DbSchema::kEventsStartsAtUs),
              StringFromInt(EventsEndpointTest::NowUs(transaction) - kUsPerHour) },
        });
        SignInAsScheduler(transaction, testDb, helper);
        auto mail = helper.GetMailHelper();
        const size_t mailBefore = mail->GetMessages().size();

        crow::response first = RunJob(helper, "expire_unreviewed_events");
        ASSERT_EQ(first.code, 200) << first.body;
        Json::Value body = Json::Value::FromText(first.body);
        EXPECT_EQ(body["expired"].Get<int64_t>(), 1);
        EXPECT_EQ(body["decision_emails_sent"].Get<int64_t>(), 1);
        ASSERT_EQ(mail->GetMessages().size(), mailBefore + 1);
        const ::Mail::MailMessage& message = mail->GetMessages().back();
        ASSERT_EQ(message.GetTo().size(), 1u);
        EXPECT_EQ(message.GetTo()[0].address, "submitter@example.com");
        KeyValueTable row = TableHelpers::EventSubmissions(databaseHelper)
                                .GetSubmission(transaction, submission.submissionId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)),
                  std::string(DbSchema::kEventSubmissionsRejectReasonExpired));
        EXPECT_NE(row.at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs)), "");

        crow::response again = RunJob(helper, "expire_unreviewed_events");
        ASSERT_EQ(again.code, 200) << again.body;
        EXPECT_EQ(Json::Value::FromText(again.body)["decision_emails_sent"].Get<int64_t>(), 0);
        EXPECT_EQ(mail->GetMessages().size(), mailBefore + 1);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
