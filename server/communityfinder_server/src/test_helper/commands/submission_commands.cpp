#include "commands/submission_commands.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/auth/auth_helper.h"
#include "business_logic/auth/person.h"
#include "business_logic/events/submission_helper.h"
#include "business_logic/events/submission_settings.h"
#include "db_schema/people.h"
#include "sql_util/table_helpers/people.h"
#include "util/types.h"

#include "command_runner.h"
#include "console_colors.h"

namespace TestHelper {

namespace {

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;
constexpr int64_t kUsPerWeek = 7 * Events::kUsPerDay;

// The EV1 §7.4 convention: every demo row says it is one.
constexpr std::string_view kDemoTitlePrefix = "Demo: ";
constexpr std::string_view kDemoFirstName = "Demo";
constexpr std::string_view kDemoLastName = "Submitter";
constexpr size_t kDemoPasswordBytes = 12;

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

// microseconds since epoch → "YYYY-MM-DD HH:MM UTC".
std::string FormatUtc(int64_t us) {
    std::time_t seconds = static_cast<std::time_t>(us / 1'000'000);
    std::tm parts{};
#if defined(_WIN32)
    gmtime_s(&parts, &seconds);
#else
    gmtime_r(&seconds, &parts);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &parts);
    return buffer;
}

std::string JoinSlugs(const std::vector<std::string>& slugs) {
    std::string joined;
    for (const std::string& slug : slugs) {
        if (!joined.empty()) joined += ",";
        joined += slug;
    }
    return joined;
}

int64_t PersonIdFromRow(const KeyValueTable& row) {
    return std::stoll(row.at(std::string(DbSchema::kPeopleId)));
}

// The person to act as: --as when given, else whoever is logged in; 0 (after
// printing why) when there is neither, or --as names nobody.
int64_t ActingPersonId(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string email = Arg(args, "as");
    if (email.empty()) {
        if (ctx.currentPersonId <= 0) {
            Color::PrintError("Give --as=<email>, or log in first");
        }
        return ctx.currentPersonId;
    }
    KeyValueTable row = TableHelpers::People(ctx.databaseHelper).LookupPersonByEmail(transaction, email);
    if (row.empty()) {
        Color::PrintError("No person with email " + email);
        return 0;
    }
    return PersonIdFromRow(row);
}

// The submitter --as names, created (verified, no roles) when the email is new.
int64_t FindOrCreateSubmitter(TestHelperContext& ctx, Transaction& transaction,
                              const std::string& email) {
    TableHelpers::People people(ctx.databaseHelper);
    KeyValueTable row = people.LookupPersonByEmail(transaction, email);
    if (!row.empty()) {
        return PersonIdFromRow(row);
    }
    Auth::AuthHelper authHelper;
    const std::string password = authHelper.Base64Encode(authHelper.RandomBytes(kDemoPasswordBytes));
    Auth::PersonHelper(ctx.databaseHelper).CreateFullyValidatedUser(
        transaction,
        { email, std::string(kDemoFirstName), std::string(kDemoLastName) },
        password,
        ctx.secretsHelper);
    const int64_t personId = PersonIdFromRow(people.LookupPersonByEmail(transaction, email));
    Color::PrintSuccess("Created demo submitter " + StringFromInt(personId) + ": " + email
                        + " (verified, no roles)");
    Color::PrintDim("Password, shown once: " + password);
    return personId;
}

int RunSubmitEvent(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string email = Arg(args, "as");
    if (email.empty()) {
        Color::PrintError("--as is required: the email to submit as");
        return 1;
    }
    std::string title = Arg(args, "title");
    if (title.empty()) {
        Color::PrintError("--title is required");
        return 1;
    }
    std::optional<int64_t> hours = IntArg(args, "starts_in_hours");
    if (!hours || *hours < 0) {
        Color::PrintError("--starts_in_hours must be a whole number of hours, 0 or more");
        return 1;
    }
    int64_t dateCount = 1;
    if (!Arg(args, "dates").empty()) {
        std::optional<int64_t> dates = IntArg(args, "dates");
        if (!dates || *dates < 1) {
            Color::PrintError("--dates must be a whole number, 1 or more");
            return 1;
        }
        dateCount = *dates;
    }

    if (!title.starts_with(kDemoTitlePrefix)) {
        title = std::string(kDemoTitlePrefix) + title;
    }
    Events::SubmissionRequest request;
    request.title = title;
    request.agreed = true;
    const int64_t firstStartUs = NowUs(transaction) + *hours * kUsPerHour;
    for (int64_t index = 0; index < dateCount; ++index) {
        request.dates.push_back({ firstStartUs + index * kUsPerWeek, std::nullopt });
    }
    std::string venueText = Arg(args, "venue_id");
    if (!venueText.empty()) {
        std::optional<int64_t> venueId = IntArg(args, "venue_id");
        if (!venueId) {
            Color::PrintError("--venue_id must be a whole number");
            return 1;
        }
        request.venueId = *venueId;  // an unknown id is the helper's VENUE_NOT_FOUND
    }

    const int64_t personId = FindOrCreateSubmitter(ctx, transaction, email);
    Events::SubmitResult result = Events::SubmissionHelper(ctx.databaseHelper)
                                      .Submit(transaction, *ctx.secretsHelper, request, personId);
    if (!result.ok) {
        std::string where = result.errorField.empty() ? "" : " (" + result.errorField;
        if (result.errorDateIndex >= 0) where += ", date " + StringFromInt(result.errorDateIndex);
        if (!where.empty()) where += ")";
        Color::PrintError(result.errorCode + where + ": " + result.errorMessage);
        return 1;
    }

    Color::PrintSuccess("Submitted " + StringFromInt(dateCount) + " date(s) as submission "
                        + StringFromInt(result.submissionId) + " by " + email);
    std::cout << Color::kBoldCyan << std::left
              << std::setw(8) << "Event"
              << std::setw(22) << "Starts"
              << "Title"
              << Color::kReset << "\n";
    for (size_t index = 0; index < result.eventIds.size(); ++index) {
        std::cout << std::setw(8) << result.eventIds[index]
                  << std::setw(22) << FormatUtc(request.dates[index].startsAtUs)
                  << title << "\n";
    }
    for (const Events::DuplicateHint& hint : result.possibleDuplicates) {
        Color::PrintWarning("Date " + StringFromInt(static_cast<int64_t>(hint.dateIndex))
                            + " might be event " + StringFromInt(hint.eventId) + ": " + hint.title
                            + " (" + FormatUtc(hint.startsAtUs) + ")");
    }
    if (!result.ignoredCategorySlugs.empty() || !result.ignoredSceneTagSlugs.empty()) {
        Color::PrintDim("Ignored slugs: " + JoinSlugs(result.ignoredCategorySlugs) + " "
                        + JoinSlugs(result.ignoredSceneTagSlugs));
    }
    Color::PrintDim("Every date waits for review: approve them on /admin/events, "
                    "or see them with list-submissions --as=" + email + ".");
    return 0;
}

int RunListSubmissions(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    const int64_t personId = ActingPersonId(ctx, transaction, args);
    if (personId <= 0) {
        return 1;
    }
    std::vector<Events::SubmissionView> submissions =
        Events::SubmissionHelper(ctx.databaseHelper).GetMySubmissions(transaction, personId);
    std::cout << Color::kBold << "Submissions by person " << personId << Color::kReset << "\n";
    if (submissions.empty()) {
        Color::PrintDim("(none — make one with submit-event)");
        return 0;
    }
    for (const Events::SubmissionView& submission : submissions) {
        std::cout << Color::kBoldCyan << "Submission " << submission.id << Color::kReset
                  << "  sent " << FormatUtc(submission.createdUs)
                  << ", " << submission.relationship
                  << (submission.regularThing ? ", a regular thing" : "") << "\n";
        if (!submission.rejectReason.empty()) {
            std::cout << "  Reason: " << submission.rejectReason << "\n";
        }
        if (!submission.messageToSubmitter.empty()) {
            std::cout << "  Message: " << submission.messageToSubmitter << "\n";
        }
        for (const Events::SubmissionDateView& date : submission.dates) {
            std::cout << "  " << std::left
                      << std::setw(8) << date.eventId
                      << std::setw(22) << FormatUtc(date.startsAtUs)
                      << std::setw(12) << date.state
                      << date.title << "\n";
        }
    }
    Color::PrintDim(std::to_string(submissions.size()) + " submission(s), newest first.");
    return 0;
}

void PrintSettings(const Events::SubmissionSettings& settings) {
    auto limit = [](std::string_view name, int64_t value, const Events::SubmissionLimitRange& range) {
        std::cout << "  " << std::left << std::setw(26) << name << value
                  << Color::kDim << "  (default " << range.defaultValue << ", "
                  << range.min << ".." << range.max << ")" << Color::kReset << "\n";
    };
    std::cout << Color::kBold << "Submissions: " << Color::kReset
              << (settings.submissionsPaused ? std::string(Color::kBoldYellow) + "PAUSED"
                                             : std::string(Color::kGreen) + "open")
              << Color::kReset << "\n";
    limit(Events::kSubmissionLimitMaxPendingDates, settings.maxPendingDates,
          Events::kMaxPendingDatesRange);
    limit(Events::kSubmissionLimitMaxSubmissionsPerDay, settings.maxSubmissionsPerDay,
          Events::kMaxSubmissionsPerDayRange);
}

int RunSubmissionSettings(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    // Types here; ranges are SaveSubmissionSettings' call, as on the page.
    Events::SubmissionSettingsUpdate update;
    auto readLimit = [&](const char* name, std::optional<int64_t>& out) {
        if (Arg(args, name).empty()) return true;
        out = IntArg(args, name);
        if (!out) {
            Color::PrintError(std::string("--") + name + " must be a whole number");
            return false;
        }
        return true;
    };
    if (!readLimit("max_pending_dates", update.maxPendingDates)
        || !readLimit("max_submissions_per_day", update.maxSubmissionsPerDay)) {
        return 1;
    }
    std::string paused = Arg(args, "paused");
    if (!paused.empty()) {
        if (paused != "true" && paused != "false") {
            Color::PrintError("--paused must be true or false");
            return 1;
        }
        update.submissionsPaused = paused == "true";
    }

    if (!update.maxPendingDates && !update.maxSubmissionsPerDay && !update.submissionsPaused) {
        PrintSettings(Events::LoadSubmissionSettings(*ctx.secretsHelper, transaction));
        Color::PrintDim("Change them with --paused=true|false, --max_pending_dates=N, "
                        "--max_submissions_per_day=N.");
        return 0;
    }
    Events::SaveSettingsResult saved =
        Events::SaveSubmissionSettings(*ctx.secretsHelper, transaction, update);
    if (!saved.ok) {
        Color::PrintError(saved.errorCode + ": --" + saved.field
                          + " is outside its allowed range; nothing was changed");
        return 1;
    }
    Color::PrintSuccess("Saved.");
    PrintSettings(saved.settings);
    return 0;
}

}  // namespace

void RegisterSubmissionCommands(CommandRegistry& registry) {
    registry.Register({
        "submit-event", "se", "Submissions",
        "Submit an event as a member (created, verified, if the email is new), "
        "held to every check and limit the form is",
        {{ "--as", "Email of the submitter", true },
         { "--title", "Event title; \"Demo: \" is added if missing", true },
         { "--starts_in_hours", "Hours from now the first date starts (1 or more passes)", true },
         { "--dates", "How many weekly dates (default 1, up to 12)", false },
         { "--venue_id", "Venue id", false }},
        RunSubmitEvent
    });

    registry.Register({
        "list-submissions", "ls", "Submissions",
        "List a person's submissions and each date's state, as My submissions shows them",
        {{ "--as", "Email of the submitter (default: the logged-in person)", false }},
        RunListSubmissions
    });

    registry.Register({
        "submission-settings", "ss", "Submissions",
        "Show the submission limits and pause switch, or change them",
        {{ "--paused", "true closes the submit form for everyone; false reopens it", false },
         { "--max_pending_dates", "Dates one person may have waiting (1..100)", false },
         { "--max_submissions_per_day", "Submissions one person may send in 24 hours (1..50)", false }},
        RunSubmissionSettings
    });
}

}  // namespace TestHelper
