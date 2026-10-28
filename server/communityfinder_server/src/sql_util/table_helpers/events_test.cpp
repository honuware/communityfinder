#include "events.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "event_sources.h"
#include "series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeEvent(std::string_view title, int64_t startsAtUs) {
    return {
        { std::string(DbSchema::kEventsTitle), std::string(title) },
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs) },
    };
}

KeyValueTable MakeIngestedEvent(std::string_view title, int64_t startsAtUs,
                                int64_t sourceId, std::string_view externalKey) {
    KeyValueTable values = MakeEvent(title, startsAtUs);
    values[std::string(DbSchema::kEventsSourceId)] = StringFromInt(sourceId);
    values[std::string(DbSchema::kEventsExternalKey)] = std::string(externalKey);
    values[std::string(DbSchema::kEventsOrigin)] = std::string(DbSchema::kEventsOriginScanned);
    return values;
}

KeyValueTable WithStatus(KeyValueTable values, std::string_view status) {
    values[std::string(DbSchema::kEventsStatus)] = std::string(status);
    return values;
}

int64_t AddSource(Transaction& transaction, DatabaseHelper databaseHelper,
                  std::string_view name) {
    EventSources sources(databaseHelper);
    return sources.AddSource(transaction, {
        { std::string(DbSchema::kEventSourcesName), std::string(name) },
        { std::string(DbSchema::kEventSourcesKind),
          std::string(DbSchema::kEventSourcesKindIcsFeed) },
    });
}

std::string IdOf(const KeyValueTable& row) {
    return row.at(std::string(DbSchema::kEventsId));
}

TEST(EventsTest, AddAndGetWithTheDefaults) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetWithTheDefaults", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeEvent("Bear Happy Hour", 1'800'000'000'000'000);
        values[std::string(DbSchema::kEventsCostText)] = "Free";
        int64_t id = events.AddEvent(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = events.GetEvent(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsTitle)), "Bear Happy Hour");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStartsAtUs)), "1800000000000000");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsCostText)), "Free");
        // Hand-entered by default, and waiting for review.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), DbSchema::kEventsStatusPending);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsOrigin)), DbSchema::kEventsOriginManual);
        // Nullable columns left out read back as "".
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsEndsAtUs)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsSourceId)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsExternalKey)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsReviewedByPersonId)).empty());
    });
}

TEST(EventsTest, GetEventNotFoundIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetEventNotFoundIsEmpty", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        EXPECT_TRUE(events.GetEvent(transaction, 999999).empty());
    });
}

TEST(EventsTest, GetEventBySourceAndExternalKeyMatchesBothParts) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventBySourceAndExternalKeyMatchesBothParts", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t kremwerk = AddSource(transaction, databaseHelper, "Kremwerk");
        int64_t wildrose = AddSource(transaction, databaseHelper, "Wildrose");
        int64_t id = events.AddEvent(
            transaction, MakeIngestedEvent("Night A", 1000, kremwerk, "uid-1"));
        // Same key from another source is a different item.
        int64_t otherSource = events.AddEvent(
            transaction, MakeIngestedEvent("Night B", 1000, wildrose, "uid-1"));

        KeyValueTable match = events.GetEventBySourceAndExternalKey(transaction, kremwerk, "uid-1");
        ASSERT_FALSE(match.empty());
        EXPECT_EQ(IdOf(match), StringFromInt(id));
        EXPECT_EQ(IdOf(events.GetEventBySourceAndExternalKey(transaction, wildrose, "uid-1")),
                  StringFromInt(otherSource));

        EXPECT_TRUE(events.GetEventBySourceAndExternalKey(transaction, kremwerk, "uid-2").empty());
    });
}

TEST(EventsTest, GetEventsInRangeIsHalfOpenAndFiltersByStatus) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsInRangeIsHalfOpenAndFiltersByStatus", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string approved(DbSchema::kEventsStatusApproved);
        int64_t atStart = events.AddEvent(transaction, WithStatus(MakeEvent("At from", 1000), approved));
        int64_t laterB = events.AddEvent(transaction, WithStatus(MakeEvent("Later B", 1500), approved));
        int64_t laterA = events.AddEvent(transaction, WithStatus(MakeEvent("Later A", 1200), approved));
        // Same start as laterB: id breaks the tie.
        int64_t tied = events.AddEvent(transaction, WithStatus(MakeEvent("Tied", 1500), approved));
        events.AddEvent(transaction, WithStatus(MakeEvent("Before", 999), approved));
        events.AddEvent(transaction, WithStatus(MakeEvent("At to", 2000), approved));
        events.AddEvent(transaction, MakeEvent("Pending inside", 1300));

        KeyValueTableArray rows = events.GetEventsInRange(transaction, 1000, 2000, approved);
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(atStart));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(laterA));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(laterB));
        EXPECT_EQ(IdOf(rows[3]), StringFromInt(tied));

        KeyValueTableArray pending = events.GetEventsInRange(
            transaction, 1000, 2000, DbSchema::kEventsStatusPending);
        ASSERT_EQ(pending.size(), 1u);
        EXPECT_EQ(pending[0].at(std::string(DbSchema::kEventsTitle)), "Pending inside");
    });
}

TEST(EventsTest, GetEventsBySeriesReturnsEveryStatusInStartOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsBySeriesReturnsEveryStatusInStartOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        Series series(databaseHelper);
        auto addSeries = [&](std::string_view slug) {
            return series.AddSeries(transaction, {
                { std::string(DbSchema::kSeriesTitle), std::string(slug) },
                { std::string(DbSchema::kSeriesSlug), std::string(slug) },
            });
        };
        int64_t t4t = addSeries("t4t");
        int64_t other = addSeries("other");
        auto addIn = [&](std::string_view title, int64_t startsAtUs, int64_t seriesId,
                         std::string_view status) {
            KeyValueTable values = WithStatus(MakeEvent(title, startsAtUs), status);
            values[std::string(DbSchema::kEventsSeriesId)] = StringFromInt(seriesId);
            return events.AddEvent(transaction, values);
        };
        int64_t march = addIn("March", 3000, t4t, DbSchema::kEventsStatusApproved);
        int64_t january = addIn("January", 1000, t4t, DbSchema::kEventsStatusArchived);
        int64_t february = addIn("February", 2000, t4t, DbSchema::kEventsStatusPending);
        addIn("Not this series", 1500, other, DbSchema::kEventsStatusApproved);
        events.AddEvent(transaction, MakeEvent("One-off", 1500));

        KeyValueTableArray rows = events.GetEventsBySeries(transaction, t4t);
        ASSERT_EQ(rows.size(), 3u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(january));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(february));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(march));
    });
}

TEST(EventsTest, GetPendingEventsIsTheReviewQueueSoonestFirst) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetPendingEventsIsTheReviewQueueSoonestFirst", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        int64_t later = events.AddEvent(transaction, MakeEvent("Later", 2000));
        int64_t sooner = events.AddEvent(transaction, MakeEvent("Sooner", 1000));
        events.AddEvent(transaction,
                        WithStatus(MakeEvent("Approved", 500), DbSchema::kEventsStatusApproved));
        events.AddEvent(transaction,
                        WithStatus(MakeEvent("Rejected", 500), DbSchema::kEventsStatusRejected));

        KeyValueTableArray rows = events.GetPendingEvents(transaction);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(sooner));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(later));
    });
}

TEST(EventsTest, UpdateBumpsUpdatedUsAndCanClearAColumn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearAColumn", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeEvent("Bear Happy Hour", 1000);
        values[std::string(DbSchema::kEventsEndsAtUs)] = "2000";
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kEventsUpdatedUs)] = "0";
        int64_t id = events.AddEvent(transaction, values);

        // The review quartet, as EventHelper::ReviewEvent will stamp it.
        events.UpdateEvent(transaction, id, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusApproved) },
            { std::string(DbSchema::kEventsReviewedByPersonId), "42" },
            { std::string(DbSchema::kEventsReviewedUs), "1500" },
            { std::string(DbSchema::kEventsReviewNotes), "Checked the venue page" },
            { std::string(DbSchema::kEventsEndsAtUs), "NULL" },
        });

        KeyValueTable row = events.GetEvent(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), DbSchema::kEventsStatusApproved);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedByPersonId)), "42");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedUs)), "1500");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewNotes)), "Checked the venue page");
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsEndsAtUs)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventsUpdatedUs))), 0);
    });
}

// Postgres treats NULLs as distinct, so neither kind of hand-entered event
// trips the (source_id, external_key) rule: no source at all, or a source with
// no key (an event credited to a source but typed in by hand).
TEST(EventsTest, HandEnteredEventsNeverCollide) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("HandEnteredEventsNeverCollide", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t sourceId = AddSource(transaction, databaseHelper, "Kremwerk");

        EXPECT_GT(events.AddEvent(transaction, MakeEvent("No source 1", 1000)), 0);
        EXPECT_GT(events.AddEvent(transaction, MakeEvent("No source 2", 1000)), 0);

        KeyValueTable credited = MakeEvent("Credited 1", 1000);
        credited[std::string(DbSchema::kEventsSourceId)] = StringFromInt(sourceId);
        EXPECT_GT(events.AddEvent(transaction, credited), 0);
        credited[std::string(DbSchema::kEventsTitle)] = "Credited 2";
        EXPECT_GT(events.AddEvent(transaction, credited), 0);
    });
}

// The ingestion idempotency anchor: a source can't hold the same item twice.
TEST(EventsTest, DuplicateSourceAndExternalKeyThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSourceAndExternalKeyThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t sourceId = AddSource(transaction, databaseHelper, "Kremwerk");
        events.AddEvent(transaction, MakeIngestedEvent("Night", 1000, sourceId, "uid-1"));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(
            events.AddEvent(transaction,
                            MakeIngestedEvent("Night again", 2000, sourceId, "uid-1")),
            std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
