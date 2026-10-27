#include "make_database_info.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "categories.h"
#include "make_app_tables.h"
#include "neighborhoods.h"
#include "scene_tags.h"
#include "venues.h"
#include "db_schema/config_secrets.h"
#include "db_schema/make_framework_tables.h"
#include "db_schema/people.h"
#include "db_schema/permissions.h"
#include "db_schema/schema_migrations.h"
#include "db_schema/sessions.h"

namespace DbSchema {
namespace {

// Index of `name` in `tables`, or -1 if absent. GetAllTables() preserves
// AddTable insertion order, so index comparisons observe the framework-first
// composition order.
int IndexOf(const StringArray& tables, std::string_view name) {
    auto it = std::find(tables.begin(), tables.end(), std::string(name));
    if (it == tables.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(tables.begin(), it));
}

std::vector<std::string> ColumnNames(
    const DatabaseInfo& databaseInfo, std::string_view table) {
    std::vector<std::string> names;
    for (const ColumnInfo& column : databaseInfo.GetTableInfo(table).GetColumns()) {
        names.push_back(column.GetColumnName());
    }
    return names;
}

const ColumnInfo& Column(
    const DatabaseInfo& databaseInfo, std::string_view table, std::string_view column) {
    return databaseInfo.GetTableInfo(table).GetColumn(column);
}

// True when `column` alone carries a table-level UNIQUE constraint.
bool IsUniqueOnItsOwn(
    const DatabaseInfo& databaseInfo, std::string_view table, std::string_view column) {
    for (const TableUniqueValues& unique :
         databaseInfo.GetTableInfo(table).GetTableUniqueValues()) {
        if (unique.columns == StringArray{std::string(column)}) {
            return true;
        }
    }
    return false;
}

// The expression of the named CHECK constraint, or "" if the table has none by
// that name.
std::string CheckExpression(
    const DatabaseInfo& databaseInfo, std::string_view table, std::string_view name) {
    for (const TableCheckConstraint& check :
         databaseInfo.GetTableInfo(table).GetTableCheckConstraints()) {
        if (check.name == name) {
            return check.expression;
        }
    }
    return "";
}

TEST(MakeDatabaseInfoTest, ComposesFrameworkTables) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");
    StringArray tables = databaseInfo.GetAllTables();

    // Representative framework tables all land in the composed schema.
    EXPECT_NE(IndexOf(tables, kPeopleTable), -1);
    EXPECT_NE(IndexOf(tables, kPermissionsTable), -1);
    EXPECT_NE(IndexOf(tables, kSessionsTable), -1);
    EXPECT_NE(IndexOf(tables, kSchemaMigrationsTable), -1);
    EXPECT_NE(IndexOf(tables, kConfigSecretsTable), -1);
}

TEST(MakeDatabaseInfoTest, DatabaseNameIsPreserved) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("some_db_name");
    EXPECT_EQ(databaseInfo.GetDatabaseName(), "some_db_name");
}

TEST(MakeDatabaseInfoTest, AppTablesFollowTheFrameworkTables) {
    DatabaseInfo frameworkOnly("test_db");
    MakeFrameworkTables(frameworkOnly);
    StringArray expected = frameworkOnly.GetAllTables();

    // The composed schema is the framework tables, unchanged and in order, then
    // the app tables in FK order. Exact equality also catches an app table that
    // MakeAppTables forgot to register.
    for (std::string_view appTable :
         {kCategoriesTable, kSceneTagsTable, kNeighborhoodsTable, kVenuesTable}) {
        expected.push_back(std::string(appTable));
    }
    EXPECT_EQ(MakeDatabaseInfo("test_db").GetAllTables(), expected);
}

TEST(MakeDatabaseInfoTest, NoDuplicateTables) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");
    StringArray tables = databaseInfo.GetAllTables();
    std::set<std::string> unique(tables.begin(), tables.end());
    EXPECT_EQ(unique.size(), tables.size());
}

// EV1 Slice 1: the three vocabularies share one shape (GD1 §1–§3) — a required,
// unique slug and a required display name. A NULL slug would be a vocabulary
// entry no URL or filter could ever name.
TEST(MakeDatabaseInfoTest, VocabulariesHaveTheGd1Shape) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kCategoriesTable),
              (std::vector<std::string>{
                  "id", "slug", "display_name", "description", "sort_order",
                  "enabled", "created_us", "updated_us"}));
    EXPECT_EQ(ColumnNames(databaseInfo, kSceneTagsTable),
              (std::vector<std::string>{
                  "id", "slug", "display_name", "description", "coverage",
                  "sort_order", "enabled", "created_us", "updated_us"}));
    EXPECT_EQ(ColumnNames(databaseInfo, kNeighborhoodsTable),
              (std::vector<std::string>{
                  "id", "slug", "display_name", "region", "sort_order", "enabled",
                  "created_us", "updated_us"}));

    for (std::string_view table :
         {kCategoriesTable, kSceneTagsTable, kNeighborhoodsTable}) {
        EXPECT_TRUE(IsUniqueOnItsOwn(databaseInfo, table, "slug")) << table;
        EXPECT_FALSE(Column(databaseInfo, table, "slug").IsNullable()) << table;
        EXPECT_FALSE(Column(databaseInfo, table, "display_name").IsNullable())
            << table;
    }
}

TEST(MakeDatabaseInfoTest, CoverageAndRegionAreConstrainedToTheGd1Values) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(CheckExpression(databaseInfo, kSceneTagsTable, kSceneTagsCoverageCheck),
              "coverage IN ('full', 'link_out')");
    EXPECT_EQ(
        CheckExpression(databaseInfo, kNeighborhoodsTable, kNeighborhoodsRegionCheck),
        "region IN ('seattle-core', 'seattle-north', 'seattle-south', "
        "'eastside', 'south-king')");
    EXPECT_FALSE(Column(databaseInfo, kNeighborhoodsTable, "region").IsNullable());
}

// Interface Contract 2 keeps venues guide-free (no slug, adult flag, or
// crowd/vibe — those live on GD2's venue_profiles), and Contract 3 gives it the
// freshness columns exactly as GD1 §5 defines them.
TEST(MakeDatabaseInfoTest, VenuesCarryTheFreshnessColumnsAndNothingFromTheGuide) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kVenuesTable),
              (std::vector<std::string>{
                  "id", "name", "description", "address", "city", "state", "zip",
                  "url", "latitude", "longitude", "neighborhood_id", "status",
                  "last_verified_at_us", "volatility_tier", "created_us",
                  "updated_us"}));

    EXPECT_EQ(CheckExpression(databaseInfo, kVenuesTable, kVenuesStatusCheck),
              "status IN ('open', 'closed', 'moved', 'seasonal', 'unverified')");
    EXPECT_EQ(
        CheckExpression(databaseInfo, kVenuesTable, kVenuesVolatilityTierCheck),
        "volatility_tier IN ('weekly', 'monthly', 'quarterly', 'semiannual', "
        "'annual')");

    // Never verified, no coordinates yet, and no neighborhood are all legal.
    for (std::string_view column :
         {kVenuesLastVerifiedAtUs, kVenuesLatitude, kVenuesLongitude,
          kVenuesNeighborhoodId}) {
        EXPECT_TRUE(Column(databaseInfo, kVenuesTable, column).IsNullable())
            << column;
    }

    const ForeignKeyManager& foreignKeys = databaseInfo.GetForeignKeyManager();
    const TableColumnPair neighborhood{
        std::string(kVenuesTable), std::string(kVenuesNeighborhoodId)};
    ASSERT_TRUE(foreignKeys.HasForeignReference(neighborhood));
    EXPECT_TRUE(foreignKeys.GetForeignReferenceOfField(neighborhood) ==
                (TableColumnPair{std::string(kNeighborhoodsTable),
                                 std::string(kNeighborhoodsId)}));
}

}  // namespace
}  // namespace DbSchema
