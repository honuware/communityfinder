#include "series_category_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "categories.h"
#include "db_schema/categories.h"
#include "db_schema/series.h"
#include "db_schema/series_category_assignments.h"
#include "series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

int64_t AddCategory(Transaction& transaction, DatabaseHelper databaseHelper,
                    std::string_view slug, int64_t sortOrder) {
    Categories categories(databaseHelper);
    return categories.AddCategory(transaction, {
        { std::string(DbSchema::kCategoriesSlug), std::string(slug) },
        { std::string(DbSchema::kCategoriesDisplayName), std::string(slug) },
        { std::string(DbSchema::kCategoriesSortOrder), StringFromInt(sortOrder) },
    });
}

int64_t AddSeries(Transaction& transaction, DatabaseHelper databaseHelper,
                  std::string_view slug) {
    Series series(databaseHelper);
    return series.AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), std::string(slug) },
        { std::string(DbSchema::kSeriesSlug), std::string(slug) },
    });
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(kSeriesCategoryAssignmentsCategorySlug)));
    }
    return slugs;
}

TEST(SeriesCategoryAssignmentsTest, AssignAndGetForSeriesInDisplayOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignAndGetForSeriesInDisplayOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesCategoryAssignments assignments(databaseHelper);
        int64_t seriesId = AddSeries(transaction, databaseHelper, "t4t");
        int64_t pride = AddCategory(transaction, databaseHelper, "pride", 30);
        int64_t dance = AddCategory(transaction, databaseHelper, "dance", 10);

        EXPECT_GT(assignments.Assign(transaction, seriesId, pride), 0);
        assignments.Assign(transaction, seriesId, dance);

        KeyValueTableArray rows = assignments.GetForSeries(transaction, seriesId);
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "dance", "pride" }));
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kSeriesCategoryAssignmentsCategoryId)),
                  StringFromInt(dance));
    });
}

TEST(SeriesCategoryAssignmentsTest, GetForSeriesIdsBatchesBySeries) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetForSeriesIdsBatchesBySeries", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesCategoryAssignments assignments(databaseHelper);
        int64_t first = AddSeries(transaction, databaseHelper, "first");
        int64_t second = AddSeries(transaction, databaseHelper, "second");
        int64_t unasked = AddSeries(transaction, databaseHelper, "unasked");
        int64_t dance = AddCategory(transaction, databaseHelper, "dance", 10);
        int64_t pride = AddCategory(transaction, databaseHelper, "pride", 20);
        assignments.Assign(transaction, second, pride);
        assignments.Assign(transaction, second, dance);
        assignments.Assign(transaction, first, pride);
        assignments.Assign(transaction, unasked, dance);

        KeyValueTableArray rows = assignments.GetForSeriesIds(transaction, { second, first });
        ASSERT_EQ(rows.size(), 3u);
        const std::string seriesIdColumn(DbSchema::kSeriesCategoryAssignmentsSeriesId);
        EXPECT_EQ(rows[0].at(seriesIdColumn), StringFromInt(first));
        EXPECT_EQ(rows[1].at(seriesIdColumn), StringFromInt(second));
        EXPECT_EQ(rows[2].at(seriesIdColumn), StringFromInt(second));
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "pride", "dance", "pride" }));

        EXPECT_TRUE(assignments.GetForSeriesIds(transaction, {}).empty());
    });
}

TEST(SeriesCategoryAssignmentsTest, RemoveDeletesOnlyThatPair) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RemoveDeletesOnlyThatPair", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesCategoryAssignments assignments(databaseHelper);
        int64_t seriesId = AddSeries(transaction, databaseHelper, "t4t");
        int64_t otherSeries = AddSeries(transaction, databaseHelper, "other");
        int64_t dance = AddCategory(transaction, databaseHelper, "dance", 10);
        int64_t pride = AddCategory(transaction, databaseHelper, "pride", 20);
        assignments.Assign(transaction, seriesId, dance);
        assignments.Assign(transaction, seriesId, pride);
        assignments.Assign(transaction, otherSeries, pride);

        assignments.Remove(transaction, seriesId, pride);
        // Removing a pair that isn't there is a no-op.
        assignments.Remove(transaction, seriesId, pride);

        EXPECT_EQ(Slugs(assignments.GetForSeries(transaction, seriesId)),
                  (std::vector<std::string>{ "dance" }));
        EXPECT_EQ(Slugs(assignments.GetForSeries(transaction, otherSeries)),
                  (std::vector<std::string>{ "pride" }));
    });
}

TEST(SeriesCategoryAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesCategoryAssignments assignments(databaseHelper);
        int64_t seriesId = AddSeries(transaction, databaseHelper, "t4t");
        int64_t dance = AddCategory(transaction, databaseHelper, "dance", 10);
        assignments.Assign(transaction, seriesId, dance);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, seriesId, dance), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
