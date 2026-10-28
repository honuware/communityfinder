#include "make_database_info.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "categories.h"
#include "event_category_assignments.h"
#include "event_scene_tag_assignments.h"
#include "event_sources.h"
#include "events.h"
#include "make_app_tables.h"
#include "neighborhoods.h"
#include "scene_tags.h"
#include "series.h"
#include "series_category_assignments.h"
#include "series_scene_tag_assignments.h"
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

// The column's DEFAULT expression as declared, or "" if it has none.
std::string DefaultValue(
    const DatabaseInfo& databaseInfo, std::string_view table, std::string_view column) {
    std::string defaultValue;
    Column(databaseInfo, table, column).IsDefault(defaultValue);
    return defaultValue;
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

// The columns of the named UNIQUE constraint, or empty if the table has none by
// that name.
StringArray UniqueColumns(
    const DatabaseInfo& databaseInfo, std::string_view table, std::string_view name) {
    for (const TableUniqueValues& unique :
         databaseInfo.GetTableInfo(table).GetTableUniqueValues()) {
        if (unique.name == name) {
            return unique.columns;
        }
    }
    return {};
}

// "parent_table.column" that `table.column` references, or "" if it is not a
// foreign key.
std::string ReferencedColumn(
    const DatabaseInfo& databaseInfo, std::string_view table, std::string_view column) {
    const ForeignKeyManager& foreignKeys = databaseInfo.GetForeignKeyManager();
    const TableColumnPair child{std::string(table), std::string(column)};
    if (!foreignKeys.HasForeignReference(child)) {
        return "";
    }
    const TableColumnPair parent = foreignKeys.GetForeignReferenceOfField(child);
    return parent.tableName + "." + parent.columnName;
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
         {kCategoriesTable, kSceneTagsTable, kNeighborhoodsTable, kVenuesTable,
          kEventSourcesTable, kSeriesTable, kEventsTable,
          kEventCategoryAssignmentsTable, kEventSceneTagAssignmentsTable,
          kSeriesCategoryAssignmentsTable, kSeriesSceneTagAssignmentsTable}) {
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

    EXPECT_EQ(ReferencedColumn(databaseInfo, kVenuesTable, kVenuesNeighborhoodId),
              "neighborhoods.id");
}

// EV1 Slice 2: a series is a listable entity (GD1 §4), so it carries the same
// freshness columns as venues, with GD1 §5's series default tier; its slug is
// the series page's URL.
TEST(MakeDatabaseInfoTest, SeriesCarryTheFreshnessColumnsAndAUniqueSlug) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kSeriesTable),
              (std::vector<std::string>{
                  "id", "title", "slug", "description", "cadence_text", "url",
                  "venue_id", "status", "last_verified_at_us", "volatility_tier",
                  "created_us", "updated_us"}));

    EXPECT_TRUE(IsUniqueOnItsOwn(databaseInfo, kSeriesTable, kSeriesSlug));
    EXPECT_FALSE(Column(databaseInfo, kSeriesTable, kSeriesSlug).IsNullable());
    EXPECT_EQ(CheckExpression(databaseInfo, kSeriesTable, kSeriesStatusCheck),
              "status IN ('open', 'closed', 'moved', 'seasonal', 'unverified')");
    EXPECT_EQ(
        CheckExpression(databaseInfo, kSeriesTable, kSeriesVolatilityTierCheck),
        "volatility_tier IN ('weekly', 'monthly', 'quarterly', 'semiannual', "
        "'annual')");
    EXPECT_EQ(DefaultValue(databaseInfo, kSeriesTable, kSeriesVolatilityTier), "'monthly'");
    EXPECT_EQ(ReferencedColumn(databaseInfo, kSeriesTable, kSeriesVenueId), "venues.id");
    EXPECT_TRUE(Column(databaseInfo, kSeriesTable, kSeriesVenueId).IsNullable());
}

TEST(MakeDatabaseInfoTest, EventSourcesAreConstrainedToTheIntakeKinds) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kEventSourcesTable),
              (std::vector<std::string>{
                  "id", "name", "url", "kind", "venue_id", "enabled", "scan_hints",
                  "volatility_tier", "last_scanned_at_us", "notes", "created_us",
                  "updated_us"}));

    EXPECT_EQ(CheckExpression(databaseInfo, kEventSourcesTable, kEventSourcesKindCheck),
              "kind IN ('ics_feed', 'venue_site', 'ticketing_org', 'aggregator', "
              "'editorial_monitor', 'submission', 'social')");
    EXPECT_FALSE(Column(databaseInfo, kEventSourcesTable, kEventSourcesKind).IsNullable());
    EXPECT_EQ(CheckExpression(databaseInfo, kEventSourcesTable,
                              kEventSourcesVolatilityTierCheck),
              "volatility_tier IN ('weekly', 'monthly', 'quarterly', 'semiannual', "
              "'annual')");
    EXPECT_EQ(DefaultValue(databaseInfo, kEventSourcesTable, kEventSourcesVolatilityTier),
              "'weekly'");
    EXPECT_EQ(ReferencedColumn(databaseInfo, kEventSourcesTable, kEventSourcesVenueId),
              "venues.id");
    EXPECT_TRUE(
        Column(databaseInfo, kEventSourcesTable, kEventSourcesLastScannedAtUs).IsNullable());
}

// Interface Contract 3: events are pipeline items with the review workflow, not
// listings — so no freshness columns, and the exact column list proves it.
TEST(MakeDatabaseInfoTest, EventsCarryTheReviewWorkflowNotTheFreshnessColumns) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kEventsTable),
              (std::vector<std::string>{
                  "id", "title", "description", "starts_at_us", "ends_at_us",
                  "venue_id", "location_text", "url", "cost_text", "source_id",
                  "series_id", "external_key", "origin", "status",
                  "created_by_person_id", "reviewed_by_person_id", "reviewed_us",
                  "review_notes", "created_us", "updated_us"}));

    EXPECT_EQ(CheckExpression(databaseInfo, kEventsTable, kEventsStatusCheck),
              "status IN ('pending', 'approved', 'rejected', 'archived')");
    EXPECT_EQ(DefaultValue(databaseInfo, kEventsTable, kEventsStatus), "'pending'");
    EXPECT_EQ(CheckExpression(databaseInfo, kEventsTable, kEventsOriginCheck),
              "origin IN ('scanned', 'manual', 'user_submitted')");
    // A row added in the admin editor is hand-entered; the ingest sets its own.
    EXPECT_EQ(DefaultValue(databaseInfo, kEventsTable, kEventsOrigin), "'manual'");

    EXPECT_FALSE(Column(databaseInfo, kEventsTable, kEventsStartsAtUs).IsNullable());
    for (std::string_view column :
         {kEventsEndsAtUs, kEventsVenueId, kEventsSourceId, kEventsSeriesId,
          kEventsExternalKey, kEventsCreatedByPersonId, kEventsReviewedByPersonId,
          kEventsReviewedUs}) {
        EXPECT_TRUE(Column(databaseInfo, kEventsTable, column).IsNullable()) << column;
    }

    EXPECT_EQ(ReferencedColumn(databaseInfo, kEventsTable, kEventsVenueId), "venues.id");
    EXPECT_EQ(ReferencedColumn(databaseInfo, kEventsTable, kEventsSourceId),
              "event_sources.id");
    EXPECT_EQ(ReferencedColumn(databaseInfo, kEventsTable, kEventsSeriesId), "series.id");
    // Deliberately not FKs: honuware FKs cascade, so deleting a person would
    // delete every event they entered or reviewed (OQ8 (e)).
    EXPECT_EQ(ReferencedColumn(databaseInfo, kEventsTable, kEventsCreatedByPersonId), "");
    EXPECT_EQ(ReferencedColumn(databaseInfo, kEventsTable, kEventsReviewedByPersonId), "");
}

// The ingestion idempotency anchor: re-ingesting an item matches the row it
// created last time instead of inserting a duplicate.
TEST(MakeDatabaseInfoTest, IngestedEventsAreUniquePerSourceAndExternalKey) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(UniqueColumns(databaseInfo, kEventsTable, kEventsSourceIdExternalKeyUnique),
              (StringArray{"source_id", "external_key"}));
}

// EV1 OQ5: events and series each carry category and scene-tag assignments.
// Each pair is required on both sides and may appear only once.
TEST(MakeDatabaseInfoTest, AssignmentTablesAllowEachPairOnce) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    struct Assignment {
        std::string_view table;
        std::string_view ownerColumn;
        std::string_view owner;
        std::string_view valueColumn;
        std::string_view value;
    };
    const Assignment kAssignments[] = {
        {kEventCategoryAssignmentsTable, "event_id", "events.id",
         "category_id", "categories.id"},
        {kEventSceneTagAssignmentsTable, "event_id", "events.id",
         "scene_tag_id", "scene_tags.id"},
        {kSeriesCategoryAssignmentsTable, "series_id", "series.id",
         "category_id", "categories.id"},
        {kSeriesSceneTagAssignmentsTable, "series_id", "series.id",
         "scene_tag_id", "scene_tags.id"},
    };
    for (const Assignment& assignment : kAssignments) {
        const std::string ownerColumn(assignment.ownerColumn);
        const std::string valueColumn(assignment.valueColumn);
        EXPECT_EQ(ColumnNames(databaseInfo, assignment.table),
                  (std::vector<std::string>{"id", ownerColumn, valueColumn, "created_us"}))
            << assignment.table;
        EXPECT_EQ(UniqueColumns(databaseInfo, assignment.table, ""),
                  (StringArray{ownerColumn, valueColumn}))
            << assignment.table;
        EXPECT_EQ(ReferencedColumn(databaseInfo, assignment.table, ownerColumn),
                  assignment.owner)
            << assignment.table;
        EXPECT_EQ(ReferencedColumn(databaseInfo, assignment.table, valueColumn),
                  assignment.value)
            << assignment.table;
        EXPECT_FALSE(Column(databaseInfo, assignment.table, ownerColumn).IsNullable())
            << assignment.table;
        EXPECT_FALSE(Column(databaseInfo, assignment.table, valueColumn).IsNullable())
            << assignment.table;
    }
}

}  // namespace
}  // namespace DbSchema
