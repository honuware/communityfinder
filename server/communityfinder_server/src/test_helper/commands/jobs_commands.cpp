#include "commands/jobs_commands.h"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "business_logic/jobs/scheduled_jobs.h"
#include "db_schema/events.h"
#include "sql_util/table_helpers/admin_alerts.h"
#include "sql_util/table_helpers/events.h"
#include "util/types.h"

#include "command_runner.h"
#include "console_colors.h"

namespace TestHelper {

namespace {

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

std::string Arg(const Args& args, const char* name) {
    auto it = args.find(name);
    return it != args.end() ? it->second : std::string();
}

// A whole number, or nullopt when the flag is missing or isn't one.
std::optional<int64_t> IntArg(const Args& args, const char* name) {
    std::string text = Arg(args, name);
    if (text.empty()) return std::nullopt;
    try {
        size_t used = 0;
        int64_t value = std::stoll(text, &used);
        if (used == text.size()) return value;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

std::string JobNameList() {
    std::string names;
    for (std::string_view name : Jobs::ScheduledJobNames()) {
        if (!names.empty()) names += ", ";
        names += std::string(name);
    }
    return names;
}

int RunRunJob(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string name = Arg(args, "name");
    if (name.empty()) {
        Color::PrintError("--name is required: one of " + JobNameList());
        return 1;
    }
    std::optional<Json::Value> counts = Jobs::RunScheduledJob(
        transaction, ctx.databaseHelper, *ctx.secretsHelper, ctx.mailHelper, name);
    if (!counts) {
        Color::PrintError("No job named " + name + ": one of " + JobNameList());
        return 1;
    }
    Color::PrintSuccess(name + ": " + counts->ToString());
    return 0;
}

int RunCreateAlert(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string description = Arg(args, "description");
    if (description.empty()) {
        Color::PrintError("--description is required");
        return 1;
    }
    TableHelpers::AdminAlerts(ctx.databaseHelper).AddAdminAlert(transaction, description);
    Color::PrintSuccess("Recorded admin alert: " + description);
    Color::PrintDim("run-job --name=notify_admin_alerts mails it to admin_alerts_recipient.");
    return 0;
}

// Moves the event so it started `hours_ago` hours ago; an end moves with it,
// so the event keeps its length.
int RunAgeEvent(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::optional<int64_t> eventId = IntArg(args, "id");
    if (!eventId) {
        Color::PrintError("--id must be an event id");
        return 1;
    }
    std::optional<int64_t> hoursAgo = IntArg(args, "hours_ago");
    if (!hoursAgo || *hoursAgo < 0) {
        Color::PrintError("--hours_ago must be a whole number of hours, 0 or more");
        return 1;
    }
    TableHelpers::Events events(ctx.databaseHelper);
    KeyValueTable row = events.GetEvent(transaction, *eventId);
    if (row.empty()) {
        Color::PrintError("No event with id " + StringFromInt(*eventId));
        return 1;
    }

    const int64_t oldStartUs = std::stoll(row.at(std::string(DbSchema::kEventsStartsAtUs)));
    const int64_t newStartUs = NowUs(transaction) - *hoursAgo * kUsPerHour;
    KeyValueTable updates = {
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(newStartUs) },
    };
    const std::string endText = row.at(std::string(DbSchema::kEventsEndsAtUs));
    if (!endText.empty()) {
        updates[std::string(DbSchema::kEventsEndsAtUs)] =
            StringFromInt(newStartUs + (std::stoll(endText) - oldStartUs));
    }
    events.UpdateEvent(transaction, *eventId, updates);
    Color::PrintSuccess("Event " + StringFromInt(*eventId) + " (" + row.at(std::string(DbSchema::kEventsStatus))
                        + ") now started " + StringFromInt(*hoursAgo) + " hour(s) ago");
    Color::PrintDim("Approved and ended 6+ hours ago: archive_past_events archives it. "
                    "Pending and started: expire_unreviewed_events rejects it.");
    return 0;
}

}  // namespace

void RegisterJobsCommands(CommandRegistry& registry) {
    registry.Register({
        "run-job", "rj", "Jobs",
        "Run one scheduled job now, in-process, and print its counts",
        {{ "--name", "archive_past_events, expire_unreviewed_events, cleanup_expired_tokens, "
                     "cleanup_login_attempts, cleanup_scaled_photos, cleanup_idempotency_keys "
                     "or notify_admin_alerts", true }},
        RunRunJob
    });

    registry.Register({
        "create-alert", "ca", "Jobs",
        "Record an admin alert, for the notify_admin_alerts digest to mail",
        {{ "--description", "What the alert says (quote it if it has spaces)", true }},
        RunCreateAlert
    });

    registry.Register({
        "age-event", "ae", "Jobs",
        "Move an event into the past (its end moves with it) for archive / expire to find",
        {{ "--id", "Event id", true },
         { "--hours_ago", "Hours ago it now starts", true }},
        RunAgeEvent
    });
}

}  // namespace TestHelper
