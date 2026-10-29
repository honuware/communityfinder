#include "business_logic/create_app_tables.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "db_schema/admin_column_data_info.h"
#include "db_schema/admin_column_friendly_names.h"
#include "db_schema/admin_nested_tables.h"
#include "db_schema/admin_table_display_template.h"
#include "db_schema/admin_table_friendly_names.h"
#include "db_schema/admin_table_permissions.h"
#include "db_schema/admin_top_level_tables.h"
#include "db_schema/allowed_tables.h"
#include "db_schema/categories.h"
#include "db_schema/event_category_assignments.h"
#include "db_schema/event_scene_tag_assignments.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/guide_links.h"
#include "db_schema/guide_page_scene_tag_assignments.h"
#include "db_schema/guide_pages.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organization_scene_tag_assignments.h"
#include "db_schema/organizations.h"
#include "db_schema/permissions.h"
#include "db_schema/photos.h"
#include "db_schema/place_scene_tag_assignments.h"
#include "db_schema/places.h"
#include "db_schema/role_permissions.h"
#include "db_schema/roles.h"
#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/series_category_assignments.h"
#include "db_schema/series_scene_tag_assignments.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profile_scene_tag_assignments.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "sql_util/database_access/db_and_table_operations.h"
#include "sql_util/database_common.h"
#include "sql_util/table_helpers/permissions.h"
#include "sql_util/table_helpers/roles.h"
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
        // `extras` carries the rarer settings, e.g. kTallTextBox below.
        auto AddRow = [&](std::string_view tableName, std::string_view columnName,
                          std::string_view label, std::string_view hint,
                          std::string_view htmlInputType,
                          std::string_view required,
                          std::string_view readonly_ = "",
                          const KeyValueTable& extras = {}) {
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
            kvt.insert(extras.begin(), extras.end());
            MakeAddRowLambda(
                transaction, databaseHelper, DbSchema::kAdminColumnDataInfoTable)(kvt);
        };
        // A page body gets a tall box; long-text shows 3 lines by default.
        const KeyValueTable kTallTextBox = {
            DbPair(DbSchema::kAdminColumnDataInfoRows, "20") };
        // A new row's checkbox shows default_value. An untouched box is left out
        // of the insert, so the database DEFAULT still applies, but without this
        // a column that defaults TRUE would show unchecked.
        const KeyValueTable kCheckedByDefault = {
            DbPair(DbSchema::kAdminColumnDataInfoDefaultValue, "true") };

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
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesEnabled, "Enabled", "Whether this category is offered in pickers and filters", "bool", "true", "", kCheckedByDefault);
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesCreatedUs, "Created", "When this category was created", "date", "false", "true");
        AddRow(DbSchema::kCategoriesTable, DbSchema::kCategoriesUpdatedUs, "Updated", "When this category was last updated", "date", "false", "true");

        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsDisplayName, "Display Name", "Name shown to visitors", "text", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsDescription, "Description", "Optional longer description", "text", "false");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsCoverage, "Coverage", "full = we curate this scene ourselves; link_out = we list its shared anchors and link out, with credit, to the scene's own curators", "text", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsSortOrder, "Sort Order", kSortOrderHint, "number", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsEnabled, "Enabled", "Whether this scene tag is offered in pickers and filters", "bool", "true", "", kCheckedByDefault);
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsCreatedUs, "Created", "When this scene tag was created", "date", "false", "true");
        AddRow(DbSchema::kSceneTagsTable, DbSchema::kSceneTagsUpdatedUs, "Updated", "When this scene tag was last updated", "date", "false", "true");

        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsDisplayName, "Display Name", "Name shown to visitors", "text", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsRegion, "Region", "seattle-core | seattle-north | seattle-south | eastside | south-king", "text", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsSortOrder, "Sort Order", kSortOrderHint, "number", "true");
        AddRow(DbSchema::kNeighborhoodsTable, DbSchema::kNeighborhoodsEnabled, "Enabled", "Whether this neighborhood is offered in pickers and filters", "bool", "true", "", kCheckedByDefault);
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

        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesName, "Name", "Name of the source, e.g. the venue or feed it reads", "text", "true");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesUrl, "URL", "Where the source is read: the feed, calendar page, or profile", "text", "false");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesKind, "Kind", "ics_feed | venue_site | ticketing_org | aggregator | editorial_monitor | submission | social", "text", "true");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesVenueId, "Venue", "The venue this source covers, when it covers exactly one (venues.id)", "number", "false");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesEnabled, "Enabled", "Whether the scanner reads this source", "bool", "true", "", kCheckedByDefault);
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesScanHints, "Scan Hints", "Notes for the scanner: what to read and what to skip", "text", "false");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesVolatilityTier, "Scan Cadence", "weekly | monthly | quarterly | semiannual | annual: how often to re-scan", "text", "true");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesLastScannedAtUs, "Last Scanned", "When the scanner last read this source", "date", "false", "true");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesNotes, "Notes", "Internal notes about this source", "text", "false");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesCreatedUs, "Created", "When this source was created", "date", "false", "true");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesUpdatedUs, "Updated", "When this source was last updated", "date", "false", "true");

        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesTitle, "Title", "Name of the series, e.g. T4T", "text", "true");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesDescription, "Description", "What the series is", "text", "false");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesCadenceText, "Cadence", "How it recurs, in words, e.g. 2nd Saturdays. Display only; it does not create events.", "text", "false");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesUrl, "Website", "The series' own page", "text", "false");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesVenueId, "Venue", "Where it usually happens (venues.id); leave empty if it moves around", "number", "false");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesStatus, "Status", "open | closed | moved | seasonal | unverified. Close a series rather than deleting it — deleting also deletes all its events.", "text", "true");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesLastVerifiedAtUs, "Verified", "When a person last confirmed the series is still running as described. Set it only when you have checked; routine edits leave it alone.", "date", "false");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesVolatilityTier, "Re-check Cadence", "weekly | monthly | quarterly | semiannual | annual: how often the series needs re-verifying", "text", "true");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesCreatedUs, "Created", "When this series was created", "date", "false", "true");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesUpdatedUs, "Updated", "When this series was last updated", "date", "false", "true");

        AddRow(DbSchema::kEventsTable, DbSchema::kEventsTitle, "Title", "Name of the event", "text", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsDescription, "Description", "What the event is", "text", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsStartsAtUs, "Starts", "When it starts", "date", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsEndsAtUs, "Ends", "When it ends; leave empty if no end is stated", "date", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsVenueId, "Venue", "Where it happens (venues.id); for a place with no venue row, leave empty and fill in Location", "number", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsLocationText, "Location", "Where it is when there is no venue row, e.g. a park", "text", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsUrl, "Website", "The event page or tickets", "text", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsCostText, "Cost", "As stated, e.g. Free, or $10–20 sliding scale", "text", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsSourceId, "Source", "The source it was ingested from (event_sources.id); empty for hand-entered events", "number", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsSeriesId, "Series", "The series this is an instance of (series.id); empty for a one-off", "number", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsExternalKey, "External Key", "The source's own id for this item, set by the ingest. Leave empty for hand-entered events.", "text", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsOrigin, "Origin", "scanned | manual | user_submitted", "text", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsStatus, "Status", "pending | approved | rejected | archived. Only approved events are public.", "text", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsCreatedByPersonId, "Created By", "The person who entered it (people.id)", "number", "false", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsReviewedByPersonId, "Reviewed By", "The person who last approved or rejected it (people.id)", "number", "false", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsReviewedUs, "Reviewed", "When it was last approved or rejected", "date", "false", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsReviewNotes, "Review Notes", "Why it was approved or rejected. A re-scan that changes the date or venue adds a note here when it sends the event back to pending.", "text", "false");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsCreatedUs, "Created", "When this event was created", "date", "false", "true");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsUpdatedUs, "Updated", "When this event was last updated", "date", "false", "true");

        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsEventId, "Event", "The event (events.id)", "number", "true");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsCategoryId, "Category", "The category (categories.id). An event's own categories replace its series' categories.", "number", "true");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsEventId, "Event", "The event (events.id)", "number", "true");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsSceneTagId, "Scene Tag", "The scene tag (scene_tags.id). An event's own scene tags replace its series' scene tags.", "number", "true");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsSeriesId, "Series", "The series (series.id)", "number", "true");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsCategoryId, "Category", "The category (categories.id). Instances without categories of their own inherit these.", "number", "true");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsSeriesId, "Series", "The series (series.id)", "number", "true");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsSceneTagId, "Scene Tag", "The scene tag (scene_tags.id). Instances without scene tags of their own inherit these.", "number", "true");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        // --- Stream B (the guide, GD2) ---------------------------------------
        constexpr std::string_view kStatusHint =
            "open | closed | moved | seasonal | unverified. Close it rather than "
            "deleting it: closed listings stay visible, and deleting also deletes "
            "its scene tags and links.";
        constexpr std::string_view kVerifiedHint =
            "When a person last confirmed these facts. Set it only when you have "
            "checked them; routine edits leave it alone.";
        constexpr std::string_view kCadenceHint =
            "weekly | monthly | quarterly | semiannual | annual: how often the facts "
            "need re-verifying";
        constexpr std::string_view kNeighborhoodHint =
            "Neighborhood it is in (neighborhoods.id); leave empty if countywide or "
            "it moves around";
        constexpr std::string_view kLinkParentHint =
            "Set exactly one of Venue Profile, Organization, Place, or Scene Tag: "
            "the page this link shows on";

        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesVenueId, "Venue", "The venue this profile describes (venues.id). One profile per venue; its name and status live on the venue.", "number", "true");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesCrowdVibe, "Crowd & Vibe", "Whose crowd it draws, the vibe, and when to go: the guide's take on this venue", "long-text", "false");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesPriceBand, "Price Band", "$, $$, or $$$; leave empty if unknown", "text", "false");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesAdult, "18+", "Puts this venue's guide page behind the 18+ gate", "bool", "true");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesClosureNote, "Closure Note", "For a closed venue, the story the graveyard tells, e.g. Closed 2021, lease lost; the owners opened a new bar. Close the venue itself on its venue row.", "long-text", "false");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesLastVerifiedAtUs, "Verified", "When a person last confirmed the guide facts (crowd, vibe, price). Set it only when you have checked them; routine edits leave it alone.", "date", "false");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesVolatilityTier, "Re-check Cadence", kCadenceHint, "text", "true");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesCreatedUs, "Created", "When this profile was created", "date", "false", "true");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesUpdatedUs, "Updated", "When this profile was last updated", "date", "false", "true");

        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsName, "Name", "Name of the organization", "text", "true");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsOrgKind, "Kind", "sports-league | chorus-performance | club-hobby | recovery | faith | professional | community-support | social-meetup", "text", "true");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsDescription, "Description", "What the organization is and does", "long-text", "false");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsUrl, "Website", "The organization's own site", "text", "false");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsMeetingCadenceText, "Meets", "When and where it meets, in words, e.g. Mondays 6:30 PM, Cal Anderson. Display only.", "text", "false");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsNeighborhoodId, "Neighborhood", kNeighborhoodHint, "number", "false");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsStatus, "Status", kStatusHint, "text", "true");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsClosureNote, "Closure Note", "For a closed organization, the story the graveyard tells, e.g. Disbanded 2022; most members joined another league", "long-text", "false");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsLastVerifiedAtUs, "Verified", kVerifiedHint, "date", "false");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsVolatilityTier, "Re-check Cadence", kCadenceHint, "text", "true");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsCreatedUs, "Created", "When this organization was created", "date", "false", "true");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsUpdatedUs, "Updated", "When this organization was last updated", "date", "false", "true");

        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesName, "Name", "Name of the place", "text", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesPlaceKind, "Kind", "park-beach | landmark | historical", "text", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesDescription, "Description", "A short factual description (the full story goes on a guide page)", "long-text", "false");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesAddressText, "Location", "Where it is, in words: an address, or directions within a park", "text", "false");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesNeighborhoodId, "Neighborhood", kNeighborhoodHint, "number", "false");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesAdult, "18+", "Puts this place's guide page behind the 18+ gate", "bool", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesStatus, "Status", kStatusHint, "text", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesLastVerifiedAtUs, "Verified", kVerifiedHint, "date", "false");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesVolatilityTier, "Re-check Cadence", "weekly | monthly | quarterly | semiannual | annual: how often the facts need re-verifying. A place in the news (litigation, a rule change) is monthly until it settles.", "text", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesCreatedUs, "Created", "When this place was created", "date", "false", "true");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesUpdatedUs, "Updated", "When this place was last updated", "date", "false", "true");

        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsName, "Name", "Name of the clinic, program, or business", "text", "true");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsSlug, "Slug", kSlugHint, "text", "true");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsServiceKind, "Kind", "sti-testing | prep | hiv-care | primary-care | mental-health | gender-affirming | dental | everyday-service", "text", "true");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsDescription, "Description", "What it offers and who it serves", "long-text", "false");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsUrl, "Website", "Its own site or booking page", "text", "false");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsPhone, "Phone", "Phone number as people should dial it", "text", "false");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsAddressText, "Address", "Street address, or where to find it", "text", "false");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsNeighborhoodId, "Neighborhood", kNeighborhoodHint, "number", "false");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsIsHealth, "Health Care", "Checked for any health service: no person is ever shown recommending a health listing. Uncheck only for everyday services.", "bool", "true", "", kCheckedByDefault);
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsStatus, "Status", kStatusHint, "text", "true");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsLastVerifiedAtUs, "Verified", kVerifiedHint, "date", "false");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsVolatilityTier, "Re-check Cadence", "weekly | monthly | quarterly | semiannual | annual: how often the facts need re-verifying. Health listings are semiannual; set an everyday service to annual.", "text", "true");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsCreatedUs, "Created", "When this listing was created", "date", "false", "true");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsUpdatedUs, "Updated", "When this listing was last updated", "date", "false", "true");

        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesTitle, "Title", "The page's headline", "text", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesSlug, "Slug", "Stable kebab-case identifier used in the page's URL. A scene's intro uses the scene tag's slug (section scenes); a neighborhood's rundown uses the neighborhood's slug (section new-to-seattle). Renaming it breaks links.", "text", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesSection, "Section", "scenes | health | services | new-to-seattle | freeze | new-to | adult", "text", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesBodyMarkdown, "Body", "The page in Markdown. Link to other guide pages with plain relative links, e.g. [The Cuff](/guide/venues/the-cuff).", "long-text", "false", "", kTallTextBox);
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesAdult, "18+", "Puts the page behind the 18+ gate. Required for every page in the adult section.", "bool", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesDraft, "Draft", "A draft is never public. To publish, uncheck this and set Publish At.", "bool", "true", "", kCheckedByDefault);
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesPublishAtUs, "Publish At", "When the page goes public, once it is not a draft. Leave empty to keep it unpublished.", "date", "false");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesStatus, "Status", "open | closed | moved | seasonal | unverified. unverified = the page's facts need a check.", "text", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesLastVerifiedAtUs, "Verified", "When a person last confirmed the page's facts. Set it only when you have checked them; routine edits leave it alone.", "date", "false");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesVolatilityTier, "Re-check Cadence", "weekly | monthly | quarterly | semiannual | annual: how often the facts need re-verifying. By section: adult monthly, health semiannual, everything else annual.", "text", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesCreatedUs, "Created", "When this page was created", "date", "false", "true");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesUpdatedUs, "Updated", "When this page was last updated", "date", "false", "true");

        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId, "Venue Profile", "The venue profile (venue_profiles.id)", "number", "true");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsSceneTagId, "Scene Tag", "The scene tag (scene_tags.id): whose crowd the venue draws", "number", "true");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsOrganizationId, "Organization", "The organization (organizations.id)", "number", "true");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsSceneTagId, "Scene Tag", "The scene tag (scene_tags.id): whose crowd the organization serves", "number", "true");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsPlaceId, "Place", "The place (places.id)", "number", "true");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsSceneTagId, "Scene Tag", "The scene tag (scene_tags.id): whose crowd gathers there", "number", "true");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsGuidePageId, "Guide Page", "The guide page (guide_pages.id)", "number", "true");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsSceneTagId, "Scene Tag", "The scene tag (scene_tags.id): a scene whose pages this page belongs on", "number", "true");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsCreatedUs, "Created", "When this assignment was created", "date", "false", "true");

        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksLabel, "Label", "The link's text, e.g. Seattle Bears Discord", "text", "true");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksUrl, "URL", "Where the link goes", "text", "true");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksPlatform, "Platform", "discord | bluesky | instagram | facebook-group | meetup | website | other", "text", "true");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksSortOrder, "Sort Order", "Lower numbers list first among one page's links", "number", "true");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksVenueProfileId, "Venue Profile", kLinkParentHint, "number", "false");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksOrganizationId, "Organization", kLinkParentHint, "number", "false");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksPlaceId, "Place", kLinkParentHint, "number", "false");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksSceneTagId, "Scene Tag", kLinkParentHint, "number", "false");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksCreatedUs, "Created", "When this link was created", "date", "false", "true");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksUpdatedUs, "Updated", "When this link was last updated", "date", "false", "true");
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

        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesId, "ID");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesName, "Name");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesUrl, "URL");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesKind, "Kind");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesVenueId, "Venue");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesEnabled, "Enabled");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesScanHints, "Scan Hints");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesVolatilityTier, "Scan Cadence");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesLastScannedAtUs, "Last Scanned");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesNotes, "Notes");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesCreatedUs, "Created");
        AddRow(DbSchema::kEventSourcesTable, DbSchema::kEventSourcesUpdatedUs, "Updated");

        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesId, "ID");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesTitle, "Title");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesSlug, "Slug");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesDescription, "Description");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesCadenceText, "Cadence");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesUrl, "Website");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesVenueId, "Venue");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesStatus, "Status");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesCreatedUs, "Created");
        AddRow(DbSchema::kSeriesTable, DbSchema::kSeriesUpdatedUs, "Updated");

        AddRow(DbSchema::kEventsTable, DbSchema::kEventsId, "ID");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsTitle, "Title");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsDescription, "Description");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsStartsAtUs, "Starts");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsEndsAtUs, "Ends");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsVenueId, "Venue");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsLocationText, "Location");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsUrl, "Website");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsCostText, "Cost");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsSourceId, "Source");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsSeriesId, "Series");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsExternalKey, "External Key");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsOrigin, "Origin");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsStatus, "Status");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsCreatedByPersonId, "Created By");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsReviewedByPersonId, "Reviewed By");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsReviewedUs, "Reviewed");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsReviewNotes, "Review Notes");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsCreatedUs, "Created");
        AddRow(DbSchema::kEventsTable, DbSchema::kEventsUpdatedUs, "Updated");

        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsId, "ID");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsEventId, "Event");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsCategoryId, "Category");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, DbSchema::kEventCategoryAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsId, "ID");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsEventId, "Event");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsSceneTagId, "Scene Tag");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, DbSchema::kEventSceneTagAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsId, "ID");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsSeriesId, "Series");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsCategoryId, "Category");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, DbSchema::kSeriesCategoryAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsId, "ID");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsSeriesId, "Series");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsSceneTagId, "Scene Tag");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, DbSchema::kSeriesSceneTagAssignmentsCreatedUs, "Created");

        // --- Stream B (the guide, GD2) ---------------------------------------
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesId, "ID");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesVenueId, "Venue");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesSlug, "Slug");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesCrowdVibe, "Crowd & Vibe");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesPriceBand, "Price Band");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesAdult, "18+");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesClosureNote, "Closure Note");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesCreatedUs, "Created");
        AddRow(DbSchema::kVenueProfilesTable, DbSchema::kVenueProfilesUpdatedUs, "Updated");

        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsId, "ID");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsName, "Name");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsSlug, "Slug");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsOrgKind, "Kind");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsDescription, "Description");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsUrl, "Website");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsMeetingCadenceText, "Meets");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsNeighborhoodId, "Neighborhood");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsStatus, "Status");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsClosureNote, "Closure Note");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsCreatedUs, "Created");
        AddRow(DbSchema::kOrganizationsTable, DbSchema::kOrganizationsUpdatedUs, "Updated");

        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesId, "ID");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesName, "Name");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesSlug, "Slug");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesPlaceKind, "Kind");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesDescription, "Description");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesAddressText, "Location");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesNeighborhoodId, "Neighborhood");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesAdult, "18+");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesStatus, "Status");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesCreatedUs, "Created");
        AddRow(DbSchema::kPlacesTable, DbSchema::kPlacesUpdatedUs, "Updated");

        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsId, "ID");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsName, "Name");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsSlug, "Slug");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsServiceKind, "Kind");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsDescription, "Description");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsUrl, "Website");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsPhone, "Phone");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsAddressText, "Address");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsNeighborhoodId, "Neighborhood");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsIsHealth, "Health Care");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsStatus, "Status");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsCreatedUs, "Created");
        AddRow(DbSchema::kServiceListingsTable, DbSchema::kServiceListingsUpdatedUs, "Updated");

        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesId, "ID");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesTitle, "Title");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesSlug, "Slug");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesSection, "Section");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesBodyMarkdown, "Body");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesAdult, "18+");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesDraft, "Draft");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesPublishAtUs, "Publish At");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesStatus, "Status");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesLastVerifiedAtUs, "Verified");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesVolatilityTier, "Re-check Cadence");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesCreatedUs, "Created");
        AddRow(DbSchema::kGuidePagesTable, DbSchema::kGuidePagesUpdatedUs, "Updated");

        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsId, "ID");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId, "Venue Profile");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsSceneTagId, "Scene Tag");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, DbSchema::kVenueProfileSceneTagAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsId, "ID");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsOrganizationId, "Organization");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsSceneTagId, "Scene Tag");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, DbSchema::kOrganizationSceneTagAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsId, "ID");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsPlaceId, "Place");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsSceneTagId, "Scene Tag");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, DbSchema::kPlaceSceneTagAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsId, "ID");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsGuidePageId, "Guide Page");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsSceneTagId, "Scene Tag");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, DbSchema::kGuidePageSceneTagAssignmentsCreatedUs, "Created");

        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksId, "ID");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksLabel, "Label");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksUrl, "URL");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksPlatform, "Platform");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksSortOrder, "Sort Order");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksVenueProfileId, "Venue Profile");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksOrganizationId, "Organization");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksPlaceId, "Place");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksSceneTagId, "Scene Tag");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksCreatedUs, "Created");
        AddRow(DbSchema::kGuideLinksTable, DbSchema::kGuideLinksUpdatedUs, "Updated");
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
        AddRow(DbSchema::kEventSourcesTable, "Event Sources", "Where events come from (feeds, venue sites, ticketing orgs) and how often each is re-scanned.");
        AddRow(DbSchema::kSeriesTable, "Event Series", "Recurring events' durable pages, e.g. T4T, 2nd Saturdays. Instances inherit a series' categories and scene tags.");
        AddRow(DbSchema::kEventsTable, "Events", "One row per dated occurrence. Only approved events are public.");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, "Event Categories", "An event's own categories. When an event has any, they replace its series' categories.");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, "Event Scene Tags", "An event's own scene tags. When an event has any, they replace its series' scene tags.");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, "Series Categories", "A series' categories, inherited by instances that have none of their own.");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, "Series Scene Tags", "A series' scene tags, inherited by instances that have none of their own.");

        // --- Stream B (the guide, GD2) ---------------------------------------
        AddRow(DbSchema::kVenueProfilesTable, "Venue Profiles", "The guide's take on a venue: crowd, vibe, price, the 18+ flag, and the closure story. One per venue.");
        AddRow(DbSchema::kOrganizationsTable, "Organizations", "Leagues, choruses, clubs, recovery groups, congregations, and other groups people join.");
        AddRow(DbSchema::kPlacesTable, "Places", "Parks, beaches, landmarks, and historical sites. The full story goes on a guide page.");
        AddRow(DbSchema::kServiceListingsTable, "Service Listings", "Clinics, testing, therapists, and everyday services. No person is ever shown recommending a health listing.");
        AddRow(DbSchema::kGuidePagesTable, "Guide Pages", "The guide's articles, in Markdown. A page is public only once it is not a draft and its publish time has passed.");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, "Venue Profile Scene Tags", "Whose crowd a venue draws.");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, "Organization Scene Tags", "Whose crowd an organization serves.");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, "Place Scene Tags", "Whose crowd gathers at a place.");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, "Guide Page Scene Tags", "The scenes whose pages a guide page belongs on.");
        AddRow(DbSchema::kGuideLinksTable, "Guide Links", "Where a scene talks: links out to the Discord, Instagram, or Meetup group it actually uses. Each link belongs to one venue profile, organization, place, or scene tag.");
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
        AddRow(DbSchema::kEventSourcesTable, "{name}");
        AddRow(DbSchema::kSeriesTable, "{title}");
        AddRow(DbSchema::kEventsTable, "{title}");
        AddRow(DbSchema::kEventCategoryAssignmentsTable, "{event_id} — {category_id}");
        AddRow(DbSchema::kEventSceneTagAssignmentsTable, "{event_id} — {scene_tag_id}");
        AddRow(DbSchema::kSeriesCategoryAssignmentsTable, "{series_id} — {category_id}");
        AddRow(DbSchema::kSeriesSceneTagAssignmentsTable, "{series_id} — {scene_tag_id}");

        // --- Stream B (the guide, GD2) ---------------------------------------
        // A profile has no name of its own (the venue's is authoritative), so
        // it reads as its slug.
        AddRow(DbSchema::kVenueProfilesTable, "{slug}");
        AddRow(DbSchema::kOrganizationsTable, "{name}");
        AddRow(DbSchema::kPlacesTable, "{name}");
        AddRow(DbSchema::kServiceListingsTable, "{name}");
        AddRow(DbSchema::kGuidePagesTable, "{title}");
        AddRow(DbSchema::kVenueProfileSceneTagAssignmentsTable, "{venue_profile_id} — {scene_tag_id}");
        AddRow(DbSchema::kOrganizationSceneTagAssignmentsTable, "{organization_id} — {scene_tag_id}");
        AddRow(DbSchema::kPlaceSceneTagAssignmentsTable, "{place_id} — {scene_tag_id}");
        AddRow(DbSchema::kGuidePageSceneTagAssignmentsTable, "{guide_page_id} — {scene_tag_id}");
        AddRow(DbSchema::kGuideLinksTable, "{label}");
    }

    // --- Nested tables: edited from their parent's row -----------------------
    // The assignment tables open under an event or a series rather than as
    // tables of their own. They stay top-level too (above): the column metadata
    // FKs onto admin_top_level_tables.
    //
    // The editor shows a nested table under every table it has an FK onto: a
    // guide link under its venue profile, organization, place, or scene tag, and
    // a venue profile under its venue.
    {
        for (std::string_view table :
             { DbSchema::kEventCategoryAssignmentsTable,
               DbSchema::kEventSceneTagAssignmentsTable,
               DbSchema::kSeriesCategoryAssignmentsTable,
               DbSchema::kSeriesSceneTagAssignmentsTable,
               DbSchema::kVenueProfilesTable,
               DbSchema::kVenueProfileSceneTagAssignmentsTable,
               DbSchema::kOrganizationSceneTagAssignmentsTable,
               DbSchema::kPlaceSceneTagAssignmentsTable,
               DbSchema::kGuidePageSceneTagAssignmentsTable,
               DbSchema::kGuideLinksTable }) {
            MakeAddRowLambda(
                transaction, databaseHelper, DbSchema::kAdminNestedTablesTable)(
                { DbPair(DbSchema::kAdminNestedTablesName, table) });
        }
    }
}

// The events domain: EV1's eleven tables. manage_events reaches every one of
// them in the generic CRUD editor. Listed explicitly rather than taken from
// AppTableCreationOrder, which will also hold the guide's tables (GD2).
constexpr std::string_view kEventsDomainTables[] = {
    DbSchema::kCategoriesTable,
    DbSchema::kSceneTagsTable,
    DbSchema::kNeighborhoodsTable,
    DbSchema::kVenuesTable,
    DbSchema::kEventSourcesTable,
    DbSchema::kSeriesTable,
    DbSchema::kEventsTable,
    DbSchema::kEventCategoryAssignmentsTable,
    DbSchema::kEventSceneTagAssignmentsTable,
    DbSchema::kSeriesCategoryAssignmentsTable,
    DbSchema::kSeriesSceneTagAssignmentsTable,
};

// The guide: GD2's ten tables, plus `venues`, because verifying a venue's facts
// is a guide curator's job (GD2 OQ3). So manage_events and manage_guide both
// reach venues.
constexpr std::string_view kManageGuideTables[] = {
    DbSchema::kVenueProfilesTable,
    DbSchema::kOrganizationsTable,
    DbSchema::kPlacesTable,
    DbSchema::kServiceListingsTable,
    DbSchema::kGuidePagesTable,
    DbSchema::kVenueProfileSceneTagAssignmentsTable,
    DbSchema::kOrganizationSceneTagAssignmentsTable,
    DbSchema::kPlaceSceneTagAssignmentsTable,
    DbSchema::kGuidePageSceneTagAssignmentsTable,
    DbSchema::kGuideLinksTable,
    DbSchema::kVenuesTable,
};

// One app permission (knottyyoga's pattern): a `permissions` row; a
// `role_permissions` grant to the Administrator role, looking both ids up by
// NAME because the framework assigns them; and an `admin_table_permissions` row
// for each table a non-admin holder should reach in the generic CRUD editor.
// Admins reach every top-level table through their role and need no such rows.
void AddAppPermission(
    Transaction& transaction, DatabaseHelper databaseHelper,
    std::string_view name, std::string_view description,
    std::span<const std::string_view> tables) {
    MakeAddRowLambda(transaction, databaseHelper, DbSchema::kPermissionsTable)(
        { DbPair(DbSchema::kPermissionsName, name),
          DbPair(DbSchema::kPermissionsDescription, description),
          DbPair(DbSchema::kPermissionsIsPricingEligible, "false") });

    TableHelpers::Roles rolesHelper(databaseHelper);
    TableHelpers::Permissions permissionsHelper(databaseHelper);
    const std::string adminRoleId =
        rolesHelper.GetRole(transaction, DbSchema::kRoleNameAdmin)
            .at(std::string(DbSchema::kRolesId));
    const std::string permissionId =
        permissionsHelper.GetPermission(transaction, name)
            .at(std::string(DbSchema::kPermissionsId));

    MakeAddRowLambda(transaction, databaseHelper, DbSchema::kRolePermissionsTable)(
        { DbPair(DbSchema::kRolePermissionsRoleId, adminRoleId),
          DbPair(DbSchema::kRolePermissionsPermissionId, permissionId) });

    for (std::string_view table : tables) {
        MakeAddRowLambda(
            transaction, databaseHelper, DbSchema::kAdminTablePermissionsTable)(
            { DbPair(DbSchema::kAdminTablePermissionsTableName, table),
              DbPair(DbSchema::kAdminTablePermissionsPermissionId, permissionId) });
    }
}

// App permissions beyond the framework base. honuware's PopulateFrameworkTables
// seeds the Administrator and User roles and the admin_portal / staff_access
// permissions; the app's own plug in here as the features that enforce them
// land. Next to land here: `manage_site_meta` (community settings,
// multi-community).
void PopulateAppPermissions(
    Transaction& transaction, DatabaseHelper databaseHelper) {
    AddAppPermission(
        transaction, databaseHelper, DbSchema::kPermissionManageEvents,
        "Permission to curate events: ingest and review them, and edit "
        "events, series, sources, venues, and the shared vocabularies.",
        kEventsDomainTables);
    AddAppPermission(
        transaction, databaseHelper, DbSchema::kPermissionManageGuide,
        "Permission to curate the guide: edit venue profiles, organizations, "
        "places, service listings, guide pages, their scene tags and links, and "
        "venues' facts; and preview unpublished guide pages.",
        kManageGuideTables);
}

// Tables whose rows can carry photos through the framework pipeline
// (table_item_photos). honuware lists `people`; venue, event, and guide images
// ride the same pipeline instead of image columns. A venue profile's photos
// are its venue's.
void PopulatePhotoSupportTables(
    Transaction& transaction, DatabaseHelper databaseHelper) {
    for (std::string_view table :
         { DbSchema::kVenuesTable, DbSchema::kEventsTable,
           DbSchema::kOrganizationsTable, DbSchema::kPlacesTable,
           DbSchema::kGuidePagesTable }) {
        MakeAddRowLambda(
            transaction, databaseHelper, DbSchema::kPhotoSupportTables)(
            { DbPair(DbSchema::kPhotoSupportTablesTableName, table) });
    }
}

// Tables anyone, signed in or not, may read through the framework's generic
// table-read endpoints. EV1 OQ1 (Option A): the site fetches the vocabularies
// this way rather than through bespoke list endpoints, so they must be here.
// Everything listed holds public display data only — which is why `events` (its
// pending and rejected rows, and its review notes), `series`, and
// `event_sources` are not: the public reads those through the events endpoints.
// No guide table is listed either (GD2 OQ4): a draft guide page must never leak
// through a generic read, so the public reads the guide through its own
// endpoints.
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
        // EV1 Slice 2 — the events core, then the four assignment tables.
        DbSchema::kEventSourcesTable,
        DbSchema::kSeriesTable,
        DbSchema::kEventsTable,
        DbSchema::kEventCategoryAssignmentsTable,
        DbSchema::kEventSceneTagAssignmentsTable,
        DbSchema::kSeriesCategoryAssignmentsTable,
        DbSchema::kSeriesSceneTagAssignmentsTable,
        // --- Stream B (the guide) ---
        // GD2 — the directory entities, the pages, their scene-tag
        // assignments, then the links.
        DbSchema::kVenueProfilesTable,
        DbSchema::kOrganizationsTable,
        DbSchema::kPlacesTable,
        DbSchema::kServiceListingsTable,
        DbSchema::kGuidePagesTable,
        DbSchema::kVenueProfileSceneTagAssignmentsTable,
        DbSchema::kOrganizationSceneTagAssignmentsTable,
        DbSchema::kPlaceSceneTagAssignmentsTable,
        DbSchema::kGuidePageSceneTagAssignmentsTable,
        DbSchema::kGuideLinksTable,
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
    DbSchema::CreateEventsIndexes(transaction);
}

void PopulateAppTables(
    Transaction& transaction, DatabaseHelper databaseHelper,
    DbSchema::DatabaseInfo databaseInfo) {
    (void)databaseInfo;
    PopulateVocabularies(transaction, databaseHelper);
    PopulateAdminMetadata(transaction, databaseHelper);
    PopulateAppPermissions(transaction, databaseHelper);
    PopulatePhotoSupportTables(transaction, databaseHelper);
    PopulateAllowedTables(transaction, databaseHelper);
}
