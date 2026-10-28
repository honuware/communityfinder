#include "series.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/freshness.h"
#include "db_schema/series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeSeries(std::string_view title, std::string_view slug) {
    return {
        { std::string(DbSchema::kSeriesTitle), std::string(title) },
        { std::string(DbSchema::kSeriesSlug), std::string(slug) },
    };
}

TEST(SeriesTest, AddAndGetByIdAndSlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetByIdAndSlug", [&](Transaction& transaction) {
        Series series(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSeries("T4T", "t4t");
        values[std::string(DbSchema::kSeriesCadenceText)] = "2nd Saturdays";
        int64_t id = series.AddSeries(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable byId = series.GetSeries(transaction, id);
        ASSERT_FALSE(byId.empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kSeriesCadenceText)), "2nd Saturdays");
        EXPECT_EQ(byId.at(std::string(DbSchema::kSeriesStatus)), DbSchema::kFreshnessStatusOpen);
        EXPECT_EQ(byId.at(std::string(DbSchema::kSeriesVolatilityTier)), "monthly");

        KeyValueTable bySlug = series.GetSeriesBySlug(transaction, "t4t");
        ASSERT_FALSE(bySlug.empty());
        EXPECT_EQ(bySlug.at(std::string(DbSchema::kSeriesId)), StringFromInt(id));
    });
}

TEST(SeriesTest, MissesAreEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissesAreEmpty", [&](Transaction& transaction) {
        Series series(testDb.GetDatabaseHelper());
        EXPECT_TRUE(series.GetSeries(transaction, 999999).empty());
        EXPECT_TRUE(series.GetSeriesBySlug(transaction, "no-such-series").empty());
    });
}

TEST(SeriesTest, GetActiveSeriesDropsClosedAndMovedAndSortsByTitle) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetActiveSeriesDropsClosedAndMovedAndSortsByTitle", [&](Transaction& transaction) {
        Series series(testDb.GetDatabaseHelper());
        auto addWithStatus = [&](std::string_view title, std::string_view slug,
                                 std::string_view status) {
            KeyValueTable values = MakeSeries(title, slug);
            values[std::string(DbSchema::kSeriesStatus)] = std::string(status);
            return series.AddSeries(transaction, values);
        };
        int64_t unverified = addWithStatus(
            "Underwear Night", "underwear-night", DbSchema::kFreshnessStatusUnverified);
        int64_t open = addWithStatus("Bear Happy Hour", "bear-happy-hour",
                                     DbSchema::kFreshnessStatusOpen);
        int64_t seasonal = addWithStatus("Pride Picnic", "pride-picnic",
                                         DbSchema::kFreshnessStatusSeasonal);
        addWithStatus("Closed Night", "closed-night", DbSchema::kFreshnessStatusClosed);
        addWithStatus("Moved Night", "moved-night", DbSchema::kFreshnessStatusMoved);

        KeyValueTableArray rows = series.GetActiveSeries(transaction);
        ASSERT_EQ(rows.size(), 3u);
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kSeriesId)), StringFromInt(open));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kSeriesId)), StringFromInt(seasonal));
        EXPECT_EQ(rows[2].at(std::string(DbSchema::kSeriesId)), StringFromInt(unverified));

        // A closed series keeps its page.
        EXPECT_FALSE(series.GetSeriesBySlug(transaction, "closed-night").empty());
    });
}

TEST(SeriesTest, UpdateBumpsUpdatedUs) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UpdateBumpsUpdatedUs", [&](Transaction& transaction) {
        Series series(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSeries("T4T", "t4t");
        // updated_us = 0 makes the bump observable (now_us() is fixed per transaction).
        values[std::string(DbSchema::kSeriesUpdatedUs)] = "0";
        int64_t id = series.AddSeries(transaction, values);

        series.UpdateSeries(transaction, id, {
            { std::string(DbSchema::kSeriesStatus), std::string(DbSchema::kFreshnessStatusClosed) },
            { std::string(DbSchema::kSeriesLastVerifiedAtUs), "1700000000000000" },
        });

        KeyValueTable row = series.GetSeries(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kSeriesStatus)), DbSchema::kFreshnessStatusClosed);
        EXPECT_EQ(row.at(std::string(DbSchema::kSeriesLastVerifiedAtUs)), "1700000000000000");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kSeriesUpdatedUs))), 0);
    });
}

TEST(SeriesTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        Series series(testDb.GetDatabaseHelper());
        series.AddSeries(transaction, MakeSeries("T4T", "t4t"));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(series.AddSeries(transaction, MakeSeries("T4T again", "t4t")),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
