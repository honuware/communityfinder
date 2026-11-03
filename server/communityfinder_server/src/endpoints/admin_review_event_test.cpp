#include "admin_review_event.h"

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/submission_helper.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/event_submissions.h"
#include "sql_util/table_helpers/events.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/types.h"

// EV1 §5.5 — POST /api/admin/review_event/<int>: approve / reject. EV3 §4.2 —
// plus a submitted date's reason, message and decision email.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

crow::response Review(EndpointTestHelper& helper, int64_t eventId, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Post,
                   "/api/admin/review_event/" + StringFromInt(eventId), body);
}

std::set<int64_t> UpcomingIds(EndpointTestHelper& helper) {
    crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/events/upcoming");
    EXPECT_EQ(resp.code, 200) << resp.body;
    std::vector<int64_t> ids = IdsOf(Json::Value::FromText(resp.body)["items"].GetArray());
    return std::set<int64_t>(ids.begin(), ids.end());
}

TEST(AdminReviewEventTest, ApprovingPublishesAndStampsTheReviewer) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ApprovingPublishesAndStampsTheReviewer", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        int64_t curatorId = SignIn(transaction, testDb, helper, "curator@example.com", true);
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 0u);

        crow::response resp = Review(helper, eventId, R"({"action": "approve", "notes": "checked"})");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["event_id"].Get<int64_t>(), eventId);
        EXPECT_EQ(body["status"].Get<std::string>(), "approved");

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedByPersonId)), StringFromInt(curatorId));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewNotes)), "checked");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventsReviewedUs))), 0);
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 1u);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, RejectingHidesAnApprovedEvent) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RejectingHidesAnApprovedEvent", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay });
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 1u);

        crow::response resp = Review(helper, eventId, R"({"action": "reject"})");
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(Json::Value::FromText(resp.body)["status"].Get<std::string>(), "rejected");
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 0u);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, ARepeatIsAnInvalidTransition) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ARepeatIsAnInvalidTransition", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay });
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        crow::response resp = Review(helper, eventId, R"({"action": "approve"})");
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kEventErrorInvalidTransition);
        EXPECT_EQ(body["status"].Get<std::string>(), "approved");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, UnknownEventsAndBadBodies) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownEventsAndBadBodies", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        EXPECT_EQ(Review(helper, 999999, R"({"action": "approve"})").code, 404);
        crow::response badAction = Review(helper, eventId, R"({"action": "publish"})");
        ASSERT_EQ(badAction.code, 400) << badAction.body;
        EXPECT_EQ(Json::Value::FromText(badAction.body)["field"].Get<std::string>(), "action");
        crow::response badNotes = Review(helper, eventId, R"({"action": "approve", "notes": 3})");
        ASSERT_EQ(badNotes.code, 400) << badNotes.body;
        EXPECT_EQ(Json::Value::FromText(badNotes.body)["field"].Get<std::string>(), "notes");
        EXPECT_EQ(Review(helper, eventId, "not json").code, 400);

        // None of those touched the event.
        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        const std::string approve = R"({"action": "approve"})";

        EXPECT_EQ(Review(helper, eventId, approve).code, 401);
        // The permission exists (a curator holds it); this member doesn't.
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(Review(helper, eventId, approve).code, 403);

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

// EV3 §4.2 — a submitted date: the reason and message land on its submission,
// and the decision email goes out once, with the last pending date.
TEST(AdminReviewEventTest, ASubmittedDatesDecisionIsStoredAndEmailedOnce) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASubmittedDatesDecisionIsStoredAndEmailedOnce", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t submitterId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        SampleSubmission submission = AddSubmission(transaction, testDb, helper, submitterId, 2);
        ASSERT_EQ(submission.eventIds.size(), 2u);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        auto mail = helper.GetMailHelper();
        const size_t mailBefore = mail->GetMessages().size();

        crow::response first = Review(helper, submission.eventIds[0], Json::Value(Json::JsonObject{
            { "action", Json::Value("reject") },
            { "reason", Json::Value(std::string(DbSchema::kEventSubmissionsRejectReasonNotAFit)) },
            { "message_to_submitter", Json::Value("Thanks for sending it.") },
        }).ToString());
        ASSERT_EQ(first.code, 200) << first.body;
        EXPECT_FALSE(Json::Value::FromText(first.body)["decision_emailed"].Get<bool>());
        EXPECT_EQ(mail->GetMessages().size(), mailBefore);

        TableHelpers::EventSubmissions submissions(databaseHelper);
        KeyValueTable row = submissions.GetSubmission(transaction, submission.submissionId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)), "not-a-fit");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsMessageToSubmitter)),
                  "Thanks for sending it.");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs)), "");

        crow::response last = Review(helper, submission.eventIds[1], R"({"action": "approve"})");
        ASSERT_EQ(last.code, 200) << last.body;
        EXPECT_TRUE(Json::Value::FromText(last.body)["decision_emailed"].Get<bool>());
        ASSERT_EQ(mail->GetMessages().size(), mailBefore + 1);
        const ::Mail::MailMessage& message = mail->GetMessages().back();
        ASSERT_EQ(message.GetTo().size(), 1u);
        EXPECT_EQ(message.GetTo()[0].address, "submitter@example.com");
        EXPECT_EQ(message.GetSubject(), "Your event has been reviewed: Sample Dance Night");
        row = submissions.GetSubmission(transaction, submission.submissionId);
        EXPECT_NE(row.at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs)), "");
        // An approval states no reason, so the first one stands.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)), "not-a-fit");

        // Re-deciding a date later sends nothing more.
        crow::response again = Review(helper, submission.eventIds[1], R"({"action": "reject"})");
        ASSERT_EQ(again.code, 200) << again.body;
        EXPECT_FALSE(Json::Value::FromText(again.body)["decision_emailed"].Get<bool>());
        EXPECT_EQ(mail->GetMessages().size(), mailBefore + 1);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, ABadDecisionIs400AndChangesNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ABadDecisionIs400AndChangesNothing", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t submitterId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        SampleSubmission submission = AddSubmission(transaction, testDb, helper, submitterId, 1);
        ASSERT_EQ(submission.eventIds.size(), 1u);
        const int64_t eventId = submission.eventIds[0];
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        struct Case {
            std::string name;
            std::string body;
            std::string errorCode;
            std::string field;
        };
        const Case cases[] = {
            { "unknown reason", R"({"action": "reject", "reason": "boring"})",
              std::string(Events::kSubmissionErrorBadReason), "reason" },
            { "reason on an approval", R"({"action": "approve", "reason": "spam"})",
              std::string(Events::kSubmissionErrorBadReason), "reason" },
            { "message too long",
              Json::Value(Json::JsonObject{
                  { "action", Json::Value("reject") },
                  { "message_to_submitter", Json::Value(std::string(2001, 'a')) },
              }).ToString(),
              std::string(Events::kSubmissionErrorFieldTooLong), "message_to_submitter" },
        };
        for (const Case& testCase : cases) {
            crow::response resp = Review(helper, eventId, testCase.body);
            ASSERT_EQ(resp.code, 400) << testCase.name << ": " << resp.body;
            Json::Value body = Json::Value::FromText(resp.body);
            EXPECT_EQ(body["error_code"].Get<std::string>(), testCase.errorCode) << testCase.name;
            EXPECT_EQ(body["field"].Get<std::string>(), testCase.field) << testCase.name;
        }
        crow::response notAString = Review(helper, eventId, R"({"action": "reject", "reason": 5})");
        ASSERT_EQ(notAString.code, 400) << notAString.body;
        EXPECT_EQ(Json::Value::FromText(notAString.body)["field"].Get<std::string>(), "reason");

        EXPECT_EQ(TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId)
                      .at(std::string(DbSchema::kEventsStatus)), "pending");
        KeyValueTable row = TableHelpers::EventSubmissions(databaseHelper)
                                .GetSubmission(transaction, submission.submissionId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)), "");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsMessageToSubmitter)), "");
    });
    Auth::ServerConfig::Shutdown();
}

// A feed's event has no submission: a reason is accepted and goes nowhere, and
// there is no one to email.
TEST(AdminReviewEventTest, AReasonOnAnEventFromElsewhereIsIgnored) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AReasonOnAnEventFromElsewhereIsIgnored", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        auto mail = helper.GetMailHelper();
        const size_t mailBefore = mail->GetMessages().size();

        crow::response resp = Review(helper, eventId,
                                     R"({"action": "reject", "reason": "spam", "message_to_submitter": "Hi"})");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["status"].Get<std::string>(), "rejected");
        EXPECT_FALSE(body["decision_emailed"].Get<bool>());
        EXPECT_EQ(mail->GetMessages().size(), mailBefore);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
