#include "admin_review_submission.h"

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/submission_helper.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/event_submissions.h"
#include "sql_util/table_helpers/events.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/types.h"

// EV3 §4.2 — POST /api/admin/review_submission/<int>: approve all / reject all.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

crow::response ReviewAll(EndpointTestHelper& helper, int64_t submissionId, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Post,
                   "/api/admin/review_submission/" + StringFromInt(submissionId), body);
}

std::string StatusOf(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId)
        .at(std::string(DbSchema::kEventsStatus));
}

std::set<int64_t> IdSet(const Json::JsonArray& ids) {
    std::set<int64_t> set;
    for (const Json::Value& id : ids) {
        set.insert(id.Get<int64_t>());
    }
    return set;
}

TEST(AdminReviewSubmissionTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        const std::string approve = R"({"action": "approve"})";
        // The permission is checked before the submission is looked up.
        EXPECT_EQ(ReviewAll(helper, 999999, approve).code, 401);

        int64_t submitterId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        SampleSubmission submission = AddSubmission(transaction, testDb, helper, submitterId, 1);
        ASSERT_EQ(submission.eventIds.size(), 1u);
        // The permission exists (a curator holds it); the submitter doesn't.
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SwitchTo(transaction, testDb, helper, "submitter@example.com");
        EXPECT_EQ(ReviewAll(helper, submission.submissionId, approve).code, 403);
        EXPECT_EQ(StatusOf(transaction, databaseHelper, submission.eventIds[0]), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

// Decided dates are left as they are; the rest are decided together, and the
// one email goes out.
TEST(AdminReviewSubmissionTest, ApproveAllDecidesWhatIsPendingAndEmailsOnce) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ApproveAllDecidesWhatIsPendingAndEmailsOnce", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t submitterId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        SampleSubmission submission = AddSubmission(transaction, testDb, helper, submitterId, 3);
        ASSERT_EQ(submission.eventIds.size(), 3u);
        int64_t curatorId = SignIn(transaction, testDb, helper, "curator@example.com", true);
        auto mail = helper.GetMailHelper();
        const size_t mailBefore = mail->GetMessages().size();

        crow::response one = Request(helper, crow::HTTPMethod::Post,
                                     "/api/admin/review_event/" + StringFromInt(submission.eventIds[0]),
                                     R"({"action": "reject"})");
        ASSERT_EQ(one.code, 200) << one.body;

        crow::response resp = ReviewAll(helper, submission.submissionId,
                                        R"({"action": "approve", "notes": "looks right"})");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["submission_id"].Get<int64_t>(), submission.submissionId);
        EXPECT_EQ(body["status"].Get<std::string>(), "approved");
        EXPECT_TRUE(body["decision_emailed"].Get<bool>());
        EXPECT_EQ(IdSet(body["decided_event_ids"].GetArray()),
                  (std::set<int64_t>{ submission.eventIds[1], submission.eventIds[2] }));

        EXPECT_EQ(StatusOf(transaction, databaseHelper, submission.eventIds[0]), "rejected");
        TableHelpers::Events events(databaseHelper);
        for (int64_t eventId : { submission.eventIds[1], submission.eventIds[2] }) {
            KeyValueTable row = events.GetEvent(transaction, eventId);
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "approved");
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedByPersonId)), StringFromInt(curatorId));
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewNotes)), "looks right");
        }
        ASSERT_EQ(mail->GetMessages().size(), mailBefore + 1);
        EXPECT_EQ(mail->GetMessages().back().GetTo()[0].address, "submitter@example.com");

        // Nothing is left to decide.
        crow::response again = ReviewAll(helper, submission.submissionId, R"({"action": "approve"})");
        ASSERT_EQ(again.code, 400) << again.body;
        EXPECT_EQ(Json::Value::FromText(again.body)["error_code"].Get<std::string>(),
                  Events::kSubmissionErrorNothingPending);
        EXPECT_EQ(mail->GetMessages().size(), mailBefore + 1);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewSubmissionTest, RejectAllStoresTheReasonAndMessage) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RejectAllStoresTheReasonAndMessage", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t submitterId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        SampleSubmission submission = AddSubmission(transaction, testDb, helper, submitterId, 2);
        ASSERT_EQ(submission.eventIds.size(), 2u);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        auto mail = helper.GetMailHelper();
        const size_t mailBefore = mail->GetMessages().size();

        crow::response resp = ReviewAll(helper, submission.submissionId, Json::Value(Json::JsonObject{
            { "action", Json::Value("reject") },
            { "reason", Json::Value(std::string(DbSchema::kEventSubmissionsRejectReasonDuplicate)) },
            { "message_to_submitter", Json::Value("It's already on the calendar.") },
        }).ToString());
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["status"].Get<std::string>(), "rejected");
        EXPECT_EQ(body["decided_event_ids"].GetArray().size(), 2u);
        for (int64_t eventId : submission.eventIds) {
            EXPECT_EQ(StatusOf(transaction, databaseHelper, eventId), "rejected");
        }

        KeyValueTable row = TableHelpers::EventSubmissions(databaseHelper)
                                .GetSubmission(transaction, submission.submissionId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)), "duplicate");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsMessageToSubmitter)),
                  "It's already on the calendar.");
        ASSERT_EQ(mail->GetMessages().size(), mailBefore + 1);
        EXPECT_EQ(mail->GetMessages().back().GetSubject(),
                  "Your event wasn't listed: Sample Dance Night");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewSubmissionTest, UnknownSubmissionsAndBadBodies) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownSubmissionsAndBadBodies", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t submitterId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        SampleSubmission submission = AddSubmission(transaction, testDb, helper, submitterId, 1);
        ASSERT_EQ(submission.eventIds.size(), 1u);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        EXPECT_EQ(ReviewAll(helper, 999999, R"({"action": "approve"})").code, 404);
        crow::response badReason = ReviewAll(helper, submission.submissionId,
                                             R"({"action": "approve", "reason": "spam"})");
        ASSERT_EQ(badReason.code, 400) << badReason.body;
        Json::Value body = Json::Value::FromText(badReason.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kSubmissionErrorBadReason);
        EXPECT_EQ(body["field"].Get<std::string>(), "reason");
        crow::response badAction = ReviewAll(helper, submission.submissionId, R"({"action": "list"})");
        ASSERT_EQ(badAction.code, 400) << badAction.body;
        EXPECT_EQ(Json::Value::FromText(badAction.body)["field"].Get<std::string>(), "action");
        EXPECT_EQ(ReviewAll(helper, submission.submissionId, "not json").code, 400);

        EXPECT_EQ(StatusOf(transaction, databaseHelper, submission.eventIds[0]), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
