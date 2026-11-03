#include "commands/submission_commands.h"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/events/submission_helper.h"
#include "business_logic/events/submission_settings.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "db_schema/people.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/people.h"
#include "test/src/util/database_test_helper.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

#include "command_runner.h"

// EV3 §6 — the submission simulation commands, run through the registry
// against the test database. The context is built by hand, with the in-memory
// test secrets standing in for the real store.
namespace TestHelper {
namespace {

constexpr int64_t kCuratorId = 7;  // reviewer ids aren't FKs (EV1 OQ8 (e))
constexpr std::string_view kSubmitterEmail = "submitter@example.com";

int RunCommand(TestHelperContext& ctx, Transaction& transaction, const std::string& command,
               const Args& args = {}) {
    const CommandInfo* info = ctx.registry.Find(command);
    EXPECT_NE(info, nullptr) << command;
    return info != nullptr ? info->execute(ctx, transaction, args) : -1;
}

// Runs the command and returns what it printed.
std::string RunAndCapture(TestHelperContext& ctx, Transaction& transaction,
                          const std::string& command, const Args& args, int& exitCode) {
    std::ostringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
    try {
        exitCode = RunCommand(ctx, transaction, command, args);
    } catch (...) {
        std::cout.rdbuf(original);
        throw;
    }
    std::cout.rdbuf(original);
    return captured.str();
}

TestHelperContext MakeContext(DatabaseHelper databaseHelper) {
    TestHelperContext ctx(databaseHelper);
    ctx.secretsHelper = Secrets::Test::MakeTestSecretsHelper();
    RegisterSubmissionCommands(ctx.registry);
    return ctx;
}

int64_t PersonId(Transaction& transaction, DatabaseHelper databaseHelper, std::string_view email) {
    KeyValueTable row = TableHelpers::People(databaseHelper).LookupPersonByEmail(transaction, email);
    return row.empty() ? 0 : std::stoll(row.at(std::string(DbSchema::kPeopleId)));
}

int64_t SubmissionCount(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        "SELECT count(*) FROM " + std::string(DbSchema::kEventSubmissionsTable)));
}

Args SubmitArgs(std::string_view title = "Sample Dance Night", std::string_view dates = "1") {
    return { { "as", std::string(kSubmitterEmail) }, { "title", std::string(title) },
             { "starts_in_hours", "48" }, { "dates", std::string(dates) } };
}

// A new email becomes a verified member with no roles, and the dates are
// pending, user_submitted events made by them, titled as demo rows.
TEST(SubmissionCommandsTest, SubmitEventCreatesTheSubmitterAndPendingDates) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SubmitEventCreatesTheSubmitterAndPendingDates", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);
        ASSERT_EQ(PersonId(transaction, databaseHelper, kSubmitterEmail), 0);

        ASSERT_EQ(RunCommand(ctx, transaction, "se", SubmitArgs("Sample Dance Night", "3")), 0);

        KeyValueTable person = TableHelpers::People(databaseHelper).LookupPersonByEmail(transaction, kSubmitterEmail);
        ASSERT_FALSE(person.empty());
        EXPECT_FALSE(person.at(std::string(DbSchema::kPeopleEmailVerifiedAt)).empty());
        const int64_t personId = std::stoll(person.at(std::string(DbSchema::kPeopleId)));

        std::vector<Events::SubmissionView> mine =
            Events::SubmissionHelper(databaseHelper).GetMySubmissions(transaction, personId);
        ASSERT_EQ(mine.size(), 1u);
        ASSERT_EQ(mine[0].dates.size(), 3u);
        TableHelpers::Events events(databaseHelper);
        for (const Events::SubmissionDateView& date : mine[0].dates) {
            EXPECT_EQ(date.title, "Demo: Sample Dance Night");
            EXPECT_EQ(date.state, Events::kSubmissionDateWaiting);
            KeyValueTable row = events.GetEvent(transaction, date.eventId);
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), DbSchema::kEventsStatusPending);
            EXPECT_EQ(row.at(std::string(DbSchema::kEventsCreatedByPersonId)), StringFromInt(personId));
        }
        // Weekly.
        EXPECT_EQ(mine[0].dates[1].startsAtUs - mine[0].dates[0].startsAtUs, 7 * Events::kUsPerDay);
    });
}

// A second submission as the same email reuses the person, and a title that
// already says "Demo:" isn't prefixed twice.
TEST(SubmissionCommandsTest, SubmitEventReusesAnExistingSubmitter) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SubmitEventReusesAnExistingSubmitter", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);
        ASSERT_EQ(RunCommand(ctx, transaction, "submit-event", SubmitArgs()), 0);
        const int64_t personId = PersonId(transaction, databaseHelper, kSubmitterEmail);

        Args again = SubmitArgs("Demo: Sample Trivia");
        again["starts_in_hours"] = "49";
        ASSERT_EQ(RunCommand(ctx, transaction, "submit-event", again), 0);
        EXPECT_EQ(PersonId(transaction, databaseHelper, kSubmitterEmail), personId);
        std::vector<Events::SubmissionView> mine =
            Events::SubmissionHelper(databaseHelper).GetMySubmissions(transaction, personId);
        ASSERT_EQ(mine.size(), 2u);
        bool foundTrivia = false;
        for (const Events::SubmissionView& submission : mine) {
            if (submission.dates[0].title == "Demo: Sample Trivia") foundTrivia = true;
        }
        EXPECT_TRUE(foundTrivia);
    });
}

// The command goes through the helper, so the form's checks refuse it too.
TEST(SubmissionCommandsTest, SubmitEventIsHeldToTheFormsChecks) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SubmitEventIsHeldToTheFormsChecks", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);

        // Its own flags first.
        Args noEmail = SubmitArgs();
        noEmail.erase("as");
        EXPECT_EQ(RunCommand(ctx, transaction, "se", noEmail), 1);
        Args noTitle = SubmitArgs();
        noTitle.erase("title");
        EXPECT_EQ(RunCommand(ctx, transaction, "se", noTitle), 1);
        for (const std::string bad : { "", "soon", "-1" }) {
            Args args = SubmitArgs();
            args["starts_in_hours"] = bad;
            EXPECT_EQ(RunCommand(ctx, transaction, "se", args), 1) << bad;
        }
        EXPECT_EQ(RunCommand(ctx, transaction, "se", SubmitArgs("Sample Dance Night", "0")), 1);
        EXPECT_EQ(PersonId(transaction, databaseHelper, kSubmitterEmail), 0);

        // Then the helper's: too soon, too many dates, an unknown venue.
        Args soon = SubmitArgs();
        soon["starts_in_hours"] = "0";
        EXPECT_EQ(RunCommand(ctx, transaction, "se", soon), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "se", SubmitArgs("Sample Dance Night", "13")), 1);
        Args venue = SubmitArgs();
        venue["venue_id"] = "999999";
        EXPECT_EQ(RunCommand(ctx, transaction, "se", venue), 1);
        EXPECT_EQ(SubmissionCount(transaction), 0);
    });
}

// The limits and the pause switch, set through submission-settings, hold the
// next submit-event.
TEST(SubmissionCommandsTest, TheSettingsCommandHoldsSubmitEventToItsLimits) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheSettingsCommandHoldsSubmitEventToItsLimits", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);
        ASSERT_EQ(RunCommand(ctx, transaction, "ss", { { "max_pending_dates", "2" } }), 0);

        EXPECT_EQ(RunCommand(ctx, transaction, "se", SubmitArgs("Sample Dance Night", "3")), 1);
        ASSERT_EQ(RunCommand(ctx, transaction, "se", SubmitArgs("Sample Dance Night", "2")), 0);
        EXPECT_EQ(SubmissionCount(transaction), 1);

        ASSERT_EQ(RunCommand(ctx, transaction, "ss",
                             { { "max_pending_dates", "20" }, { "paused", "true" } }), 0);
        Args next = SubmitArgs();
        next["starts_in_hours"] = "50";
        int exitCode = 0;
        std::string printed = RunAndCapture(ctx, transaction, "se", next, exitCode);
        EXPECT_EQ(exitCode, 1);
        EXPECT_NE(printed.find(Events::kSubmissionErrorPaused), std::string::npos) << printed;
        EXPECT_EQ(SubmissionCount(transaction), 1);

        ASSERT_EQ(RunCommand(ctx, transaction, "ss", { { "paused", "false" } }), 0);
        EXPECT_EQ(RunCommand(ctx, transaction, "se", next), 0);
        EXPECT_EQ(SubmissionCount(transaction), 2);
    });
}

TEST(SubmissionCommandsTest, TheSettingsCommandShowsAndChecksItsValues) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheSettingsCommandShowsAndChecksItsValues", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);

        int exitCode = -1;
        std::string shown = RunAndCapture(ctx, transaction, "submission-settings", {}, exitCode);
        EXPECT_EQ(exitCode, 0);
        EXPECT_NE(shown.find("open"), std::string::npos) << shown;
        EXPECT_NE(shown.find("max_pending_dates"), std::string::npos) << shown;

        // Bad values change nothing — not even the good ones beside them.
        EXPECT_EQ(RunCommand(ctx, transaction, "ss",
                             { { "max_pending_dates", "0" }, { "paused", "true" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "ss", { { "max_submissions_per_day", "lots" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "ss", { { "paused", "yes" } }), 1);
        Events::SubmissionSettings settings = Events::LoadSubmissionSettings(*ctx.secretsHelper, transaction);
        EXPECT_EQ(settings.maxPendingDates, Events::kMaxPendingDatesRange.defaultValue);
        EXPECT_EQ(settings.maxSubmissionsPerDay, Events::kMaxSubmissionsPerDayRange.defaultValue);
        EXPECT_FALSE(settings.submissionsPaused);

        shown = RunAndCapture(ctx, transaction, "ss",
                              { { "paused", "true" }, { "max_submissions_per_day", "3" } }, exitCode);
        EXPECT_EQ(exitCode, 0);
        EXPECT_NE(shown.find("PAUSED"), std::string::npos) << shown;
        settings = Events::LoadSubmissionSettings(*ctx.secretsHelper, transaction);
        EXPECT_TRUE(settings.submissionsPaused);
        EXPECT_EQ(settings.maxSubmissionsPerDay, 3);
        EXPECT_EQ(settings.maxPendingDates, Events::kMaxPendingDatesRange.defaultValue);
    });
}

// What My submissions shows: each date's state, for --as or the logged-in person.
TEST(SubmissionCommandsTest, ListSubmissionsShowsEachDatesState) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ListSubmissionsShowsEachDatesState", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);

        // Nobody named, nobody logged in; or a stranger.
        EXPECT_EQ(RunCommand(ctx, transaction, "ls"), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "ls", { { "as", "nobody@example.com" } }), 1);

        ASSERT_EQ(RunCommand(ctx, transaction, "se", SubmitArgs("Sample Dance Night", "2")), 0);
        const int64_t personId = PersonId(transaction, databaseHelper, kSubmitterEmail);
        Events::SubmissionHelper helper(databaseHelper);
        std::vector<Events::SubmissionView> mine = helper.GetMySubmissions(transaction, personId);
        ASSERT_EQ(mine.size(), 1u);
        ASSERT_TRUE(helper.ReviewDate(transaction, mine[0].dates[0].eventId, Events::ReviewAction::Approve,
                                      kCuratorId, "", {}).review.ok);

        int exitCode = -1;
        std::string listed = RunAndCapture(ctx, transaction, "list-submissions",
                                           { { "as", std::string(kSubmitterEmail) } }, exitCode);
        EXPECT_EQ(exitCode, 0);
        EXPECT_NE(listed.find("Submission " + StringFromInt(mine[0].id)), std::string::npos) << listed;
        EXPECT_NE(listed.find(Events::kSubmissionDateListed), std::string::npos) << listed;
        EXPECT_NE(listed.find(Events::kSubmissionDateWaiting), std::string::npos) << listed;
        EXPECT_NE(listed.find("Demo: Sample Dance Night"), std::string::npos) << listed;

        // The logged-in person, when --as is left off.
        ctx.currentPersonId = personId;
        std::string loggedIn = RunAndCapture(ctx, transaction, "ls", {}, exitCode);
        EXPECT_EQ(exitCode, 0);
        EXPECT_EQ(loggedIn, listed);
    });
}

}  // namespace
}  // namespace TestHelper
