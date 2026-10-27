#include "business_logic/create_app_tables.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "db_schema/admin_column_data_info.h"
#include "db_schema/admin_column_friendly_names.h"
#include "db_schema/admin_table_display_template.h"
#include "db_schema/admin_table_friendly_names.h"
#include "db_schema/admin_top_level_tables.h"
#include "db_schema/allowed_tables.h"
#include "db_schema/categories.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/scene_tags.h"
#include "db_schema/venues.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "sql_util/database_access/db_and_table_operations.h"
#include "sql_util/database_common.h"
#include "util/types.h"

namespace {

// Returns a lambda that inserts one row into `tableName`. Captures databaseHelper
// and tableName BY VALUE (the returned lambda outlives this call) — a [&] capture
// dangles the string_view on Linux; `transaction` is a reference to the caller's
// long-lived, non-copyable Transaction. (Same rule as create_database.cpp.)
auto MakeAddRowLambda(
    Transaction& transaction, DatabaseHelper databaseHelper,
    std::string_view tableName) {
    return [&transaction, databaseHelper, tableName](
               const KeyValueTable& keyValueTable) {
        DbCrud::AddRowToTable(
            transaction, databaseHelper, tableName, keyValueTable);
    };
}

// Seeded sort_order steps by 10 so an admin can slot a new entry between two
// seeded ones without renumbering.
constexpr int64_t kSortOrderStep = 10;

// --- The shared vocabularies: GD1 §1–§3, verbatim ----------------------------
// Slugs are load-bearing (URLs + cross-stream references); renaming one needs a
// cross-stream ping. Display names are free to edit. Listed in display order.

struct CategorySeed {
    std::string_view slug;
    std::string_view displayName;
};

// GD1 §1 — the twelve locked by SUTP Q4a.
constexpr CategorySeed kCategorySeeds[] = {
    { "bar", "Bar & club nights" },
    { "drag", "Drag" },
    { "nightlife", "Nightlife" },
    { "music", "Music & concerts" },
    { "theatre", "Theatre & film" },
    { "arts", "Arts & culture" },
    { "pride", "Pride & festivals" },
    { "sports", "Sports & recreation" },
    { "outdoors", "Outdoors" },
    { "volunteer", "Volunteering" },
    { "protest", "Protest & civic" },
    { "community", "Community & social" },
};

struct SceneTagSeed {
    std::string_view slug;
    std::string_view displayName;
    std::string_view coverage;
};

// GD1 §2 — eleven tags; sapphic / trans / qtpoc are link-out scenes.
constexpr SceneTagSeed kSceneTagSeeds[] = {
    { "bear", "Bears", DbSchema::kSceneTagsCoverageFull },
    { "leather", "Leather & kink", DbSchema::kSceneTagsCoverageFull },
    { "jock", "Jocks & sports", DbSchema::kSceneTagsCoverageFull },
    { "sober", "Sober", DbSchema::kSceneTagsCoverageFull },
    { "elders", "Elders", DbSchema::kSceneTagsCoverageFull },
    { "geek", "Geeks & gaymers", DbSchema::kSceneTagsCoverageFull },
    { "furry", "Furries", DbSchema::kSceneTagsCoverageFull },
    { "bipoc", "BIPOC", DbSchema::kSceneTagsCoverageFull },
    { "sapphic", "Sapphic", DbSchema::kSceneTagsCoverageLinkOut },
    { "trans", "Trans", DbSchema::kSceneTagsCoverageLinkOut },
    { "qtpoc", "QTPOC", DbSchema::kSceneTagsCoverageLinkOut },
};

struct NeighborhoodSeed {
    std::string_view slug;
    std::string_view displayName;
    std::string_view region;
};

// GD1 §3 — slugs and regions verbatim, in GD1's order. GD1 lists slugs only, so
// the display names are the neighborhoods' common names.
constexpr NeighborhoodSeed kNeighborhoodSeeds[] = {
    { "capitol-hill", "Capitol Hill", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "first-hill", "First Hill", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "central-district", "Central District", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "downtown", "Downtown", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "sodo", "SODO", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "queen-anne", "Queen Anne", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "u-district", "U District", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "wallingford", "Wallingford", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "ballard", "Ballard", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "fremont", "Fremont", DbSchema::kNeighborhoodsRegionSeattleCore },
    { "beacon-hill", "Beacon Hill", DbSchema::kNeighborhoodsRegionSeattleSouth },
    { "columbia-city", "Columbia City", DbSchema::kNeighborhoodsRegionSeattleSouth },
    { "georgetown", "Georgetown", DbSchema::kNeighborhoodsRegionSeattleSouth },
    { "west-seattle", "West Seattle", DbSchema::kNeighborhoodsRegionSeattleSouth },
    { "white-center", "White Center", DbSchema::kNeighborhoodsRegionSeattleSouth },
    { "shoreline", "Shoreline", DbSchema::kNeighborhoodsRegionSeattleNorth },
    { "bellevue", "Bellevue", DbSchema::kNeighborhoodsRegionEastside },
    { "kirkland", "Kirkland", DbSchema::kNeighborhoodsRegionEastside },
    { "redmond", "Redmond", DbSchema::kNeighborhoodsRegionEastside },
    { "bothell", "Bothell", DbSchema::kNeighborhoodsRegionEastside },
    { "issaquah", "Issaquah", DbSchema::kNeighborhoodsRegionEastside },
    { "sammamish", "Sammamish", DbSchema::kNeighborhoodsRegionEastside },
    { "burien", "Burien", DbSchema::kNeighborhoodsRegionSouthKing },
    { "renton", "Renton", DbSchema::kNeighborhoodsRegionSouthKing },
    { "tukwila", "Tukwila", DbSchema::kNeighborhoodsRegionSouthKing },
};

void PopulateVocabularies(
    Transaction& transaction, DatabaseHelper databaseHelper) {
    int64_t sortOrder = 0;
    for (const CategorySeed& seed : kCategorySeeds) {
        sortOrder += kSortOrderStep;
        MakeAddRowLambda(transaction, databaseHelper, DbSchema::kCategoriesTable)(
            { DbPair(DbSchema::kCategoriesSlug, seed.slug),
              DbPair(DbSchema::kCategoriesDisplayName, seed.displayName),
              DbPair(DbSchema::kCategoriesSortOrder, std::to_string(sortOrder)) });
    }

    sortOrder = 0;
    for (const SceneTagSeed& seed : kSceneTagSeeds) {
        sortOrder += kSortOrderStep;
        MakeAddRowLambda(transaction, databaseHelper, DbSchema::kSceneTagsTable)(
            { DbPair(DbSchema::kSceneTagsSlug, seed.slug),
              DbPair(DbSchema::kSceneTagsDisplayName, seed.displayName),
              DbPair(DbSchema::kSceneTagsCoverage, seed.coverage),
              DbPair(DbSchema::kSceneTagsSortOrder, std::to_string(sortOrder)) });
    }

    sortOrder = 0;
    for (const NeighborhoodSeed& seed : kNeighborhoodSeeds) {
        sortOrder += kSortOrderStep;
        MakeAddRowLambda(transaction, databaseHelper, DbSchema::kNeighborhoodsTable)(
            { DbPair(DbSchema::kNeighborhoodsSlug, seed.slug),
              DbPair(DbSchema::kNeighborhoodsDisplayName, seed.displayName),
              DbPair(DbSchema::kNeighborhoodsRegion, seed.region),
              DbPair(DbSchema::kNeighborhoodsSortOrder, std::to_string(sortOrder)) });
    }
}

// The admin CRUD editor's view of the app tables. Every table goes in
// admin_top_level_tables FIRST: admin_column_data_info and
// admin_column_friendly_names carry a foreign key onto it, so column metadata
// for an unregistered table fails the seed outright (knottyyoga's #1 trap).
void PopulateAdminMetadata(
    Transaction& transaction, DatabaseHelper databaseHelper) {
    for (std::string_view table : AppTableCreationOrder()) {
        MakeAddRowLambda(
            transaction, databaseHelper, DbSchema::kAdminTopLevelTablesTable)(
            { DbPair(DbSchema::kAdminTopLevelTablesName, table) });
    }

    // --- Column data-info: edit types, labels, and hints ---------------------
    {
        auto AddRow = [&](std::string_view tableName, std::string_view columnName,
                          std::string_view label, std::string_view hint,
                          std::string_view htmlInputType,
                          std::string_view required,
                          std::string_view readonly_ = "") {
            KeyValueTable kvt = {
                DbPair(DbSchema::kAdminColumnDataInfoTableName, tableName),
                DbPair(DbSchema::kAdminColumnDataInfoColumnName, columnName),
                DbPair(DbSchema::kAdminColumnDataInfoLabel, label),
                DbPair(DbSchema::kAdminColumnDataInfoHint, hint),
                DbPair(DbSchema::kAdminColumnDataInfoHtmlInputType, htmlInputType),
                DbPair(DbSchema::kAdminColumnDataInfoRequired, required) };
            if (!readonly_.empty())
                AddToKeyValueTable(
                    kvt, DbSchema::kAdminColumnDataInfoReadonly, readonly_);
            MakeAddRowLambda(
                transaction, databaseHelper, DbSchema::kAdminColumnDataInfoTable)(kvt);
        };

        constexpr std::string_view kSlugHint =
            "Stable kebab-case identifier used in URLs and filters. Renaming it "
            "breaks links and needs a cross-stream ping; edit the display name "
            "instead.";
        constexpr std::string_view kSortOrderHint =
            "Lower numbers list first (seeded in steps of 10)";

        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesDisplayName, "Display Name", "Name shown to visitors", "text", "true");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesDescription, "Description", "Optional longer description", "text", "false");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesSortOrder, "Sort Order", kSortOrderHint, "number", "true");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesEnabled, "Enabled", "Whether this category is offered in pickers and filters", "bool", "true");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesCreatedUs, "Created", "When this category was created", "date", "false", "true");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesUpdatedUs, "Updated", "When this category was last updated", "date", "false", "true");

        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsDisplayName, "Display Name", "Name shown to visitors", "text", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsDescription, "Description", "Optional longer description", "text", "false");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsCoverage, "Coverage", "full = we curate this scene ourselves; link_out = we list its shared anchors and link out, with credit, to the scene's own curators", "text", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsSortOrder, "Sort Order", kSortOrderHint, "number", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsEnabled, "Enabled", "Whether this scene tag is offered in pickers and filters", "bool", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsCreatedUs, "Created", "When this scene tag was created", "date", "false", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsUpdatedUs, "Updated", "When this scene tag was last updated", "date", "false", "true");

        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsDisplayName, "Display Name", "Name shown to visitors", "text", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsRegion, "Region", "seattle-core | seattle-north | seattle-south | eastside | south-king", "text", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsSortOrder, "Sort Order", kSortOrderHint, "number", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsEnabled, "Enabled", "Whether this neighborhood is offered in pickers and filters", "bool", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsCreatedUs, "Created", "When this neighborhood was created", "date", "false", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsUpdatedUs, "Updated", "When this neighborhood was last updated", "date", "false", "true");

        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesName, "Name", "Name of the venue", "text", "true");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesDescription, "Description", "Short factual description (the guide's editorial lives on its venue profile)", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesAddress, "Address", "Street address", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesCity, "City", "City", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesState, "State", "Two-letter state code", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesZip, "ZIP", "ZIP code", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesUrl, "Website", "The venue's own website", "text", "false");
        // Text, not number: an HTML number input steps by whole numbers and would
        // refuse a coordinate like 47.6145.
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesLatitude, "Latitude", "Decimal degrees, e.g. 47.6145", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesLongitude, "Longitude", "Decimal degrees, e.g. -122.3205", "text", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesNeighborhoodId, "Neighborhood", "Neighborhood the venue is in (neighborhoods.id); leave empty if it varies", "number", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesStatus, "Status", "open | closed | moved | seasonal | unverified. Close a venue rather than deleting it — deleting also deletes its events.", "text", "true");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesLastVerifiedAtUs, "Verified", "When a person last confirmed this venue's facts. Set it only when you have checked them; routine edits leave it alone.", "date", "false");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesVolatilityTier, "Re-check Cadence", "weekly | monthly | quarterly | semiannual | annual: how often the facts need re-verifying", "text", "true");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesCreatedUs, "Created", "When this venue was created", "date", "false", "true");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesUpdatedUs, "Updated", "When this venue was last updated", "date", "false", "true");
    }

    // --- Column friendly names: the list view's headers ----------------------
    {
        auto AddRow = [&](std::string_view tableName, std::string_view columnName,
                          std::string_view friendlyName) {
            MakeAddRowLambda(transaction, databaseHelper,
                DbSchema::kAdminColumnFriendlyNamesTable)(
                { DbPair(DbSchema::kAdminColumnFriendlyNamesTableName, tableName),
                  DbPair(DbSchema::kAdminColumnFriendlyNamesColumnName, columnName),
                  DbPair(DbSchema::kAdminColumnFriendlyNamesFriendlyName, friendlyName) });
        };
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesId, "ID");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesSlug, "Slug");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesDisplayName, "Display Name");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesDescription, "Description");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesSortOrder, "Sort Order");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesEnabled, "Enabled");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesCreatedUs, "Created");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesUpdatedUs, "Updated");

        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsId, "ID");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsSlug, "Slug");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsDisplayName, "Display Name");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsDescription, "Description");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsCoverage, "Coverage");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsSortOrder, "Sort Order");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsEnabled, "Enabled");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsCreatedUs, "Created");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsUpdatedUs, "Updated");

        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsId, "ID");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsSlug, "Slug");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsDisplayName, "Display Name");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsRegion, "Region");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsSortOrder, "Sort Order");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsEnabled, "Enabled");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsCreatedUs, "Created");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsUpdatedUs, "Updated");

        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesId, "ID");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesName, "Name");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesDescription, "Description");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesAddress, "Address");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesCity, "City");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesState, "State");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesZip, "ZIP");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesUrl, "Website");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesLatitude, "Latitude");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesLongitude, "Longitude");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesNeighborhoodId, "Neighborhood");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesStatus, "Status");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesCreatedUs, "Created");
        AddRow(DbSchema::kVenuesTable, DbSchema::kVenuesUpdatedUs, "Updated");
    }

    // --- Table friendly names ------------------------------------------------
    {
        auto AddRow = [&](std::string_view name, std::string_view friendlyName,
                          std::string_view description) {
            MakeAddRowLambda(transaction, databaseHelper,
                DbSchema::kAdminTableFriendlyNamesTable)(
                { DbPair(DbSchema::kAdminTableFriendlyNamesName, name),
                  DbPair(DbSchema::kAdminTableFriendlyNamesFriendlyName, friendlyName),
                  DbPair(DbSchema::kAdminTableFriendlyNamesDescription, description) });
        };
        AddRow(DbSchema::kCategoriesTable, "Categories", "What kind of thing is happening (bar night, drag, sports, ...). Shared by events and the guide.");
        AddRow(DbSchema::kSceneTagsTable, "Scene Tags", "Whose crowd it is (bears, leather, sober, ...), with each scene's coverage posture. Shared by events and the guide.");
        AddRow(DbSchema::kNeighborhoodsTable, "Neighborhoods", "King County neighborhoods, grouped by region. Shared by events and the guide.");
        AddRow(DbSchema::kVenuesTable, "Venues", "Places events happen. The guide's venue profiles attach to these.");
    }

    // --- Table display templates: how a row reads in an FK picker ------------
    {
        auto AddRow = [&](std::string_view name, std::string_view displayTemplate) {
            MakeAddRowLambda(transaction, databaseHelper,
                DbSchema::kAdminTableDisplayTemplateTable)(
                { DbPair(DbSchema::kAdminTableDisplayTemplateName, name),
                  DbPair(DbSchema::kAdminTableDisplayTemplateTemplate, displayTemplate) });
        };
        AddRow(DbSchema::kCategoriesTable, "{display_name}");
        AddRow(DbSchema::kSceneTagsTable, "{display_name}");
        AddRow(DbSchema::kNeighborhoodsTable, "{display_name}");
        AddRow(DbSchema::kVenuesTable, "{name}");
    }
}

// Tables anyone, signed in or not, may read through the framework's generic
// table-read endpoints. EV1 OQ1 (Option A): the site fetches the vocabularies
// this way rather than through bespoke list endpoints, so they must be here.
// Everything listed holds public display data only.
void PopulateAllowedTables(
    Transaction& transaction, DatabaseHelper databaseHelper) {
    for (std::string_view table :
         { DbSchema::kCategoriesTable, DbSchema::kSceneTagsTable,
           DbSchema::kNeighborhoodsTable, DbSchema::kVenuesTable }) {
        MakeAddRowLambda(
            transaction, databaseHelper, DbSchema::kAllowedTablesTable)(
            { DbPair(DbSchema::kAllowedTablesName, table) });
    }
}

}  // namespace

const std::vector<std::string_view>& AppTableCreationOrder() {
    static const std::vector<std::string_view> tables = {
        // EV1 Slice 1 — the shared vocabularies + venues.
        DbSchema::kCategoriesTable,
        DbSchema::kSceneTagsTable,
        DbSchema::kNeighborhoodsTable,
        DbSchema::kVenuesTable,
    };
    return tables;
}

void CreateAppTables(
    Transaction& transaction, DbSchema::DatabaseInfo databaseInfo) {
    for (std::string_view tableName : AppTableCreationOrder()) {
        DbOps::DropIfExistsAndCreateTable(transaction, databaseInfo, tableName);
    }
    // Indexes exist only on a live database — the test harness builds tables
    // from MakeDatabaseInfo and never calls this.
    DbSchema::CreateVenuesIndexes(transaction);
}

void PopulateAppTables(
    Transaction& transaction, DatabaseHelper databaseHelper,
    DbSchema::DatabaseInfo databaseInfo) {
    (void)databaseInfo;
    PopulateVocabularies(transaction, databaseHelper);
    PopulateAdminMetadata(transaction, databaseHelper);
    PopulateAllowedTables(transaction, databaseHelper);
}
