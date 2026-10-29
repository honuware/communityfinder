#include "guide_page_scene_tag_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/guide_page_scene_tag_assignments.h"
#include "db_schema/guide_pages.h"
#include "db_schema/scene_tags.h"
#include "guide_pages.h"
#include "scene_tags.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// One round trip through every method; the full set of junction cases lives in
// venue_profile_scene_tag_assignments_test.cpp, which shares this shape.
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

// A draft: tags don't depend on the publish state.
int64_t AddPage(Transaction& transaction, DatabaseHelper databaseHelper,
                std::string_view slug) {
    GuidePages pages(databaseHelper);
    return pages.AddPage(transaction, {
        { std::string(DbSchema::kGuidePagesTitle), std::string(slug) },
        { std::string(DbSchema::kGuidePagesSlug), std::string(slug) },
        { std::string(DbSchema::kGuidePagesSection),
          std::string(DbSchema::kGuidePagesSectionScenes) },
    });
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(kGuidePageSceneTagAssignmentsSceneTagSlug)));
    }
    return slugs;
}

TEST(GuidePageSceneTagAssignmentsTest, AssignReadBatchAndRemove) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignReadBatchAndRemove", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuidePageSceneTagAssignments assignments(databaseHelper);
        // intro was added first, so its id is the lower one.
        int64_t intro = AddPage(transaction, databaseHelper, "intro");
        int64_t history = AddPage(transaction, databaseHelper, "history");
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);

        EXPECT_GT(assignments.Assign(transaction, intro, leather), 0);
        assignments.Assign(transaction, intro, bears);
        assignments.Assign(transaction, history, bears);

        EXPECT_EQ(Slugs(assignments.GetForGuidePage(transaction, intro)),
                  (std::vector<std::string>{ "bears", "leather" }));
        KeyValueTableArray batch = assignments.GetForGuidePages(transaction, { history, intro });
        ASSERT_EQ(batch.size(), 3u);
        EXPECT_EQ(batch[0].at(std::string(DbSchema::kGuidePageSceneTagAssignmentsGuidePageId)),
                  StringFromInt(intro));
        EXPECT_EQ(batch[2].at(std::string(DbSchema::kGuidePageSceneTagAssignmentsGuidePageId)),
                  StringFromInt(history));
        EXPECT_TRUE(assignments.GetForGuidePages(transaction, {}).empty());
        EXPECT_EQ(assignments.GetGuidePageIdsForSceneTag(transaction, bears),
                  (std::vector<int64_t>{ intro, history }));

        assignments.Remove(transaction, intro, bears);
        EXPECT_EQ(Slugs(assignments.GetForGuidePage(transaction, intro)),
                  (std::vector<std::string>{ "leather" }));
        EXPECT_EQ(assignments.GetGuidePageIdsForSceneTag(transaction, bears),
                  (std::vector<int64_t>{ history }));
    });
}

TEST(GuidePageSceneTagAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuidePageSceneTagAssignments assignments(databaseHelper);
        int64_t intro = AddPage(transaction, databaseHelper, "intro");
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);
        assignments.Assign(transaction, intro, bears);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, intro, bears), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
