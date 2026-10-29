#include "make_database_info.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "categories.h"
#include "event_category_assignments.h"
#include "event_scene_tag_assignments.h"
#include "event_sources.h"
#include "events.h"
#include "guide_links.h"
#include "guide_page_scene_tag_assignments.h"
#include "guide_pages.h"
#include "make_app_tables.h"
#include "neighborhoods.h"
#include "organization_scene_tag_assignments.h"
#include "organizations.h"
#include "place_scene_tag_assignments.h"
#include "places.h"
#include "scene_tags.h"
#include "series.h"
#include "series_category_assignments.h"
#include "series_scene_tag_assignments.h"
#include "service_listings.h"
#include "venue_profile_scene_tag_assignments.h"
#include "venue_profiles.h"
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

// An assignment table: one owner row, one tag row, each pair at most once.
struct Assignment {
    std::string_view table;
    std::string_view ownerColumn;
    std::string_view owner;
    std::string_view valueColumn;
    std::string_view value;
};

// Each pair is required on both sides and may appear only once.
void ExpectAssignmentTable(const DatabaseInfo& databaseInfo, const Assignment& assignment) {
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

// GD1 §5's freshness columns, as every listable guide table carries them.
void ExpectFreshnessColumns(
    const DatabaseInfo& databaseInfo, std::string_view table,
    std::string_view statusCheck, std::string_view volatilityTierCheck,
    std::string_view defaultTier) {
    EXPECT_EQ(CheckExpression(databaseInfo, table, statusCheck),
              "status IN ('open', 'closed', 'moved', 'seasonal', 'unverified')")
        << table;
    EXPECT_EQ(DefaultValue(databaseInfo, table, "status"), "'open'") << table;
    EXPECT_EQ(CheckExpression(databaseInfo, table, volatilityTierCheck),
              "volatility_tier IN ('weekly', 'monthly', 'quarterly', 'semiannual', "
              "'annual')")
        << table;
    EXPECT_EQ(DefaultValue(databaseInfo, table, "volatility_tier"), defaultTier) << table;
    // Never verified is legal; the stamp is set only by a person checking.
    EXPECT_TRUE(Column(databaseInfo, table, "last_verified_at_us").IsNullable()) << table;
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
          kSeriesCategoryAssignmentsTable, kSeriesSceneTagAssignmentsTable,
          kVenueProfilesTable, kOrganizationsTable, kPlacesTable,
          kServiceListingsTable, kGuidePagesTable,
          kVenueProfileSceneTagAssignmentsTable,
          kOrganizationSceneTagAssignmentsTable, kPlaceSceneTagAssignmentsTable,
          kGuidePageSceneTagAssignmentsTable, kGuideLinksTable}) {
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
        ExpectAssignmentTable(databaseInfo, assignment);
    }
}

// --- GD2: the guide ----------------------------------------------------------

// Interface Contract 2: the profile is a 1:1 satellite of a venue. It has no
// name and no status of its own (the venue's are authoritative), but it does
// carry its own verification stamp for the guide facts (GD1 §4).
TEST(MakeDatabaseInfoTest, VenueProfilesAreAOneToOneSatelliteOfVenues) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kVenueProfilesTable),
              (std::vector<std::string>{
                  "id", "venue_id", "slug", "crowd_vibe", "price_band", "adult",
                  "closure_note", "last_verified_at_us", "volatility_tier",
                  "created_us", "updated_us"}));

    EXPECT_EQ(ReferencedColumn(databaseInfo, kVenueProfilesTable, kVenueProfilesVenueId),
              "venues.id");
    EXPECT_TRUE(IsUniqueOnItsOwn(databaseInfo, kVenueProfilesTable, kVenueProfilesVenueId));
    EXPECT_FALSE(Column(databaseInfo, kVenueProfilesTable, kVenueProfilesVenueId).IsNullable());
    EXPECT_TRUE(IsUniqueOnItsOwn(databaseInfo, kVenueProfilesTable, kVenueProfilesSlug));
    EXPECT_FALSE(Column(databaseInfo, kVenueProfilesTable, kVenueProfilesSlug).IsNullable());

    EXPECT_EQ(DefaultValue(databaseInfo, kVenueProfilesTable, kVenueProfilesAdult), "FALSE");
    EXPECT_EQ(CheckExpression(databaseInfo, kVenueProfilesTable,
                              kVenueProfilesVolatilityTierCheck),
              "volatility_tier IN ('weekly', 'monthly', 'quarterly', 'semiannual', "
              "'annual')");
    EXPECT_EQ(DefaultValue(databaseInfo, kVenueProfilesTable, kVenueProfilesVolatilityTier),
              "'quarterly'");
    // Absent until there is something to say.
    for (std::string_view column :
         {kVenueProfilesPriceBand, kVenueProfilesClosureNote,
          kVenueProfilesLastVerifiedAtUs}) {
        EXPECT_TRUE(Column(databaseInfo, kVenueProfilesTable, column).IsNullable())
            << column;
    }
}

// Interface Contract 3: organizations, places, service listings, and guide pages
// are listable, so each carries the freshness columns with GD1 §5's default
// tier for its type, and a unique slug for its page.
TEST(MakeDatabaseInfoTest, GuideEntitiesCarryTheFreshnessColumnsAndAUniqueSlug) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kOrganizationsTable),
              (std::vector<std::string>{
                  "id", "name", "slug", "org_kind", "description", "url",
                  "meeting_cadence_text", "neighborhood_id", "adult", "status",
                  "closure_note", "last_verified_at_us", "volatility_tier",
                  "created_us", "updated_us"}));
    EXPECT_EQ(ColumnNames(databaseInfo, kPlacesTable),
              (std::vector<std::string>{
                  "id", "name", "slug", "place_kind", "description", "address_text",
                  "neighborhood_id", "adult", "status", "last_verified_at_us",
                  "volatility_tier", "created_us", "updated_us"}));
    EXPECT_EQ(ColumnNames(databaseInfo, kServiceListingsTable),
              (std::vector<std::string>{
                  "id", "name", "slug", "service_kind", "description", "url",
                  "phone", "address_text", "neighborhood_id", "is_health", "status",
                  "last_verified_at_us", "volatility_tier", "created_us",
                  "updated_us"}));
    EXPECT_EQ(ColumnNames(databaseInfo, kGuidePagesTable),
              (std::vector<std::string>{
                  "id", "title", "slug", "section", "body_markdown", "adult", "draft",
                  "publish_at_us", "status", "last_verified_at_us", "volatility_tier",
                  "created_us", "updated_us"}));

    ExpectFreshnessColumns(databaseInfo, kOrganizationsTable, kOrganizationsStatusCheck,
                           kOrganizationsVolatilityTierCheck, "'annual'");
    ExpectFreshnessColumns(databaseInfo, kPlacesTable, kPlacesStatusCheck,
                           kPlacesVolatilityTierCheck, "'quarterly'");
    ExpectFreshnessColumns(databaseInfo, kServiceListingsTable, kServiceListingsStatusCheck,
                           kServiceListingsVolatilityTierCheck, "'semiannual'");
    ExpectFreshnessColumns(databaseInfo, kGuidePagesTable, kGuidePagesStatusCheck,
                           kGuidePagesVolatilityTierCheck, "'annual'");

    for (std::string_view table :
         {kOrganizationsTable, kPlacesTable, kServiceListingsTable, kGuidePagesTable}) {
        EXPECT_TRUE(IsUniqueOnItsOwn(databaseInfo, table, "slug")) << table;
        EXPECT_FALSE(Column(databaseInfo, table, "slug").IsNullable()) << table;
    }

    // One neighborhood or none: NULL = countywide or it moves (GD2 OQ7).
    for (std::string_view table :
         {kOrganizationsTable, kPlacesTable, kServiceListingsTable}) {
        EXPECT_EQ(ReferencedColumn(databaseInfo, table, "neighborhood_id"),
                  "neighborhoods.id")
            << table;
        EXPECT_TRUE(Column(databaseInfo, table, "neighborhood_id").IsNullable()) << table;
    }
    EXPECT_TRUE(
        Column(databaseInfo, kOrganizationsTable, kOrganizationsClosureNote).IsNullable());
}

// GD1 §4's kinds and sections, verbatim. Each is required: there is no default
// kind to fall back on.
TEST(MakeDatabaseInfoTest, GuideKindsAreConstrainedToTheGd1Values) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(CheckExpression(databaseInfo, kOrganizationsTable, kOrganizationsOrgKindCheck),
              "org_kind IN ('sports-league', 'chorus-performance', 'club-hobby', "
              "'recovery', 'faith', 'professional', 'community-support', "
              "'social-meetup')");
    EXPECT_EQ(CheckExpression(databaseInfo, kPlacesTable, kPlacesPlaceKindCheck),
              "place_kind IN ('park-beach', 'landmark', 'historical')");
    EXPECT_EQ(CheckExpression(databaseInfo, kServiceListingsTable,
                              kServiceListingsServiceKindCheck),
              "service_kind IN ('sti-testing', 'prep', 'hiv-care', 'primary-care', "
              "'mental-health', 'gender-affirming', 'dental', 'everyday-service')");
    EXPECT_EQ(CheckExpression(databaseInfo, kGuidePagesTable, kGuidePagesSectionCheck),
              "section IN ('scenes', 'health', 'services', 'new-to-seattle', 'freeze', "
              "'new-to', 'adult')");

    const std::pair<std::string_view, std::string_view> kKindColumns[] = {
        {kOrganizationsTable, kOrganizationsOrgKind},
        {kPlacesTable, kPlacesPlaceKind},
        {kServiceListingsTable, kServiceListingsServiceKind},
        {kGuidePagesTable, kGuidePagesSection},
    };
    for (const auto& [table, column] : kKindColumns) {
        EXPECT_FALSE(Column(databaseInfo, table, column).IsNullable()) << table;
        EXPECT_EQ(DefaultValue(databaseInfo, table, column), "") << table;
    }
}

// The two flags that gate what a visitor sees: the 18+ gate, and the MHMDA
// rule that no person is ever attributed to a health listing.
TEST(MakeDatabaseInfoTest, GuideFlagsDefaultToTheSafeSide) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(DefaultValue(databaseInfo, kOrganizationsTable, kOrganizationsAdult), "FALSE");
    EXPECT_EQ(DefaultValue(databaseInfo, kPlacesTable, kPlacesAdult), "FALSE");
    EXPECT_EQ(DefaultValue(databaseInfo, kGuidePagesTable, kGuidePagesAdult), "FALSE");
    // A listing is treated as health care unless a person says otherwise.
    EXPECT_EQ(DefaultValue(databaseInfo, kServiceListingsTable, kServiceListingsIsHealth),
              "TRUE");
    // Everything filed in the 18+ section is behind the 18+ gate.
    EXPECT_EQ(CheckExpression(databaseInfo, kGuidePagesTable, kGuidePagesAdultSectionCheck),
              "section <> 'adult' OR adult");
}

// The knottyyoga blog's publish predicate needs both columns: a page is public
// only when it is not a draft and its publish time has arrived. A new page is a
// draft with no publish time, so nothing goes public by accident.
TEST(MakeDatabaseInfoTest, GuidePagesStartAsUnpublishedDrafts) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(DefaultValue(databaseInfo, kGuidePagesTable, kGuidePagesDraft), "TRUE");
    EXPECT_FALSE(Column(databaseInfo, kGuidePagesTable, kGuidePagesDraft).IsNullable());
    EXPECT_TRUE(Column(databaseInfo, kGuidePagesTable, kGuidePagesPublishAtUs).IsNullable());
    EXPECT_EQ(DefaultValue(databaseInfo, kGuidePagesTable, kGuidePagesPublishAtUs), "");
}

// Scene tags apply to every guide entity alike (GD1 §2).
TEST(MakeDatabaseInfoTest, GuideSceneTagAssignmentsAllowEachPairOnce) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    const Assignment kAssignments[] = {
        {kVenueProfileSceneTagAssignmentsTable, "venue_profile_id", "venue_profiles.id",
         "scene_tag_id", "scene_tags.id"},
        {kOrganizationSceneTagAssignmentsTable, "organization_id", "organizations.id",
         "scene_tag_id", "scene_tags.id"},
        {kPlaceSceneTagAssignmentsTable, "place_id", "places.id",
         "scene_tag_id", "scene_tags.id"},
        {kGuidePageSceneTagAssignmentsTable, "guide_page_id", "guide_pages.id",
         "scene_tag_id", "scene_tags.id"},
    };
    for (const Assignment& assignment : kAssignments) {
        ExpectAssignmentTable(databaseInfo, assignment);
    }
}

// GD2 OQ2: one links table, a real FK to each of the four kinds of parent, and a
// CHECK that exactly one of them is set.
TEST(MakeDatabaseInfoTest, GuideLinksHaveExactlyOneParent) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");

    EXPECT_EQ(ColumnNames(databaseInfo, kGuideLinksTable),
              (std::vector<std::string>{
                  "id", "label", "url", "platform", "sort_order", "venue_profile_id",
                  "organization_id", "place_id", "scene_tag_id", "created_us",
                  "updated_us"}));

    EXPECT_EQ(CheckExpression(databaseInfo, kGuideLinksTable,
                              kGuideLinksExactlyOneParentCheck),
              "num_nonnulls(venue_profile_id, organization_id, place_id, "
              "scene_tag_id) = 1");
    EXPECT_EQ(CheckExpression(databaseInfo, kGuideLinksTable, kGuideLinksPlatformCheck),
              "platform IN ('discord', 'bluesky', 'instagram', 'facebook-group', "
              "'meetup', 'website', 'other')");

    const std::pair<std::string_view, std::string_view> kParents[] = {
        {kGuideLinksVenueProfileId, "venue_profiles.id"},
        {kGuideLinksOrganizationId, "organizations.id"},
        {kGuideLinksPlaceId, "places.id"},
        {kGuideLinksSceneTagId, "scene_tags.id"},
    };
    for (const auto& [column, parent] : kParents) {
        EXPECT_EQ(ReferencedColumn(databaseInfo, kGuideLinksTable, column), parent)
            << column;
        EXPECT_TRUE(Column(databaseInfo, kGuideLinksTable, column).IsNullable()) << column;
    }
    for (std::string_view column :
         {kGuideLinksLabel, kGuideLinksUrl, kGuideLinksPlatform}) {
        EXPECT_FALSE(Column(databaseInfo, kGuideLinksTable, column).IsNullable()) << column;
    }
}

}  // namespace
}  // namespace DbSchema
