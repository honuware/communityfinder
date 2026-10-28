#include "series_scene_tag_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/series_scene_tag_assignments.h"
#include "scene_tags.h"
#include "series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

int64_t AddSceneTag(Transaction& transaction, DatabaseHelper databaseHelper,
                    std::string_view slug, int64_t sortOrder) {
    SceneTags sceneTags(databaseHelper);
    return sceneTags.AddSceneTag(transaction, {
        { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
        { std::string(DbSchema::kSceneTagsDisplayName), std::string(slug) },
        { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
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
        slugs.push_back(row.at(std::string(kSeriesSceneTagAssignmentsSceneTagSlug)));
    }
    return slugs;
}

TEST(SeriesSceneTagAssignmentsTest, AssignAndGetForSeriesInDisplayOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignAndGetForSeriesInDisplayOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesSceneTagAssignments assignments(databaseHelper);
        int64_t seriesId = AddSeries(transaction, databaseHelper, "t4t");
        int64_t trans = AddSceneTag(transaction, databaseHelper, "trans", 30);
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);

        EXPECT_GT(assignments.Assign(transaction, seriesId, trans), 0);
        assignments.Assign(transaction, seriesId, queer);

        KeyValueTableArray rows = assignments.GetForSeries(transaction, seriesId);
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "queer", "trans" }));
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kSeriesSceneTagAssignmentsSceneTagId)),
                  StringFromInt(queer));
    });
}

TEST(SeriesSceneTagAssignmentsTest, GetForSeriesIdsBatchesBySeries) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetForSeriesIdsBatchesBySeries", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesSceneTagAssignments assignments(databaseHelper);
        int64_t first = AddSeries(transaction, databaseHelper, "first");
        int64_t second = AddSeries(transaction, databaseHelper, "second");
        int64_t unasked = AddSeries(transaction, databaseHelper, "unasked");
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);
        int64_t trans = AddSceneTag(transaction, databaseHelper, "trans", 20);
        assignments.Assign(transaction, second, trans);
        assignments.Assign(transaction, second, queer);
        assignments.Assign(transaction, first, trans);
        assignments.Assign(transaction, unasked, queer);

        KeyValueTableArray rows = assignments.GetForSeriesIds(transaction, { second, first });
        ASSERT_EQ(rows.size(), 3u);
        const std::string seriesIdColumn(DbSchema::kSeriesSceneTagAssignmentsSeriesId);
        EXPECT_EQ(rows[0].at(seriesIdColumn), StringFromInt(first));
        EXPECT_EQ(rows[1].at(seriesIdColumn), StringFromInt(second));
        EXPECT_EQ(rows[2].at(seriesIdColumn), StringFromInt(second));
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "trans", "queer", "trans" }));

        EXPECT_TRUE(assignments.GetForSeriesIds(transaction, {}).empty());
    });
}

TEST(SeriesSceneTagAssignmentsTest, RemoveDeletesOnlyThatPair) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RemoveDeletesOnlyThatPair", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesSceneTagAssignments assignments(databaseHelper);
        int64_t seriesId = AddSeries(transaction, databaseHelper, "t4t");
        int64_t otherSeries = AddSeries(transaction, databaseHelper, "other");
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);
        int64_t trans = AddSceneTag(transaction, databaseHelper, "trans", 20);
        assignments.Assign(transaction, seriesId, queer);
        assignments.Assign(transaction, seriesId, trans);
        assignments.Assign(transaction, otherSeries, trans);

        assignments.Remove(transaction, seriesId, trans);
        // Removing a pair that isn't there is a no-op.
        assignments.Remove(transaction, seriesId, trans);

        EXPECT_EQ(Slugs(assignments.GetForSeries(transaction, seriesId)),
                  (std::vector<std::string>{ "queer" }));
        EXPECT_EQ(Slugs(assignments.GetForSeries(transaction, otherSeries)),
                  (std::vector<std::string>{ "trans" }));
    });
}

TEST(SeriesSceneTagAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        SeriesSceneTagAssignments assignments(databaseHelper);
        int64_t seriesId = AddSeries(transaction, databaseHelper, "t4t");
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);
        assignments.Assign(transaction, seriesId, queer);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, seriesId, queer), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
