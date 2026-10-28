#include "scene_tags.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/scene_tags.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// The harness seeds nothing, so every test adds the scene tags it reads.
namespace TableHelpers {
namespace {

KeyValueTable MakeSceneTag(std::string_view slug, int64_t sortOrder) {
    return {
        { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
        { std::string(DbSchema::kSceneTagsDisplayName), std::string(slug) + " name" },
        { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
    };
}

TEST(SceneTagsTest, AddAndGetBySlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetBySlug", [&](Transaction& transaction) {
        SceneTags sceneTags(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSceneTag("trans", 10);
        values[std::string(DbSchema::kSceneTagsCoverage)] =
            std::string(DbSchema::kSceneTagsCoverageLinkOut);
        int64_t id = sceneTags.AddSceneTag(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = sceneTags.GetBySlug(transaction, "trans");
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kSceneTagsId)), StringFromInt(id));
        EXPECT_EQ(row.at(std::string(DbSchema::kSceneTagsCoverage)),
                  DbSchema::kSceneTagsCoverageLinkOut);

        // Coverage defaults to full when not given.
        sceneTags.AddSceneTag(transaction, MakeSceneTag("bear", 20));
        EXPECT_EQ(sceneTags.GetBySlug(transaction, "bear")
                      .at(std::string(DbSchema::kSceneTagsCoverage)),
                  DbSchema::kSceneTagsCoverageFull);
    });
}

TEST(SceneTagsTest, GetBySlugMissIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetBySlugMissIsEmpty", [&](Transaction& transaction) {
        SceneTags sceneTags(testDb.GetDatabaseHelper());
        EXPECT_TRUE(sceneTags.GetBySlug(transaction, "no-such-tag").empty());
    });
}

TEST(SceneTagsTest, GetAllIsEnabledOnlyInSortOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetAllIsEnabledOnlyInSortOrder", [&](Transaction& transaction) {
        SceneTags sceneTags(testDb.GetDatabaseHelper());
        int64_t second = sceneTags.AddSceneTag(transaction, MakeSceneTag("leather", 20));
        int64_t first = sceneTags.AddSceneTag(transaction, MakeSceneTag("bear", 10));
        KeyValueTable disabled = MakeSceneTag("retired", 5);
        disabled[std::string(DbSchema::kSceneTagsEnabled)] = "false";
        sceneTags.AddSceneTag(transaction, disabled);

        KeyValueTableArray rows = sceneTags.GetAll(transaction);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kSceneTagsId)), StringFromInt(first));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kSceneTagsId)), StringFromInt(second));
    });
}

TEST(SceneTagsTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        SceneTags sceneTags(testDb.GetDatabaseHelper());
        sceneTags.AddSceneTag(transaction, MakeSceneTag("bear", 10));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(sceneTags.AddSceneTag(transaction, MakeSceneTag("bear", 20)),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
