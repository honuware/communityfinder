#include "commands/events_commands.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/events/event_helper.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/event_sources.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/venues.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

#include "command_runner.h"

// EV1 §6 — the events simulation commands, run through the registry against
// the test database. The helper's own context is built by hand: the commands
// only use its databaseHelper and current person.
namespace TestHelper {
namespace {

constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;
constexpr int64_t kCuratorId = 7;  // people ids aren't FKs on events (OQ8 (e))

const std::vector<std::string> kDemoKeys{
    "demo-drag-show", "demo-bear-happy-hour", "demo-leather-night",
    "demo-sober-social", "demo-pride-picnic", "demo-geek-trivia",
};

int RunCommand(TestHelperContext& ctx, Transaction& transaction, const std::string& command,
        const Args& args = {}) {
    const CommandInfo* info = ctx.registry.Find(command);
    EXPECT_NE(info, nullptr) << command;
    return info != nullptr ? info->execute(ctx, transaction, args) : -1;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

int64_t EventCount(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        "SELECT count(*) FROM " + std::string(DbSchema::kEventsTable)));
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it != row.end() ? it->second : std::string();
}

// The demo feed's id, after checking there is exactly one.
int64_t DemoSourceId(Transaction& transaction, DatabaseHelper databaseHelper) {
    int64_t sourceId = 0;
    int found = 0;
    for (const KeyValueTable& source :
         TableHelpers::EventSources(databaseHelper).GetEnabledSources(transaction)) {
        if (Column(source, DbSchema::kEventSourcesName) == "test_helper demo feed") {
            sourceId = std::stoll(Column(source, DbSchema::kEventSourcesId));
            ++found;
        }
    }
    EXPECT_EQ(found, 1);
    return sourceId;
}

std::map<std::string, KeyValueTable> DemoEvents(Transaction& transaction,
                                                DatabaseHelper databaseHelper) {
    int64_t sourceId = DemoSourceId(transaction, databaseHelper);
    TableHelpers::Events events(databaseHelper);
    std::map<std::string, KeyValueTable> byKey;
    for (const std::string& key : kDemoKeys) {
        byKey[key] = events.GetEventBySourceAndExternalKey(transaction, sourceId, key);
        EXPECT_FALSE(byKey[key].empty()) << key;
    }
    return byKey;
}

const Events::PublicEvent* FindByTitle(const Events::UpcomingEvents& upcoming,
                                       const std::string& title) {
    for (const Events::PublicEvent& event : upcoming.events) {
        if (event.title == title) return &event;
    }
    return nullptr;
}

TEST(EventsCommandsTest, EachCommandIsRegisteredUnderEventsWithItsAlias) {
    CommandRegistry registry;
    RegisterEventsCommands(registry);
    for (const auto& [name, alias] : std::map<std::string, std::string>{
             { "create-venue", "cv" }, { "create-event", "ce" },
             { "ingest-demo", "demo" }, { "list-upcoming", "lu" } }) {
        const CommandInfo* byName = registry.Find(name);
        ASSERT_NE(byName, nullptr) << name;
        EXPECT_EQ(byName->category, "Events");
        EXPECT_EQ(registry.Find(alias), byName) << alias;
    }
}

TEST(EventsCommandsTest, IngestDemoIsIdempotentAndItsChangedPassHitsEachRule) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("IngestDemoIsIdempotentAndItsChangedPassHitsEachRule",
                            [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterEventsCommands(ctx.registry);
        ctx.currentPersonId = kCuratorId;

        // First run: six new events, approved, all in the public window.
        ASSERT_EQ(RunCommand(ctx, transaction, "ingest-demo", { { "approve", "true" } }), 0);
        std::map<std::string, KeyValueTable> first = DemoEvents(transaction, databaseHelper);
        for (const auto& [key, row] : first) {
            EXPECT_EQ(Column(row, DbSchema::kEventsStatus), "approved") << key;
            EXPECT_EQ(Column(row, DbSchema::kEventsOrigin), "scanned") << key;
            EXPECT_EQ(Column(row, DbSchema::kEventsReviewedByPersonId), StringFromInt(kCuratorId));
        }
        Events::EventHelper helper(databaseHelper);
        EXPECT_EQ(helper.GetUpcomingPublic(transaction, {}).events.size(), 6u);

        // A plain re-run writes nothing, and reuses the demo feed.
        ASSERT_EQ(RunCommand(ctx, transaction, "ingest-demo"), 0);
        std::map<std::string, KeyValueTable> second = DemoEvents(transaction, databaseHelper);
        for (const std::string& key : kDemoKeys) {
            EXPECT_EQ(Column(second[key], DbSchema::kEventsUpdatedUs),
                      Column(first[key], DbSchema::kEventsUpdatedUs)) << key;
            EXPECT_EQ(Column(second[key], DbSchema::kEventsStatus), "approved") << key;
        }

        // --changed: a description edit and a title edit keep the event
        // approved; a start an hour later sends it back for review.
        ASSERT_EQ(RunCommand(ctx, transaction, "ingest-demo", { { "changed", "true" } }), 0);
        std::map<std::string, KeyValueTable> third = DemoEvents(transaction, databaseHelper);
        const KeyValueTable& described = third["demo-bear-happy-hour"];
        EXPECT_NE(Column(described, DbSchema::kEventsDescription),
                  Column(first["demo-bear-happy-hour"], DbSchema::kEventsDescription));
        EXPECT_EQ(Column(described, DbSchema::kEventsStatus), "approved");
        const KeyValueTable& moved = third["demo-leather-night"];
        EXPECT_EQ(Column(moved, DbSchema::kEventsStatus), "pending");
        EXPECT_EQ(Column(moved, DbSchema::kEventsReviewNotes), Events::kReviewNoteChangedByRescan);
        EXPECT_EQ(std::stoll(Column(moved, DbSchema::kEventsStartsAtUs)),
                  std::stoll(Column(first["demo-leather-night"], DbSchema::kEventsStartsAtUs))
                      + kUsPerHour);
        const KeyValueTable& retitled = third["demo-sober-social"];
        EXPECT_EQ(Column(retitled, DbSchema::kEventsTitle), "Demo: Sober Social & Games");
        EXPECT_EQ(Column(retitled, DbSchema::kEventsStatus), "approved");
        for (const char* key : { "demo-drag-show", "demo-pride-picnic", "demo-geek-trivia" }) {
            EXPECT_EQ(Column(third[key], DbSchema::kEventsUpdatedUs),
                      Column(first[key], DbSchema::kEventsUpdatedUs)) << key;
        }
        EXPECT_EQ(EventCount(transaction), 6);
        EXPECT_EQ(RunCommand(ctx, transaction, "list-upcoming"), 0);
    });
}

TEST(EventsCommandsTest, ApprovingNeedsALoggedInPerson) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ApprovingNeedsALoggedInPerson", [&](Transaction& transaction) {
        TestHelperContext ctx(testDb.GetDatabaseHelper());
        RegisterEventsCommands(ctx.registry);

        EXPECT_EQ(RunCommand(ctx, transaction, "ingest-demo", { { "approve", "true" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "create-event",
                      { { "title", "Bear Night" }, { "starts_in_hours", "5" },
                        { "status", "approved" } }), 1);
        EXPECT_EQ(EventCount(transaction), 0);
    });
}

TEST(EventsCommandsTest, CreateEventStartsHoursFromNowAndCanBeApproved) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CreateEventStartsHoursFromNowAndCanBeApproved", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterEventsCommands(ctx.registry);
        ctx.currentPersonId = kCuratorId;

        int64_t beforeUs = NowUs(transaction);
        ASSERT_EQ(RunCommand(ctx, transaction, "create-event",
                      { { "title", "Bear Night" }, { "starts_in_hours", "5" },
                        { "status", "approved" } }), 0);
        int64_t afterUs = NowUs(transaction);
        Events::EventHelper helper(databaseHelper);
        Events::UpcomingEvents upcoming = helper.GetUpcomingPublic(transaction, {});
        const Events::PublicEvent* approved = FindByTitle(upcoming, "Bear Night");
        ASSERT_NE(approved, nullptr);
        EXPECT_GE(approved->startsAtUs, beforeUs + 5 * kUsPerHour);
        EXPECT_LE(approved->startsAtUs, afterUs + 5 * kUsPerHour);
        EXPECT_EQ(approved->origin, "manual");
        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, approved->id);
        EXPECT_EQ(Column(row, DbSchema::kEventsCreatedByPersonId), StringFromInt(kCuratorId));
        EXPECT_EQ(Column(row, DbSchema::kEventsReviewedByPersonId), StringFromInt(kCuratorId));
        EXPECT_EQ(Column(row, DbSchema::kEventsSourceId), "");

        // --past, no status: pending, and already started.
        ASSERT_EQ(RunCommand(ctx, transaction, "create-event",
                      { { "title", "Last Night" }, { "starts_in_hours", "2" },
                        { "past", "true" } }), 0);
        KeyValueTableArray pending = TableHelpers::Events(databaseHelper).GetPendingEvents(transaction);
        ASSERT_EQ(pending.size(), 1u);
        EXPECT_EQ(Column(pending[0], DbSchema::kEventsTitle), "Last Night");
        EXPECT_LT(std::stoll(Column(pending[0], DbSchema::kEventsStartsAtUs)), NowUs(transaction));
        EXPECT_EQ(FindByTitle(helper.GetUpcomingPublic(transaction, {}), "Last Night"), nullptr);
        EXPECT_EQ(RunCommand(ctx, transaction, "list-upcoming", { { "category", "drag" } }), 0);
    });
}

TEST(EventsCommandsTest, CreateEventRefusesBadInput) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CreateEventRefusesBadInput", [&](Transaction& transaction) {
        TestHelperContext ctx(testDb.GetDatabaseHelper());
        RegisterEventsCommands(ctx.registry);
        ctx.currentPersonId = kCuratorId;

        EXPECT_EQ(RunCommand(ctx, transaction, "create-event", { { "starts_in_hours", "5" } }), 1);
        for (const char* hours : { "", "soon", "5h", "-1" }) {
            EXPECT_EQ(RunCommand(ctx, transaction, "create-event",
                          { { "title", "X" }, { "starts_in_hours", hours } }), 1) << hours;
        }
        EXPECT_EQ(RunCommand(ctx, transaction, "create-event",
                      { { "title", "X" }, { "starts_in_hours", "5" }, { "status", "rejected" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "create-event",
                      { { "title", "X" }, { "starts_in_hours", "5" }, { "venue_id", "999999" } }), 1);
        EXPECT_EQ(EventCount(transaction), 0);
    });
}

TEST(EventsCommandsTest, CreateVenuePlacesItInItsNeighborhoodForEventsToUse) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CreateVenuePlacesItInItsNeighborhoodForEventsToUse", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterEventsCommands(ctx.registry);
        int64_t capitolHill = TableHelpers::Neighborhoods(databaseHelper).AddNeighborhood(transaction, {
            { std::string(DbSchema::kNeighborhoodsSlug), "capitol-hill" },
            { std::string(DbSchema::kNeighborhoodsDisplayName), "Capitol Hill" },
            { std::string(DbSchema::kNeighborhoodsRegion),
              std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
        });

        ASSERT_EQ(RunCommand(ctx, transaction, "create-venue",
                      { { "name", "The Cuff" }, { "neighborhood", "capitol-hill" } }), 0);
        KeyValueTableArray venues =
            TableHelpers::Venues(databaseHelper).GetVenuesByNeighborhood(transaction, capitolHill);
        ASSERT_EQ(venues.size(), 1u);
        EXPECT_EQ(Column(venues[0], DbSchema::kVenuesName), "The Cuff");
        std::string venueId = Column(venues[0], DbSchema::kVenuesId);

        ASSERT_EQ(RunCommand(ctx, transaction, "create-event",
                      { { "title", "Bear Night" }, { "starts_in_hours", "0" },
                        { "venue_id", venueId } }), 0);
        KeyValueTableArray pending = TableHelpers::Events(databaseHelper).GetPendingEvents(transaction);
        ASSERT_EQ(pending.size(), 1u);
        EXPECT_EQ(Column(pending[0], DbSchema::kEventsVenueId), venueId);

        EXPECT_EQ(RunCommand(ctx, transaction, "create-venue",
                      { { "name", "Nowhere" }, { "neighborhood", "atlantis" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "create-venue"), 1);
        EXPECT_EQ(TableHelpers::Venues(databaseHelper)
                      .GetVenuesByNeighborhood(transaction, capitolHill).size(), 1u);
    });
}

}  // namespace
}  // namespace TestHelper
