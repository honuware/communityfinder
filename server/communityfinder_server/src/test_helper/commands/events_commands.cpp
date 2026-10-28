#include "commands/events_commands.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/event_helper.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/event_sources.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/venues.h"
#include "util/types.h"

#include "command_runner.h"
#include "console_colors.h"

namespace TestHelper {

namespace {

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

// The feed ingest-demo files its batch under — found by name, or created.
constexpr std::string_view kDemoSourceName = "test_helper demo feed";
constexpr std::string_view kReviewNote = "approved by test_helper";

std::string Arg(const Args& args, const char* name) {
    auto it = args.find(name);
    return it != args.end() ? it->second : std::string();
}

bool HasFlag(const Args& args, const char* name) {
    return Arg(args, name) == "true";
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
    if (slugs.empty()) return "-";
    std::string joined;
    for (const std::string& slug : slugs) {
        if (!joined.empty()) joined += ",";
        joined += slug;
    }
    return joined;
}

// Next Monday, 00:00 UTC. Day 0 of the Unix epoch was a Thursday.
int64_t NextMondayUs(int64_t nowUs) {
    int64_t day = nowUs / Events::kUsPerDay;
    int64_t daysSinceMonday = (day + 3) % 7;
    return (day - daysSinceMonday + 7) * Events::kUsPerDay;
}

int64_t DemoSourceId(TestHelperContext& ctx, Transaction& transaction) {
    TableHelpers::EventSources sources(ctx.databaseHelper);
    for (const KeyValueTable& source : sources.GetEnabledSources(transaction)) {
        if (source.at(std::string(DbSchema::kEventSourcesName)) == kDemoSourceName) {
            return std::stoll(source.at(std::string(DbSchema::kEventSourcesId)));
        }
    }
    return sources.AddSource(transaction, {
        { std::string(DbSchema::kEventSourcesName), std::string(kDemoSourceName) },
        { std::string(DbSchema::kEventSourcesKind), std::string(DbSchema::kEventSourcesKindIcsFeed) },
    });
}

// The canned batch: six evenings in the week after `anchorUs` (03:00 UTC is
// 7–8pm in Seattle). Anchored to next Monday, the batch is identical on every
// run in the same week, so a re-run reports all six unchanged. `changed` sends
// the same six with three edits — a new description (minor: updated, status
// kept), a start an hour later (major: an approved event goes back to
// pending), and a new title (minor). The last item names a category that
// doesn't exist, to show it reported as ignored.
std::vector<Events::IngestItem> DemoBatch(int64_t sourceId, int64_t anchorUs, bool changed) {
    auto item = [&](std::string_view key, std::string_view title, int64_t day,
                    std::vector<std::string> categorySlugs,
                    std::vector<std::string> sceneTagSlugs) {
        Events::IngestItem result;
        result.sourceId = sourceId;
        result.externalKey = "demo-" + std::string(key);
        result.title = "Demo: " + std::string(title);
        result.startsAtUs = anchorUs + (day * 24 + 3) * kUsPerHour;
        result.description = std::string("A demo event from test_helper ingest-demo.");
        result.categorySlugs = std::move(categorySlugs);
        result.sceneTagSlugs = std::move(sceneTagSlugs);
        return result;
    };
    std::vector<Events::IngestItem> batch{
        item("drag-show", "Drag Show", 0, { "drag" }, {}),
        item("bear-happy-hour", "Bear Happy Hour", 1, { "bar" }, { "bear" }),
        item("leather-night", "Leather Night", 2, { "nightlife" }, { "leather" }),
        item("sober-social", "Sober Social", 3, { "community" }, { "sober" }),
        item("pride-picnic", "Pride Picnic", 5, { "pride", "outdoors" }, {}),
        item("geek-trivia", "Geek Trivia", 6, { "community", "not-a-category" }, { "geek" }),
    };
    batch[4].locationText = std::string("Volunteer Park");
    if (changed) {
        batch[1].description = std::string("A demo event from test_helper ingest-demo, now with a DJ.");
        batch[2].startsAtUs += kUsPerHour;
        batch[3].title = "Demo: Sober Social & Games";
    }
    return batch;
}

int RunCreateVenue(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::string name = Arg(args, "name");
    if (name.empty()) {
        Color::PrintError("--name is required");
        return 1;
    }
    KeyValueTable values{ { std::string(DbSchema::kVenuesName), name } };
    std::string neighborhood = Arg(args, "neighborhood");
    if (!neighborhood.empty()) {
        KeyValueTable row = TableHelpers::Neighborhoods(ctx.databaseHelper)
                                .GetBySlug(transaction, neighborhood);
        if (row.empty()) {
            Color::PrintError("No neighborhood with slug " + neighborhood);
            return 1;
        }
        values[std::string(DbSchema::kVenuesNeighborhoodId)] =
            row.at(std::string(DbSchema::kNeighborhoodsId));
    }

    int64_t venueId = TableHelpers::Venues(ctx.databaseHelper).AddVenue(transaction, values);
    Color::PrintSuccess("Created venue " + StringFromInt(venueId) + ": " + name
                        + (neighborhood.empty() ? "" : " (" + neighborhood + ")"));
    return 0;
}

int RunCreateEvent(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
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
    std::string status = Arg(args, "status");
    if (status.empty()) {
        status = DbSchema::kEventsStatusPending;
    }
    if (status != DbSchema::kEventsStatusPending && status != DbSchema::kEventsStatusApproved) {
        Color::PrintError("--status must be pending or approved");
        return 1;
    }
    if (status == DbSchema::kEventsStatusApproved && ctx.currentPersonId <= 0) {
        Color::PrintError("Log in first: approving records who reviewed it");
        return 1;
    }

    KeyValueTable values{ { std::string(DbSchema::kEventsTitle), title } };
    std::string venueText = Arg(args, "venue_id");
    if (!venueText.empty()) {
        std::optional<int64_t> venueId = IntArg(args, "venue_id");
        if (!venueId
            || TableHelpers::Venues(ctx.databaseHelper).GetVenue(transaction, *venueId).empty()) {
            Color::PrintError("No venue with id " + venueText);
            return 1;
        }
        values[std::string(DbSchema::kEventsVenueId)] = StringFromInt(*venueId);
    }
    int64_t offsetUs = *hours * kUsPerHour;
    int64_t startsAtUs = NowUs(transaction) + (HasFlag(args, "past") ? -offsetUs : offsetUs);
    values[std::string(DbSchema::kEventsStartsAtUs)] = StringFromInt(startsAtUs);
    if (ctx.currentPersonId > 0) {
        values[std::string(DbSchema::kEventsCreatedByPersonId)] = StringFromInt(ctx.currentPersonId);
    }

    // A hand-entered event, as the admin editor makes one: no source, origin
    // manual (the column default), pending until reviewed.
    int64_t eventId = TableHelpers::Events(ctx.databaseHelper).AddEvent(transaction, values);
    if (status == DbSchema::kEventsStatusApproved) {
        Events::ReviewResult review = Events::EventHelper(ctx.databaseHelper).ReviewEvent(
            transaction, eventId, Events::ReviewAction::Approve, ctx.currentPersonId, kReviewNote);
        if (!review.ok) {
            Color::PrintError("Created event " + StringFromInt(eventId)
                              + " but could not approve it: " + review.errorCode);
            return 1;
        }
    }
    Color::PrintSuccess("Created event " + StringFromInt(eventId) + ": " + title + " ("
                        + status + ", starts " + FormatUtc(startsAtUs)
                        + (venueText.empty() ? "" : ", venue " + venueText) + ")");
    return 0;
}

int RunIngestDemo(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    bool approve = HasFlag(args, "approve");
    if (approve && ctx.currentPersonId <= 0) {
        Color::PrintError("Log in first: approving records who reviewed it");
        return 1;
    }

    int64_t sourceId = DemoSourceId(ctx, transaction);
    std::vector<Events::IngestItem> batch =
        DemoBatch(sourceId, NextMondayUs(NowUs(transaction)), HasFlag(args, "changed"));
    Events::EventHelper helper(ctx.databaseHelper);
    Events::IngestResult result = helper.IngestEvents(transaction, batch);
    if (!result.ok) {
        Color::PrintError("Ingest failed on item " + StringFromInt(result.errorItemIndex)
                          + ": " + result.errorCode);
        return 1;
    }

    std::cout << Color::kBold << "Feed: " << Color::kReset << kDemoSourceName
              << " (source " << sourceId << ")\n";
    std::cout << Color::kBoldCyan << std::left
              << std::setw(24) << "Key"
              << std::setw(8) << "Event"
              << std::setw(22) << "Outcome"
              << std::setw(10) << "Status"
              << "Ignored slugs"
              << Color::kReset << "\n";
    TableHelpers::Events events(ctx.databaseHelper);
    std::map<std::string, int> counts;
    for (size_t index = 0; index < result.items.size(); ++index) {
        const Events::IngestItemResult& item = result.items[index];
        std::string status = events.GetEvent(transaction, item.eventId)
                                 .at(std::string(DbSchema::kEventsStatus));
        if (approve && status == DbSchema::kEventsStatusPending) {
            Events::ReviewResult review = helper.ReviewEvent(
                transaction, item.eventId, Events::ReviewAction::Approve,
                ctx.currentPersonId, kReviewNote);
            if (review.ok) status = review.status;
        }
        std::vector<std::string> ignored = item.ignoredCategorySlugs;
        ignored.insert(ignored.end(), item.ignoredSceneTagSlugs.begin(),
                       item.ignoredSceneTagSlugs.end());
        if (!item.ignoredSeriesSlug.empty()) ignored.push_back(item.ignoredSeriesSlug);
        ++counts[item.outcome];
        std::cout << std::setw(24) << batch[index].externalKey
                  << std::setw(8) << item.eventId
                  << std::setw(22) << item.outcome
                  << std::setw(10) << status
                  << JoinSlugs(ignored) << "\n";
    }

    std::string summary;
    for (const auto& [outcome, count] : counts) {
        if (!summary.empty()) summary += ", ";
        summary += outcome + " " + std::to_string(count);
    }
    Color::PrintSuccess(summary);
    Color::PrintDim("Re-run to see all unchanged; --changed sends three edits; "
                    "--approve approves whatever is pending.");
    return 0;
}

int RunListUpcoming(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    Events::UpcomingQuery query;
    query.categorySlug = Arg(args, "category");
    query.sceneTagSlug = Arg(args, "scene");
    query.neighborhoodSlug = Arg(args, "neighborhood");
    Events::UpcomingEvents upcoming =
        Events::EventHelper(ctx.databaseHelper).GetUpcomingPublic(transaction, query);

    std::cout << Color::kBold << "Approved events " << Color::kReset
              << FormatUtc(upcoming.window.fromUs) << " .. "
              << FormatUtc(upcoming.window.toUs) << "\n";
    if (upcoming.events.empty()) {
        Color::PrintDim("(none — an event shows once approved: create-event "
                        "--status=approved, or ingest-demo --approve)");
        return 0;
    }
    std::cout << Color::kBoldCyan << std::left
              << std::setw(7) << "ID"
              << std::setw(22) << "Starts"
              << std::setw(32) << "Title"
              << std::setw(30) << "Where"
              << std::setw(24) << "Categories"
              << "Scene"
              << Color::kReset << "\n";
    for (const Events::PublicEvent& event : upcoming.events) {
        std::string where = !event.venueName.empty() ? event.venueName
            : !event.locationText.empty()            ? event.locationText
                                                     : "-";
        if (!event.neighborhoodSlug.empty()) where += " (" + event.neighborhoodSlug + ")";
        std::cout << std::setw(7) << event.id
                  << std::setw(22) << FormatUtc(event.startsAtUs)
                  << std::setw(32) << event.title
                  << std::setw(30) << where
                  << std::setw(24) << JoinSlugs(event.categorySlugs)
                  << JoinSlugs(event.sceneTagSlugs) << "\n";
    }
    Color::PrintDim(std::to_string(upcoming.events.size()) + " event(s).");
    return 0;
}

}  // namespace

void RegisterEventsCommands(CommandRegistry& registry) {
    registry.Register({
        "create-venue", "cv", "Events",
        "Create a venue, optionally in a neighborhood",
        {{ "--name", "Venue name (quote it if it has spaces)", true },
         { "--neighborhood", "Neighborhood slug, e.g. capitol-hill", false }},
        RunCreateVenue
    });

    registry.Register({
        "create-event", "ce", "Events",
        "Create a hand-entered event starting N hours from now (pending unless approved)",
        {{ "--title", "Event title (quote it if it has spaces)", true },
         { "--starts_in_hours", "Hours from now it starts (0 = now)", true },
         { "--venue_id", "Venue id", false },
         { "--status", "pending (default) or approved", false },
         { "--past", "Start that many hours ago instead", false }},
        RunCreateEvent
    });

    registry.Register({
        "ingest-demo", "demo", "Events",
        "Ingest a canned 6-event batch from a demo feed and show each outcome",
        {{ "--changed", "Send the batch with three edits (updated / back to pending)", false },
         { "--approve", "Approve every demo event left pending", false }},
        RunIngestDemo
    });

    registry.Register({
        "list-upcoming", "lu", "Events",
        "List the approved upcoming events, as /api/events/upcoming serves them",
        {{ "--category", "Category slug filter", false },
         { "--scene", "Scene tag slug filter", false },
         { "--neighborhood", "Neighborhood slug filter", false }},
        RunListUpcoming
    });
}

}  // namespace TestHelper
