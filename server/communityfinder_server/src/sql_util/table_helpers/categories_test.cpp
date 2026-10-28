#include "categories.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/categories.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// The harness seeds nothing, so every test adds the categories it reads.
namespace TableHelpers {
namespace {

KeyValueTable MakeCategory(std::string_view slug, int64_t sortOrder) {
    return {
        { std::string(DbSchema::kCategoriesSlug), std::string(slug) },
        { std::string(DbSchema::kCategoriesDisplayName), std::string(slug) + " name" },
        { std::string(DbSchema::kCategoriesSortOrder), StringFromInt(sortOrder) },
    };
}

TEST(CategoriesTest, AddAndGetBySlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetBySlug", [&](Transaction& transaction) {
        Categories categories(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeCategory("drag", 10);
        values[std::string(DbSchema::kCategoriesDescription)] = "Drag shows and brunches";
        int64_t id = categories.AddCategory(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = categories.GetBySlug(transaction, "drag");
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kCategoriesId)), StringFromInt(id));
        EXPECT_EQ(row.at(std::string(DbSchema::kCategoriesDisplayName)), "drag name");
        EXPECT_EQ(row.at(std::string(DbSchema::kCategoriesDescription)),
                  "Drag shows and brunches");
        EXPECT_EQ(row.at(std::string(DbSchema::kCategoriesEnabled)), "t");
    });
}

TEST(CategoriesTest, GetBySlugMissIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetBySlugMissIsEmpty", [&](Transaction& transaction) {
        Categories categories(testDb.GetDatabaseHelper());
        EXPECT_TRUE(categories.GetBySlug(transaction, "no-such-category").empty());
    });
}

TEST(CategoriesTest, GetAllIsEnabledOnlyInSortOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetAllIsEnabledOnlyInSortOrder", [&](Transaction& transaction) {
        Categories categories(testDb.GetDatabaseHelper());
        // Inserted out of order; two share a sort_order so id breaks the tie.
        int64_t third = categories.AddCategory(transaction, MakeCategory("pride", 30));
        int64_t first = categories.AddCategory(transaction, MakeCategory("bar", 10));
        int64_t secondA = categories.AddCategory(transaction, MakeCategory("drag", 20));
        int64_t secondB = categories.AddCategory(transaction, MakeCategory("dance", 20));
        KeyValueTable disabled = MakeCategory("retired", 5);
        disabled[std::string(DbSchema::kCategoriesEnabled)] = "false";
        categories.AddCategory(transaction, disabled);

        KeyValueTableArray rows = categories.GetAll(transaction);
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kCategoriesId)), StringFromInt(first));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kCategoriesId)), StringFromInt(secondA));
        EXPECT_EQ(rows[2].at(std::string(DbSchema::kCategoriesId)), StringFromInt(secondB));
        EXPECT_EQ(rows[3].at(std::string(DbSchema::kCategoriesId)), StringFromInt(third));

        // A disabled category is still reachable by slug (an ingest naming it
        // resolves; the public filter list just doesn't offer it).
        EXPECT_FALSE(categories.GetBySlug(transaction, "retired").empty());
    });
}

TEST(CategoriesTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        Categories categories(testDb.GetDatabaseHelper());
        categories.AddCategory(transaction, MakeCategory("bar", 10));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(categories.AddCategory(transaction, MakeCategory("bar", 20)),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
