#include "commands/jobs_commands.h"

#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "db_schema/admin_alerts.h"
#include "db_schema/events.h"
#include "sql_util/table_helpers/admin_alerts.h"
#include "sql_util/table_helpers/events.h"
#include "test/src/util/database_test_helper.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/secrets/secret_keys.h"
#include "util/types.h"

#include "command_runner.h"

// EV4 §7 — the jobs commands, run through the registry against the test
// database. The context is built by hand, with the database's secrets and the
// capturing mail helper.
namespace TestHelper {
namespace {

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

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

TestHelperContext MakeContext(DatabaseHelper databaseHelper, ::Mail::Test::TestMailHelperPtr mail = nullptr) {
    TestHelperContext ctx(databaseHelper);
    ctx.secretsHelper = Secrets::MakeSecretsHelper(databaseHelper);
    ctx.mailHelper = mail;
    RegisterJobsCommands(ctx.registry);
    return ctx;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

int64_t AddEvent(Transaction& transaction, DatabaseHelper databaseHelper, std::string_view status,
                 int64_t startsAtUs, std::optional<int64_t> endsAtUs = std::nullopt) {
    KeyValueTable values = {
        { std::string(DbSchema::kEventsTitle), "Sample Event" },
        { std::string(DbSchema::kEventsStatus), std::string(status) },
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs) },
    };
    if (endsAtUs) values[std::string(DbSchema::kEventsEndsAtUs)] = StringFromInt(*endsAtUs);
    return TableHelpers::Events(databaseHelper).AddEvent(transaction, values);
}

KeyValueTable EventRow(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
}

TEST(JobsCommandsTest, EachCommandIsRegisteredUnderJobsWithItsAlias) {
    CommandRegistry registry;
    RegisterJobsCommands(registry);
    for (const auto& [name, alias] : std::map<std::string, std::string>{
             { "run-job", "rj" }, { "create-alert", "ca" }, { "age-event", "ae" } }) {
        const CommandInfo* byName = registry.Find(name);
        ASSERT_NE(byName, nullptr) << name;
        EXPECT_EQ(byName->category, "Jobs");
        EXPECT_EQ(registry.Find(alias), byName) << alias;
    }
}

// run-job runs the job in-process and prints its counts.
TEST(JobsCommandsTest, RunJobRunsTheNamedJobAndPrintsItsCounts) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RunJobRunsTheNamedJob", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);
        const int64_t past = AddEvent(transaction, databaseHelper, "approved", NowUs(transaction) - 9 * kUsPerHour);

        int exitCode = -1;
        std::string printed =
            RunAndCapture(ctx, transaction, "rj", { { "name", "archive_past_events" } }, exitCode);
        EXPECT_EQ(exitCode, 0);
        EXPECT_NE(printed.find("archive_past_events"), std::string::npos) << printed;
        EXPECT_NE(printed.find("\"archived\":1"), std::string::npos) << printed;
        EXPECT_EQ(EventRow(transaction, databaseHelper, past).at(std::string(DbSchema::kEventsStatus)), "archived");

        printed = RunAndCapture(ctx, transaction, "run-job", { { "name", "archive_past_events" } }, exitCode);
        EXPECT_EQ(exitCode, 0);
        EXPECT_NE(printed.find("\"archived\":0"), std::string::npos) << printed;

        // A missing or unknown name lists the jobs.
        printed = RunAndCapture(ctx, transaction, "rj", {}, exitCode);
        EXPECT_EQ(exitCode, 1);
        EXPECT_NE(printed.find("notify_admin_alerts"), std::string::npos) << printed;
        EXPECT_EQ(RunCommand(ctx, transaction, "rj", { { "name", "import_event_feeds" } }), 1);
    });
}

// create-alert, then the digest mails it.
TEST(JobsCommandsTest, CreateAlertFeedsTheDigest) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CreateAlertFeedsTheDigest", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        ::Mail::Test::TestMailHelperPtr mail = ::Mail::Test::MakeTestMailHelper();
        TestHelperContext ctx = MakeContext(databaseHelper, mail);
        ctx.secretsHelper->AddSecret(transaction, Secrets::kMailSenderName, "Sample Site");
        ctx.secretsHelper->AddSecret(transaction, Secrets::kMailSenderAddress, "sender@example.com");
        ctx.secretsHelper->AddSecret(transaction, Secrets::kAdminAlertsRecipient, "admin@example.com");

        EXPECT_EQ(RunCommand(ctx, transaction, "ca"), 1);
        ASSERT_EQ(RunCommand(ctx, transaction, "ca", { { "description", "A sample problem" } }), 0);
        KeyValueTableArray alerts = TableHelpers::AdminAlerts(databaseHelper).GetAdminAlertsSince(transaction, 0);
        ASSERT_EQ(alerts.size(), 1u);
        EXPECT_EQ(alerts[0].at(std::string(DbSchema::kAdminAlertsDescription)), "A sample problem");

        ASSERT_EQ(RunCommand(ctx, transaction, "rj", { { "name", "notify_admin_alerts" } }), 0);
        ASSERT_EQ(mail->GetMessages().size(), 1u);
        EXPECT_NE(mail->GetMessages()[0].GetBodyText().find("A sample problem"), std::string::npos);
    });
}

// age-event moves the start to N hours ago and the end with it; then the
// sweeps find the event.
TEST(JobsCommandsTest, AgeEventMovesTheEventIntoThePastForTheSweeps) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AgeEventMovesTheEvent", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);
        const int64_t nowUs = NowUs(transaction);
        const int64_t approved =
            AddEvent(transaction, databaseHelper, "approved", nowUs + 10 * kUsPerHour, nowUs + 12 * kUsPerHour);
        const int64_t pending = AddEvent(transaction, databaseHelper, "pending", nowUs + 10 * kUsPerHour);

        const int64_t before = NowUs(transaction);
        ASSERT_EQ(RunCommand(ctx, transaction, "ae",
                             { { "id", StringFromInt(approved) }, { "hours_ago", "9" } }), 0);
        const int64_t after = NowUs(transaction);
        KeyValueTable row = EventRow(transaction, databaseHelper, approved);
        const int64_t startsAtUs = std::stoll(row.at(std::string(DbSchema::kEventsStartsAtUs)));
        EXPECT_GE(startsAtUs, before - 9 * kUsPerHour);
        EXPECT_LE(startsAtUs, after - 9 * kUsPerHour);
        EXPECT_EQ(std::stoll(row.at(std::string(DbSchema::kEventsEndsAtUs))) - startsAtUs, 2 * kUsPerHour);

        ASSERT_EQ(RunCommand(ctx, transaction, "age-event",
                             { { "id", StringFromInt(pending) }, { "hours_ago", "1" } }), 0);
        EXPECT_EQ(EventRow(transaction, databaseHelper, pending).at(std::string(DbSchema::kEventsEndsAtUs)), "");

        ASSERT_EQ(RunCommand(ctx, transaction, "rj", { { "name", "archive_past_events" } }), 0);
        ASSERT_EQ(RunCommand(ctx, transaction, "rj", { { "name", "expire_unreviewed_events" } }), 0);
        EXPECT_EQ(EventRow(transaction, databaseHelper, approved).at(std::string(DbSchema::kEventsStatus)), "archived");
        EXPECT_EQ(EventRow(transaction, databaseHelper, pending).at(std::string(DbSchema::kEventsStatus)), "rejected");
    });
}

TEST(JobsCommandsTest, AgeEventChecksItsFlags) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AgeEventChecksItsFlags", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx = MakeContext(databaseHelper);
        const int64_t startsAtUs = NowUs(transaction) + 10 * kUsPerHour;
        const int64_t eventId = AddEvent(transaction, databaseHelper, "pending", startsAtUs);

        EXPECT_EQ(RunCommand(ctx, transaction, "ae", { { "hours_ago", "1" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "ae", { { "id", "999999" }, { "hours_ago", "1" } }), 1);
        for (const std::string bad : { "", "soon", "-1" }) {
            EXPECT_EQ(RunCommand(ctx, transaction, "ae", { { "id", StringFromInt(eventId) }, { "hours_ago", bad } }), 1)
                << bad;
        }
        EXPECT_EQ(EventRow(transaction, databaseHelper, eventId).at(std::string(DbSchema::kEventsStartsAtUs)),
                  StringFromInt(startsAtUs));
    });
}

}  // namespace
}  // namespace TestHelper
