#include "admin_event_submission_settings.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/submission_helper.h"
#include "business_logic/events/submission_settings.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/types.h"

// EV3 §4.5 / §4.6 — GET / PUT /api/admin/event_submission_settings: the two
// submission limits and the pause switch.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr const char* kSettingsUrl = "/api/admin/event_submission_settings";

crow::response GetSettings(EndpointTestHelper& helper) {
    return Request(helper, crow::HTTPMethod::Get, kSettingsUrl);
}

crow::response PutSettings(EndpointTestHelper& helper, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Put, kSettingsUrl, body);
}

void ExpectLimit(const Json::Value& body, std::string_view name, int64_t value,
                 const Events::SubmissionLimitRange& range) {
    const Json::Value& limit = body[name];
    EXPECT_EQ(limit["value"].Get<int64_t>(), value) << name;
    EXPECT_EQ(limit["default"].Get<int64_t>(), range.defaultValue) << name;
    EXPECT_EQ(limit["min"].Get<int64_t>(), range.min) << name;
    EXPECT_EQ(limit["max"].Get<int64_t>(), range.max) << name;
}

void ExpectSettings(const crow::response& resp, int64_t maxPendingDates, int64_t maxPerDay,
                    bool paused = false) {
    ASSERT_EQ(resp.code, 200) << resp.body;
    const Json::Value body = Json::Value::FromText(resp.body);
    ExpectLimit(body, Events::kSubmissionLimitMaxPendingDates, maxPendingDates,
                Events::kMaxPendingDatesRange);
    ExpectLimit(body, Events::kSubmissionLimitMaxSubmissionsPerDay, maxPerDay,
                Events::kMaxSubmissionsPerDayRange);
    const Json::Value& submissionsPaused = body[Events::kSubmissionSettingSubmissionsPaused];
    EXPECT_EQ(submissionsPaused["value"].Get<bool>(), paused);
    EXPECT_FALSE(submissionsPaused["default"].Get<bool>());
}

// The submit form, `dateCount` weekly dates from two days out.
std::string Form(int64_t nowUs, int dateCount) {
    Json::JsonArray dates;
    for (int index = 0; index < dateCount; ++index) {
        dates.push_back(Json::Value(Json::JsonObject{
            { "starts_at_us", Json::Value(nowUs + (2 + 7 * index) * Events::kUsPerDay) },
        }));
    }
    return Json::Value(Json::JsonObject{
        { "title", Json::Value("Sample Dance Night") },
        { "dates", Json::Value(dates) },
        { "agreed", Json::Value(true) },
    }).ToString();
}

TEST(AdminEventSubmissionSettingsTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        const std::string limit = R"({"max_pending_dates": 20})";
        const std::string pause = R"({"submissions_paused": true})";

        EXPECT_EQ(GetSettings(helper).code, 401);
        EXPECT_EQ(PutSettings(helper, limit).code, 401);
        EXPECT_EQ(PutSettings(helper, pause).code, 401);
        // The permission exists (a curator holds it); this member doesn't.
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(GetSettings(helper).code, 403);
        EXPECT_EQ(PutSettings(helper, limit).code, 403);
        EXPECT_EQ(PutSettings(helper, pause).code, 403);

        SwitchTo(transaction, testDb, helper, "curator@example.com");
        ExpectSettings(GetSettings(helper), 10, 5);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventSubmissionSettingsTest, GetShowsEachLimitWithItsDefaultAndRange) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetShowsEachLimitWithItsDefaultAndRange", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        ExpectSettings(GetSettings(helper), 10, 5);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventSubmissionSettingsTest, PuttingOneLimitLeavesTheOther) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PuttingOneLimitLeavesTheOther", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        ExpectSettings(PutSettings(helper, R"({"max_pending_dates": 20})"), 20, 5);
        ExpectSettings(GetSettings(helper), 20, 5);
        // null is "not given", like an absent field.
        ExpectSettings(PutSettings(helper, R"({"max_pending_dates": null, "max_submissions_per_day": 50})"),
                       20, 50);
        ExpectSettings(GetSettings(helper), 20, 50);
    });
    Auth::ServerConfig::Shutdown();
}

// The switch on its own: it leaves the limits, and the limits leave it.
TEST(AdminEventSubmissionSettingsTest, ThePauseSwitchFlipsOnItsOwn) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ThePauseSwitchFlipsOnItsOwn", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        ExpectSettings(PutSettings(helper, R"({"max_pending_dates": 20})"), 20, 5);

        ExpectSettings(PutSettings(helper, R"({"submissions_paused": true})"), 20, 5, true);
        ExpectSettings(GetSettings(helper), 20, 5, true);
        ExpectSettings(PutSettings(helper, R"({"max_submissions_per_day": 3})"), 20, 3, true);
        ExpectSettings(PutSettings(helper, R"({"submissions_paused": false})"), 20, 3, false);
        ExpectSettings(GetSettings(helper), 20, 3, false);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventSubmissionSettingsTest, BadValuesAre400AndWriteNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BadValuesAre400AndWriteNothing", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        // One out of range: nothing is written, and the error names it.
        crow::response outOfRange = PutSettings(
            helper, R"({"max_pending_dates": 20, "max_submissions_per_day": 51, "submissions_paused": true})");
        ASSERT_EQ(outOfRange.code, 400) << outOfRange.body;
        Json::Value body = Json::Value::FromText(outOfRange.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kSubmissionErrorLimitOutOfRange);
        EXPECT_EQ(body["field"].Get<std::string>(), Events::kSubmissionLimitMaxSubmissionsPerDay);
        crow::response zero = PutSettings(helper, R"({"max_pending_dates": 0})");
        ASSERT_EQ(zero.code, 400) << zero.body;
        EXPECT_EQ(Json::Value::FromText(zero.body)["field"].Get<std::string>(),
                  Events::kSubmissionLimitMaxPendingDates);

        crow::response notANumber = PutSettings(helper, R"({"max_pending_dates": "ten"})");
        ASSERT_EQ(notANumber.code, 400) << notANumber.body;
        EXPECT_EQ(Json::Value::FromText(notANumber.body)["field"].Get<std::string>(),
                  Events::kSubmissionLimitMaxPendingDates);
        EXPECT_EQ(PutSettings(helper, R"({"max_pending_dates": 2.5})").code, 400);
        // The switch takes true or false, nothing that merely looks like them.
        for (const std::string notABoolean : { R"("true")", "1", "0", R"("yes")" }) {
            crow::response resp = PutSettings(
                helper, R"({"max_pending_dates": 20, "submissions_paused": )" + notABoolean + "}");
            ASSERT_EQ(resp.code, 400) << notABoolean << ": " << resp.body;
            Json::Value error = Json::Value::FromText(resp.body);
            EXPECT_EQ(error["field"].Get<std::string>(), Events::kSubmissionSettingSubmissionsPaused);
            EXPECT_EQ(error["detail"].Get<std::string>(), "submissions_paused must be true or false");
        }
        EXPECT_EQ(PutSettings(helper, "{}").code, 400);
        EXPECT_EQ(PutSettings(helper, R"({"submissions_paused": null})").code, 400);
        EXPECT_EQ(PutSettings(helper, "not json").code, 400);

        ExpectSettings(GetSettings(helper), 10, 5, false);
    });
    Auth::ServerConfig::Shutdown();
}

// A change on the settings page holds the very next submission to it.
TEST(AdminEventSubmissionSettingsTest, ALoweredLimitAppliesToTheNextSubmission) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ALoweredLimitAppliesToTheNextSubmission", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        ExpectSettings(PutSettings(helper, R"({"max_pending_dates": 2})"), 2, 5);

        SignIn(transaction, testDb, helper, "submitter@example.com", false);
        const int64_t nowUs = NowUs(transaction);
        crow::response refused = Request(helper, crow::HTTPMethod::Post, "/api/event_submissions", Form(nowUs, 3));
        ASSERT_EQ(refused.code, 429) << refused.body;
        EXPECT_NE(Json::Value::FromText(refused.body)["detail"].Get<std::string>().find("up to 2 dates"),
                  std::string::npos) << refused.body;
        crow::response accepted = Request(helper, crow::HTTPMethod::Post, "/api/event_submissions", Form(nowUs, 2));
        EXPECT_EQ(accepted.code, 200) << accepted.body;
    });
    Auth::ServerConfig::Shutdown();
}

// Pausing on the settings page closes the form for the next submission, and
// resuming opens it again.
TEST(AdminEventSubmissionSettingsTest, PausingAppliesToTheNextSubmission) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PausingAppliesToTheNextSubmission", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        ExpectSettings(PutSettings(helper, R"({"submissions_paused": true})"), 10, 5, true);

        SignIn(transaction, testDb, helper, "submitter@example.com", false);
        const int64_t nowUs = NowUs(transaction);
        crow::response paused = Request(helper, crow::HTTPMethod::Post, "/api/event_submissions", Form(nowUs, 1));
        ASSERT_EQ(paused.code, 503) << paused.body;
        EXPECT_EQ(Json::Value::FromText(paused.body)["error_code"].Get<std::string>(),
                  Events::kSubmissionErrorPaused);

        SwitchTo(transaction, testDb, helper, "curator@example.com");
        ExpectSettings(PutSettings(helper, R"({"submissions_paused": false})"), 10, 5, false);
        SwitchTo(transaction, testDb, helper, "submitter@example.com");
        crow::response accepted = Request(helper, crow::HTTPMethod::Post, "/api/event_submissions", Form(nowUs, 1));
        EXPECT_EQ(accepted.code, 200) << accepted.body;
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
