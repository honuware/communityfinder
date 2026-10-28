#include "event_sources.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/event_sources.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeSource(std::string_view name) {
    return {
        { std::string(DbSchema::kEventSourcesName), std::string(name) },
        { std::string(DbSchema::kEventSourcesKind),
          std::string(DbSchema::kEventSourcesKindIcsFeed) },
    };
}

TEST(EventSourcesTest, AddAndGet) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGet", [&](Transaction& transaction) {
        EventSources sources(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSource("Kremwerk calendar");
        values[std::string(DbSchema::kEventSourcesUrl)] = "https://example.com/cal.ics";
        int64_t id = sources.AddSource(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = sources.GetSource(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesName)), "Kremwerk calendar");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesKind)),
                  DbSchema::kEventSourcesKindIcsFeed);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesEnabled)), "t");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesVolatilityTier)), "weekly");
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventSourcesLastScannedAtUs)).empty());
    });
}

TEST(EventSourcesTest, GetSourceNotFoundIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetSourceNotFoundIsEmpty", [&](Transaction& transaction) {
        EventSources sources(testDb.GetDatabaseHelper());
        EXPECT_TRUE(sources.GetSource(transaction, 999999).empty());
    });
}

TEST(EventSourcesTest, GetEnabledSourcesSkipsDisabledAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEnabledSourcesSkipsDisabledAndSortsByName", [&](Transaction& transaction) {
        EventSources sources(testDb.GetDatabaseHelper());
        int64_t wildrose = sources.AddSource(transaction, MakeSource("Wildrose"));
        int64_t kremwerk = sources.AddSource(transaction, MakeSource("Kremwerk"));
        KeyValueTable paused = MakeSource("Paused feed");
        paused[std::string(DbSchema::kEventSourcesEnabled)] = "false";
        sources.AddSource(transaction, paused);

        KeyValueTableArray rows = sources.GetEnabledSources(transaction);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kEventSourcesId)), StringFromInt(kremwerk));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kEventSourcesId)), StringFromInt(wildrose));
    });
}

TEST(EventSourcesTest, UpdateBumpsUpdatedUs) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UpdateBumpsUpdatedUs", [&](Transaction& transaction) {
        EventSources sources(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSource("Kremwerk");
        // updated_us = 0 makes the bump observable (now_us() is fixed per transaction).
        values[std::string(DbSchema::kEventSourcesUpdatedUs)] = "0";
        int64_t id = sources.AddSource(transaction, values);

        sources.UpdateSource(transaction, id, {
            { std::string(DbSchema::kEventSourcesEnabled), "false" },
            { std::string(DbSchema::kEventSourcesScanHints), "Lineup is on the /events page" },
        });

        KeyValueTable row = sources.GetSource(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesEnabled)), "f");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesScanHints)),
                  "Lineup is on the /events page");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventSourcesUpdatedUs))), 0);
    });
}

TEST(EventSourcesTest, TouchLastScannedSetsOnlyTheScanTime) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TouchLastScannedSetsOnlyTheScanTime", [&](Transaction& transaction) {
        EventSources sources(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSource("Kremwerk");
        values[std::string(DbSchema::kEventSourcesUpdatedUs)] = "0";
        int64_t id = sources.AddSource(transaction, values);

        sources.TouchLastScanned(transaction, id, 1'700'000'000'000'000);

        KeyValueTable row = sources.GetSource(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesLastScannedAtUs)),
                  "1700000000000000");
        // A scan is not an edit.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSourcesUpdatedUs)), "0");
    });
}

TEST(EventSourcesTest, UnknownKindIsRejected) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownKindIsRejected", [&](Transaction& transaction) {
        EventSources sources(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSource("Some feed");
        values[std::string(DbSchema::kEventSourcesKind)] = "rss";
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(sources.AddSource(transaction, values), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
