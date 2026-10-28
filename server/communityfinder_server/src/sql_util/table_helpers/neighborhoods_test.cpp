#include "neighborhoods.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/neighborhoods.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// The harness seeds nothing, so every test adds the neighborhoods it reads.
namespace TableHelpers {
namespace {

KeyValueTable MakeNeighborhood(std::string_view slug, int64_t sortOrder) {
    return {
        { std::string(DbSchema::kNeighborhoodsSlug), std::string(slug) },
        { std::string(DbSchema::kNeighborhoodsDisplayName), std::string(slug) + " name" },
        { std::string(DbSchema::kNeighborhoodsRegion),
          std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
        { std::string(DbSchema::kNeighborhoodsSortOrder), StringFromInt(sortOrder) },
    };
}

TEST(NeighborhoodsTest, AddAndGetBySlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetBySlug", [&](Transaction& transaction) {
        Neighborhoods neighborhoods(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeNeighborhood("bellevue", 10);
        values[std::string(DbSchema::kNeighborhoodsRegion)] =
            std::string(DbSchema::kNeighborhoodsRegionEastside);
        int64_t id = neighborhoods.AddNeighborhood(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = neighborhoods.GetBySlug(transaction, "bellevue");
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kNeighborhoodsId)), StringFromInt(id));
        EXPECT_EQ(row.at(std::string(DbSchema::kNeighborhoodsRegion)),
                  DbSchema::kNeighborhoodsRegionEastside);
    });
}

TEST(NeighborhoodsTest, GetBySlugMissIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetBySlugMissIsEmpty", [&](Transaction& transaction) {
        Neighborhoods neighborhoods(testDb.GetDatabaseHelper());
        EXPECT_TRUE(neighborhoods.GetBySlug(transaction, "no-such-neighborhood").empty());
    });
}

TEST(NeighborhoodsTest, GetAllIsEnabledOnlyInSortOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetAllIsEnabledOnlyInSortOrder", [&](Transaction& transaction) {
        Neighborhoods neighborhoods(testDb.GetDatabaseHelper());
        int64_t second = neighborhoods.AddNeighborhood(
            transaction, MakeNeighborhood("fremont", 20));
        int64_t first = neighborhoods.AddNeighborhood(
            transaction, MakeNeighborhood("capitol-hill", 10));
        KeyValueTable disabled = MakeNeighborhood("retired", 5);
        disabled[std::string(DbSchema::kNeighborhoodsEnabled)] = "false";
        neighborhoods.AddNeighborhood(transaction, disabled);

        KeyValueTableArray rows = neighborhoods.GetAll(transaction);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kNeighborhoodsId)), StringFromInt(first));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kNeighborhoodsId)), StringFromInt(second));
    });
}

TEST(NeighborhoodsTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        Neighborhoods neighborhoods(testDb.GetDatabaseHelper());
        neighborhoods.AddNeighborhood(transaction, MakeNeighborhood("fremont", 10));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(
            neighborhoods.AddNeighborhood(transaction, MakeNeighborhood("fremont", 20)),
            std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
