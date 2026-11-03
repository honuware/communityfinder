#include "event_submissions.h"

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
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

// EV3 §4.1 — the submitter's endpoints: submit, mine, withdraw.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr int64_t kUsPerWeek = 7 * Events::kUsPerDay;
constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

// A form that passes: `dateCount` weekly dates from two days out.
Json::JsonObject Form(int64_t nowUs, int dateCount = 1) {
    Json::JsonArray dates;
    for (int index = 0; index < dateCount; ++index) {
        dates.push_back(Json::Value(Json::JsonObject{
            { "starts_at_us", Json::Value(nowUs + 2 * Events::kUsPerDay + index * kUsPerWeek) },
        }));
    }
    return Json::JsonObject{
        { "title", Json::Value("Sample Dance Night") },
        { "dates", Json::Value(dates) },
        { "agreed", Json::Value(true) },
    };
}

crow::response Submit(EndpointTestHelper& helper, const Json::JsonObject& form) {
    return Request(helper, crow::HTTPMethod::Post, "/api/event_submissions",
                   Json::Value(form).ToString());
}

crow::response Mine(EndpointTestHelper& helper) {
    return Request(helper, crow::HTTPMethod::Get, "/api/event_submissions/mine");
}

crow::response Withdraw(EndpointTestHelper& helper, int64_t submissionId) {
    return Request(helper, crow::HTTPMethod::Post,
                   "/api/event_submissions/" + StringFromInt(submissionId) + "/withdraw");
}

std::string ErrorCodeOf(const crow::response& resp) {
    return Json::Value::FromText(resp.body)["error_code"].Get<std::string>();
}

std::string FieldOf(const crow::response& resp) {
    return Json::Value::FromText(resp.body)["field"].Get<std::string>();
}

TEST(EventSubmissionsEndpointTest, SignedOutIs401) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        EXPECT_EQ(Submit(helper, Form(NowUs(transaction))).code, 401);
        EXPECT_EQ(Mine(helper).code, 401);
        EXPECT_EQ(Withdraw(helper, 1).code, 401);
    });
    Auth::ServerConfig::Shutdown();
}

// Sign-in already needs a confirmed email; this is the belt to that's braces.
TEST(EventSubmissionsEndpointTest, AnUnconfirmedEmailIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnUnconfirmedEmailIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        int64_t personId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        transaction.RunSqlStatement("UPDATE people SET email_verified_at = NULL WHERE id = $1",
                                    StringFromInt(personId));

        crow::response resp = Submit(helper, Form(NowUs(transaction)));
        ASSERT_EQ(resp.code, 403) << resp.body;
        EXPECT_EQ(ErrorCodeOf(resp), Events::kSubmissionErrorEmailNotVerified);
        EXPECT_EQ(TableHelpers::EventSubmissions(testDb.GetDatabaseHelper())
                      .CountSubmissionsSince(transaction, personId, 0), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(EventSubmissionsEndpointTest, SubmitWritesTheDatesAndAnswersWithTheIds) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SubmitWritesTheDatesAndAnswersWithTheIds", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t personId = SignIn(transaction, testDb, helper, "submitter@example.com", false);

        Json::JsonObject form = Form(NowUs(transaction), 2);
        form["venue_id"] = Json::Value(world.sunsetTavern);
        form["url"] = Json::Value("https://example.org/sample");
        form["category_slugs"] = Json::Value(Json::JsonArray{ Json::Value("dance"), Json::Value("nope") });
        form["relationship"] = Json::Value("organizer");
        form["regular_thing"] = Json::Value(true);
        form["submitter_note"] = Json::Value("Every other week.");
        crow::response resp = Submit(helper, form);
        ASSERT_EQ(resp.code, 200) << resp.body;

        Json::Value body = Json::Value::FromText(resp.body);
        const int64_t submissionId = body["submission_id"].Get<int64_t>();
        EXPECT_GT(submissionId, 0);
        const Json::JsonArray& eventIds = body["event_ids"].GetArray();
        ASSERT_EQ(eventIds.size(), 2u);
        ASSERT_EQ(body["ignored_category_slugs"].GetArray().size(), 1u);
        EXPECT_EQ(body["ignored_category_slugs"].GetArray()[0].Get<std::string>(), "nope");
        EXPECT_TRUE(body["ignored_scene_tag_slugs"].GetArray().empty());
        EXPECT_TRUE(body["possible_duplicates"].GetArray().empty());

        TableHelpers::Events events(databaseHelper);
        for (const Json::Value& eventId : eventIds) {
            KeyValueTable row = events.GetEvent(transaction, eventId.Get<int64_t>());
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "pending");
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsOrigin)), "user_submitted");
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsCreatedByPersonId)), StringFromInt(personId));
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsSubmissionId)), StringFromInt(submissionId));
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsVenueId)), StringFromInt(world.sunsetTavern));
        }
        KeyValueTable submission = TableHelpers::EventSubmissions(databaseHelper)
                                       .GetSubmission(transaction, submissionId);
        EXPECT_EQ(submission.at(std::string(DbSchema::kEventSubmissionsRelationship)), "organizer");
        EXPECT_EQ(submission.at(std::string(DbSchema::kEventSubmissionsRegularThing)), "t");
        EXPECT_EQ(submission.at(std::string(DbSchema::kEventSubmissionsSubmitterNote)), "Every other week.");
    });
    Auth::ServerConfig::Shutdown();
}

// One case per validation family, each a 400 that names its code and field.
TEST(EventSubmissionsEndpointTest, EachValidationFamilyIs400WithItsCode) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachValidationFamilyIs400WithItsCode", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        int64_t personId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        const int64_t nowUs = NowUs(transaction);

        struct Case {
            std::string name;
            Json::JsonObject form;
            std::string errorCode;
            std::string field;
        };
        std::vector<Case> cases;
        auto add = [&](std::string name, std::string key, Json::Value value,
                       std::string_view errorCode, std::string field) {
            Json::JsonObject form = Form(nowUs);
            form[key] = std::move(value);
            cases.push_back({ std::move(name), std::move(form), std::string(errorCode), std::move(field) });
        };
        add("title", "title", Json::Value("ab"), Events::kSubmissionErrorTitleLength, "title");
        add("not agreed", "agreed", Json::Value(false), Events::kSubmissionErrorNotAgreed, "agreed");
        add("no dates", "dates", Json::Value(Json::JsonArray{}), Events::kSubmissionErrorNoDates, "dates");
        add("link", "url", Json::Value("ftp://example.org"), Events::kSubmissionErrorBadUrl, "url");
        add("length", "description", Json::Value(std::string(4001, 'a')),
            Events::kSubmissionErrorFieldTooLong, "description");
        add("venue", "venue_id", Json::Value(int64_t{ 999999 }), Events::kEventErrorVenueNotFound, "venue_id");
        add("relationship", "relationship", Json::Value("fan"),
            Events::kSubmissionErrorBadRelationship, "relationship");

        for (const Case& testCase : cases) {
            crow::response resp = Submit(helper, testCase.form);
            ASSERT_EQ(resp.code, 400) << testCase.name << ": " << resp.body;
            EXPECT_EQ(ErrorCodeOf(resp), testCase.errorCode) << testCase.name;
            EXPECT_EQ(FieldOf(resp), testCase.field) << testCase.name;
        }

        // A date's error says which date.
        Json::JsonObject soon = Form(nowUs, 2);
        Json::JsonArray dates = soon["dates"].GetArray();
        dates[1] = Json::Value(Json::JsonObject{ { "starts_at_us", Json::Value(nowUs + kUsPerHour / 2) } });
        soon["dates"] = Json::Value(dates);
        crow::response resp = Submit(helper, soon);
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kSubmissionErrorStartTooSoon);
        EXPECT_EQ(body["date_index"].Get<int64_t>(), 1);

        // Wrong types are 400s that name the field, before the helper sees them.
        Json::JsonObject badTitle = Form(nowUs);
        badTitle["title"] = Json::Value(int64_t{ 5 });
        resp = Submit(helper, badTitle);
        ASSERT_EQ(resp.code, 400);
        EXPECT_EQ(FieldOf(resp), "title");
        Json::JsonObject badDates = Form(nowUs);
        badDates["dates"] = Json::Value(Json::JsonArray{ Json::Value(Json::JsonObject{ { "starts_at_us", Json::Value("soon") } }) });
        resp = Submit(helper, badDates);
        ASSERT_EQ(resp.code, 400);
        EXPECT_EQ(FieldOf(resp), "dates");
        EXPECT_EQ(Request(helper, crow::HTTPMethod::Post, "/api/event_submissions", "not json").code, 400);

        EXPECT_EQ(TableHelpers::EventSubmissions(testDb.GetDatabaseHelper())
                      .CountSubmissionsSince(transaction, personId, 0), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(EventSubmissionsEndpointTest, EachRateLimitIs429) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachRateLimitIs429", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "submitter@example.com", false);
        const int64_t nowUs = NowUs(transaction);

        // Eleven dates at once is past the default of 10 waiting.
        crow::response pending = Submit(helper, Form(nowUs, 11));
        ASSERT_EQ(pending.code, 429) << pending.body;
        Json::Value body = Json::Value::FromText(pending.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kSubmissionErrorRateLimited);
        EXPECT_EQ(body["field"].Get<std::string>(), Events::kSubmissionLimitMaxPendingDates);
        EXPECT_NE(body["detail"].Get<std::string>().find("up to 10 dates"), std::string::npos)
            << pending.body;

        // Five a day at the default; the sixth is refused.
        for (int index = 0; index < 5; ++index) {
            Json::JsonObject form = Form(nowUs + index * kUsPerHour);
            ASSERT_EQ(Submit(helper, form).code, 200) << index;
        }
        crow::response daily = Submit(helper, Form(nowUs + 5 * kUsPerHour));
        ASSERT_EQ(daily.code, 429) << daily.body;
        body = Json::Value::FromText(daily.body);
        EXPECT_EQ(body["field"].Get<std::string>(), Events::kSubmissionLimitMaxSubmissionsPerDay);
        EXPECT_NE(body["detail"].Get<std::string>().find("up to 5 submissions"), std::string::npos)
            << daily.body;
    });
    Auth::ServerConfig::Shutdown();
}

TEST(EventSubmissionsEndpointTest, PossibleDuplicatesComeBack) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PossibleDuplicatesComeBack", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        SignIn(transaction, testDb, helper, "submitter@example.com", false);
        Json::JsonObject form = Form(NowUs(transaction));
        const int64_t startsAtUs =
            form["dates"].GetArray()[0]["starts_at_us"].Get<int64_t>();
        int64_t existing = AddEvent(transaction, databaseHelper, world,
                                    { "listed", startsAtUs + kUsPerHour, "approved", world.sunsetTavern });
        form["venue_id"] = Json::Value(world.sunsetTavern);

        crow::response resp = Submit(helper, form);
        ASSERT_EQ(resp.code, 200) << resp.body;
        const Json::Value body = Json::Value::FromText(resp.body);
        const Json::JsonArray& duplicates = body["possible_duplicates"].GetArray();
        ASSERT_EQ(duplicates.size(), 1u);
        EXPECT_EQ(duplicates[0]["event_id"].Get<int64_t>(), existing);
        EXPECT_EQ(duplicates[0]["date_index"].Get<int64_t>(), 0);
        EXPECT_EQ(duplicates[0]["starts_at_us"].Get<int64_t>(), startsAtUs + kUsPerHour);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(EventSubmissionsEndpointTest, TheHoneypotAnswersOkAndWritesNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheHoneypotAnswersOkAndWritesNothing", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        int64_t personId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        Json::JsonObject form = Form(NowUs(transaction));
        form["website"] = Json::Value("https://example.org");

        crow::response resp = Submit(helper, form);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(Json::Value::FromText(resp.body)["submission_id"].Get<int64_t>(), 0);
        EXPECT_EQ(TableHelpers::EventSubmissions(testDb.GetDatabaseHelper())
                      .CountSubmissionsSince(transaction, personId, 0), 0);
    });
    Auth::ServerConfig::Shutdown();
}

// Only the caller's own, with each date's state and none of the curators' notes.
TEST(EventSubmissionsEndpointTest, MineListsOnlyYourOwn) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MineListsOnlyYourOwn", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        SignIn(transaction, testDb, helper, "other@example.com", false);
        ASSERT_EQ(Submit(helper, Form(nowUs)).code, 200);
        SignIn(transaction, testDb, helper, "submitter@example.com", false);
        crow::response submitted = Submit(helper, Form(nowUs + kUsPerHour, 2));
        ASSERT_EQ(submitted.code, 200) << submitted.body;
        Json::Value submittedBody = Json::Value::FromText(submitted.body);
        const int64_t submissionId = submittedBody["submission_id"].Get<int64_t>();
        const int64_t firstDate = submittedBody["event_ids"].GetArray()[0].Get<int64_t>();

        Events::SubmissionHelper submissions(databaseHelper);
        ASSERT_TRUE(submissions.ReviewDate(transaction, firstDate, Events::ReviewAction::Approve,
                                           77, "curators only", {}).review.ok);

        crow::response resp = Mine(helper);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(resp.body.find("curators only"), std::string::npos);
        EXPECT_EQ(resp.body.find("review_notes"), std::string::npos);
        const Json::Value body = Json::Value::FromText(resp.body);
        const Json::JsonArray& items = body["items"].GetArray();
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0]["id"].Get<int64_t>(), submissionId);
        const Json::JsonArray& dates = items[0]["dates"].GetArray();
        ASSERT_EQ(dates.size(), 2u);
        EXPECT_EQ(dates[0]["event_id"].Get<int64_t>(), firstDate);
        EXPECT_EQ(dates[0]["state"].Get<std::string>(), "listed");
        EXPECT_EQ(dates[1]["state"].Get<std::string>(), "waiting");
        EXPECT_TRUE(dates[1]["ends_at_us"].IsNull());
        EXPECT_EQ(items[0]["relationship"].Get<std::string>(), "other");
        EXPECT_FALSE(items[0]["regular_thing"].Get<bool>());
    });
    Auth::ServerConfig::Shutdown();
}

// Someone else's submission is a 404, exactly like one that doesn't exist.
TEST(EventSubmissionsEndpointTest, WithdrawYoursButNotSomeoneElses) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("WithdrawYoursButNotSomeoneElses", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SignIn(transaction, testDb, helper, "submitter@example.com", false);
        crow::response submitted = Submit(helper, Form(NowUs(transaction), 2));
        ASSERT_EQ(submitted.code, 200) << submitted.body;
        Json::Value submittedBody = Json::Value::FromText(submitted.body);
        const int64_t submissionId = submittedBody["submission_id"].Get<int64_t>();

        SignIn(transaction, testDb, helper, "other@example.com", false);
        crow::response notYours = Withdraw(helper, submissionId);
        EXPECT_EQ(notYours.code, 404) << notYours.body;
        crow::response missing = Withdraw(helper, 999999);
        EXPECT_EQ(missing.code, 404);
        EXPECT_EQ(notYours.body, missing.body);

        SwitchTo(transaction, testDb, helper, "submitter@example.com");
        crow::response resp = Withdraw(helper, submissionId);
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["submission_id"].Get<int64_t>(), submissionId);
        EXPECT_EQ(body["withdrawn_event_ids"].GetArray().size(), 2u);
        for (const Json::Value& eventId : submittedBody["event_ids"].GetArray()) {
            EXPECT_EQ(TableHelpers::Events(databaseHelper)
                          .GetEvent(transaction, eventId.Get<int64_t>())
                          .at(std::string(DbSchema::kEventsStatus)),
                      "rejected");
        }

        crow::response again = Withdraw(helper, submissionId);
        ASSERT_EQ(again.code, 400) << again.body;
        EXPECT_EQ(ErrorCodeOf(again), Events::kSubmissionErrorNothingPending);
    });
    Auth::ServerConfig::Shutdown();
}

void SetPaused(Transaction& transaction, EndpointTestHelper& helper, bool paused) {
    Events::SubmissionSettingsUpdate update;
    update.submissionsPaused = paused;
    ASSERT_TRUE(Events::SaveSubmissionSettings(*helper.GetSecretsHelper(), transaction, update).ok);
}

// While paused, a submit is a 503 carrying the notice and writes nothing; the
// person's own submissions and withdrawing keep working.
TEST(EventSubmissionsEndpointTest, APausedSubmitIs503ButMineAndWithdrawStillWork) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("APausedSubmitIs503ButMineAndWithdrawStillWork", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        int64_t personId = SignIn(transaction, testDb, helper, "submitter@example.com", false);
        const int64_t nowUs = NowUs(transaction);
        crow::response before = Submit(helper, Form(nowUs));
        ASSERT_EQ(before.code, 200) << before.body;
        const int64_t submissionId = Json::Value::FromText(before.body)["submission_id"].Get<int64_t>();

        SetPaused(transaction, helper, true);
        crow::response paused = Submit(helper, Form(nowUs + kUsPerHour, 2));
        ASSERT_EQ(paused.code, 503) << paused.body;
        Json::Value body = Json::Value::FromText(paused.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kSubmissionErrorPaused);
        EXPECT_EQ(body["detail"].Get<std::string>(),
                  "We're not taking event submissions right now. Please check back soon.");
        EXPECT_FALSE(body.HasChild("field"));
        EXPECT_EQ(TableHelpers::EventSubmissions(testDb.GetDatabaseHelper())
                      .CountSubmissionsSince(transaction, personId, 0), 1);

        crow::response mine = Mine(helper);
        ASSERT_EQ(mine.code, 200) << mine.body;
        EXPECT_EQ(Json::Value::FromText(mine.body)["items"].GetArray().size(), 1u);
        EXPECT_EQ(Withdraw(helper, submissionId).code, 200);

        SetPaused(transaction, helper, false);
        crow::response resumed = Submit(helper, Form(nowUs + kUsPerHour, 2));
        EXPECT_EQ(resumed.code, 200) << resumed.body;
    });
    Auth::ServerConfig::Shutdown();
}

// The form asks before anyone fills it in, so the status needs no sign-in.
TEST(EventSubmissionsEndpointTest, TheStatusRouteAnswersSignedOut) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheStatusRouteAnswersSignedOut", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        auto accepting = [&]() {
            crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/event_submissions/status");
            EXPECT_EQ(resp.code, 200) << resp.body;
            return Json::Value::FromText(resp.body)["accepting_submissions"].Get<bool>();
        };
        EXPECT_TRUE(accepting());
        SetPaused(transaction, helper, true);
        EXPECT_FALSE(accepting());
        SetPaused(transaction, helper, false);
        EXPECT_TRUE(accepting());
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
