#include "business_logic/create_app_tables.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/create_framework_tables.h"
#include "db_schema/admin_column_data_info.h"
#include "db_schema/admin_nested_tables.h"
#include "db_schema/admin_table_display_template.h"
#include "db_schema/admin_table_friendly_names.h"
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
#include "db_schema/make_database_info.h"
#include "db_schema/make_framework_tables.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organization_scene_tag_assignments.h"
#include "db_schema/organizations.h"
#include "db_schema/photos.h"
#include "db_schema/place_scene_tag_assignments.h"
#include "db_schema/places.h"
#include "db_schema/roles.h"
#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/series_category_assignments.h"
#include "db_schema/series_scene_tag_assignments.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profile_scene_tag_assignments.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "sql_util/database_access/transaction.h"
#include "test/src/util/database_test_helper.h"

// The app halves of create_database. The harness pre-creates tables from
// MakeDatabaseInfo but seeds no rows, so these run the seed in a test
// transaction — the only place the gate executes it. Running it is itself a
// check: every admin-metadata row lands against the database's own foreign keys.
namespace {

using Rows = StringTable;

Rows Query(Transaction& transaction, const std::string& sql) {
    return transaction.RunSqlStatementReturningStringTable(sql);
}

// The seed exactly as --recreate_database runs it: the framework baseline
// first (it creates the Administrator role the app's manage_events grant looks
// up by name), then the app's rows.
void SeedAsRecreateDatabaseDoes(Transaction& transaction, TestDatabaseUtil& testDb) {
    PopulateFrameworkTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());
    PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());
}

// EV1's eleven tables — the ones manage_events reaches in the admin editor.
const std::set<std::string> kEventsDomainTables = {
    std::string(DbSchema::kCategoriesTable),
    std::string(DbSchema::kSceneTagsTable),
    std::string(DbSchema::kNeighborhoodsTable),
    std::string(DbSchema::kVenuesTable),
    std::string(DbSchema::kEventSourcesTable),
    std::string(DbSchema::kSeriesTable),
    std::string(DbSchema::kEventsTable),
    std::string(DbSchema::kEventCategoryAssignmentsTable),
    std::string(DbSchema::kEventSceneTagAssignmentsTable),
    std::string(DbSchema::kSeriesCategoryAssignmentsTable),
    std::string(DbSchema::kSeriesSceneTagAssignmentsTable),
};

// The tables whose rows are only ever edited from a parent's row.
const std::string_view kAssignmentTables[] = {
    DbSchema::kEventCategoryAssignmentsTable,
    DbSchema::kEventSceneTagAssignmentsTable,
    DbSchema::kSeriesCategoryAssignmentsTable,
    DbSchema::kSeriesSceneTagAssignmentsTable,
};

// GD2's ten tables.
const std::set<std::string> kGuideTables = {
    std::string(DbSchema::kVenueProfilesTable),
    std::string(DbSchema::kOrganizationsTable),
    std::string(DbSchema::kPlacesTable),
    std::string(DbSchema::kServiceListingsTable),
    std::string(DbSchema::kGuidePagesTable),
    std::string(DbSchema::kVenueProfileSceneTagAssignmentsTable),
    std::string(DbSchema::kOrganizationSceneTagAssignmentsTable),
    std::string(DbSchema::kPlaceSceneTagAssignmentsTable),
    std::string(DbSchema::kGuidePageSceneTagAssignmentsTable),
    std::string(DbSchema::kGuideLinksTable),
};

// One admin_column_data_info value for a column.
std::string ColumnDataInfo(
    Transaction& transaction, std::string_view field, std::string_view table,
    std::string_view column) {
    return transaction.RunSqlStatementReturningOneValue(
        "SELECT COALESCE(" + std::string(field) + "::TEXT, '') FROM " +
            std::string(DbSchema::kAdminColumnDataInfoTable) + " WHERE " +
            std::string(DbSchema::kAdminColumnDataInfoTableName) + " = $1 AND " +
            std::string(DbSchema::kAdminColumnDataInfoColumnName) + " = $2",
        std::string(table), std::string(column));
}

std::set<std::string> NamesIn(
    Transaction& transaction, std::string_view table, std::string_view column) {
    std::set<std::string> names;
    for (const auto& row : Query(transaction,
             "SELECT " + std::string(column) + " FROM " + std::string(table))) {
        names.insert(row.at(0));
    }
    return names;
}

// MakeAppTables says which tables EXIST; AppTableCreationOrder says which get
// BUILT by --recreate_database. Nothing else in the gate compares them (the
// harness builds from MakeDatabaseInfo), so a table missing from the creation
// list would pass every test and be absent from every live database. Exact
// order equality also makes the list FK-safe, because MakeAppTables' order is
// validated eagerly by DatabaseInfo.
TEST(CreateAppTablesTest, CreationOrderMatchesMakeAppTables) {
    DbSchema::DatabaseInfo frameworkOnly("framework_only");
    DbSchema::MakeFrameworkTables(frameworkOnly);
    const StringArray frameworkTables = frameworkOnly.GetAllTables();
    const StringArray allTables = DbSchema::MakeDatabaseInfo("composed").GetAllTables();
    ASSERT_GT(allTables.size(), frameworkTables.size());

    // The composed schema is framework-first (MakeDatabaseInfoTest pins that),
    // so the app tables are its suffix.
    const std::vector<std::string> registered(
        allTables.begin() + static_cast<std::ptrdiff_t>(frameworkTables.size()),
        allTables.end());
    std::vector<std::string> created;
    for (std::string_view table : AppTableCreationOrder()) {
        created.push_back(std::string(table));
    }
    EXPECT_EQ(created, registered)
        << "AppTableCreationOrder (create_app_tables.cpp) must list exactly the "
           "tables MakeAppTables registers, in the same order — otherwise "
           "--recreate_database builds a different schema than the tests run on.";
}

// GD1 §1, copied here by hand rather than read from the seed, so drift on either
// side fails. Sort order is part of the contract: it is display order.
TEST(CreateAppTablesTest, SeedsTheTwelveCategoriesVerbatimFromGd1) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SeedsCategories", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        EXPECT_EQ(Query(transaction,
                      "SELECT slug, display_name, enabled FROM categories "
                      "ORDER BY sort_order, id"),
                  (Rows{
                      {"bar", "Bar & club nights", "t"},
                      {"drag", "Drag", "t"},
                      {"nightlife", "Nightlife", "t"},
                      {"music", "Music & concerts", "t"},
                      {"theatre", "Theatre & film", "t"},
                      {"arts", "Arts & culture", "t"},
                      {"pride", "Pride & festivals", "t"},
                      {"sports", "Sports & recreation", "t"},
                      {"outdoors", "Outdoors", "t"},
                      {"volunteer", "Volunteering", "t"},
                      {"protest", "Protest & civic", "t"},
                      {"community", "Community & social", "t"},
                  }));
    });
}

// GD1 §2: eleven tags, of which exactly sapphic / trans / qtpoc are link-out.
TEST(CreateAppTablesTest, SeedsTheElevenSceneTagsWithTheirCoverageFromGd1) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SeedsSceneTags", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        EXPECT_EQ(Query(transaction,
                      "SELECT slug, display_name, coverage FROM scene_tags "
                      "ORDER BY sort_order, id"),
                  (Rows{
                      {"bear", "Bears", "full"},
                      {"leather", "Leather & kink", "full"},
                      {"jock", "Jocks & sports", "full"},
                      {"sober", "Sober", "full"},
                      {"elders", "Elders", "full"},
                      {"geek", "Geeks & gaymers", "full"},
                      {"furry", "Furries", "full"},
                      {"bipoc", "BIPOC", "full"},
                      {"sapphic", "Sapphic", "link_out"},
                      {"trans", "Trans", "link_out"},
                      {"qtpoc", "QTPOC", "link_out"},
                  }));
    });
}

// GD1 §3: twenty-five neighborhoods across five regions, Eastside first-class.
// GD1 pins slugs and regions only, so display names are not asserted here.
TEST(CreateAppTablesTest, SeedsTheNeighborhoodsWithTheirRegionsFromGd1) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SeedsNeighborhoods", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        EXPECT_EQ(Query(transaction,
                      "SELECT slug, region FROM neighborhoods ORDER BY sort_order, id"),
                  (Rows{
                      {"capitol-hill", "seattle-core"},
                      {"first-hill", "seattle-core"},
                      {"central-district", "seattle-core"},
                      {"downtown", "seattle-core"},
                      {"sodo", "seattle-core"},
                      {"queen-anne", "seattle-core"},
                      {"u-district", "seattle-core"},
                      {"wallingford", "seattle-core"},
                      {"ballard", "seattle-core"},
                      {"fremont", "seattle-core"},
                      {"beacon-hill", "seattle-south"},
                      {"columbia-city", "seattle-south"},
                      {"georgetown", "seattle-south"},
                      {"west-seattle", "seattle-south"},
                      {"white-center", "seattle-south"},
                      {"shoreline", "seattle-north"},
                      {"bellevue", "eastside"},
                      {"kirkland", "eastside"},
                      {"redmond", "eastside"},
                      {"bothell", "eastside"},
                      {"issaquah", "eastside"},
                      {"sammamish", "eastside"},
                      {"burien", "south-king"},
                      {"renton", "south-king"},
                      {"tukwila", "south-king"},
                  }));

        // Every seeded row got a display name.
        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT count(*) FROM neighborhoods WHERE display_name = ''"),
                  "0");
    });
}

// EV1 OQ1 (Option A): the site reads the vocabularies — and venues — through the
// framework's generic table reads, which serve exactly what allowed_tables lists.
TEST(CreateAppTablesTest, MakesTheVocabulariesAndVenuesPublic) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PublicTables", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> allowed = NamesIn(
            transaction, DbSchema::kAllowedTablesTable, DbSchema::kAllowedTablesName);
        for (std::string_view table :
             {DbSchema::kCategoriesTable, DbSchema::kSceneTagsTable,
              DbSchema::kNeighborhoodsTable, DbSchema::kVenuesTable}) {
            EXPECT_EQ(allowed.count(std::string(table)), 1u) << table;
        }
    });
}

// Every app table is editable in the admin CRUD editor: registered top-level
// (the allow list the generic CRUD endpoints check, and the parent key the
// column metadata hangs off), named, given an FK-picker template, and given
// column edit types.
TEST(CreateAppTablesTest, RegistersEveryAppTableWithTheAdminEditor) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AdminMetadata", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> topLevel = NamesIn(transaction,
            DbSchema::kAdminTopLevelTablesTable, DbSchema::kAdminTopLevelTablesName);
        const std::set<std::string> friendlyNamed = NamesIn(transaction,
            DbSchema::kAdminTableFriendlyNamesTable,
            DbSchema::kAdminTableFriendlyNamesName);
        const std::set<std::string> templated = NamesIn(transaction,
            DbSchema::kAdminTableDisplayTemplateTable,
            DbSchema::kAdminTableDisplayTemplateName);
        const std::set<std::string> described = NamesIn(transaction,
            DbSchema::kAdminColumnDataInfoTable,
            DbSchema::kAdminColumnDataInfoTableName);

        ASSERT_FALSE(AppTableCreationOrder().empty());
        for (std::string_view view : AppTableCreationOrder()) {
            const std::string table(view);
            EXPECT_EQ(topLevel.count(table), 1u) << table;
            EXPECT_EQ(friendlyNamed.count(table), 1u) << table;
            EXPECT_EQ(templated.count(table), 1u) << table;
            EXPECT_EQ(described.count(table), 1u) << table;
        }

        // FK pickers show names, not ids.
        auto displayTemplate = [&](std::string_view table) {
            return transaction.RunSqlStatementReturningOneValue(
                "SELECT " + std::string(DbSchema::kAdminTableDisplayTemplateTemplate) +
                    " FROM " + std::string(DbSchema::kAdminTableDisplayTemplateTable) +
                    " WHERE " + std::string(DbSchema::kAdminTableDisplayTemplateName) +
                    " = $1",
                std::string(table));
        };
        EXPECT_EQ(displayTemplate(DbSchema::kNeighborhoodsTable), "{display_name}");
        EXPECT_EQ(displayTemplate(DbSchema::kVenuesTable), "{name}");
        EXPECT_EQ(displayTemplate(DbSchema::kEventSourcesTable), "{name}");
        EXPECT_EQ(displayTemplate(DbSchema::kSeriesTable), "{title}");
        EXPECT_EQ(displayTemplate(DbSchema::kEventsTable), "{title}");

        // The verification stamp edits as a date, and the bookkeeping
        // timestamps cannot be edited at all.
        auto inputType = [&](std::string_view column) {
            return transaction.RunSqlStatementReturningOneValue(
                "SELECT html_input_type FROM admin_column_data_info "
                "WHERE table_name = $1 AND column_name = $2",
                std::string(DbSchema::kVenuesTable), std::string(column));
        };
        EXPECT_EQ(inputType(DbSchema::kVenuesLastVerifiedAtUs), "date");
        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT count(*) FROM admin_column_data_info "
                      "WHERE column_name IN ('created_us', 'updated_us') "
                      "AND readonly IS NOT TRUE"),
                  "0");

        // An event's times edit as dates, which the admin editor renders as a
        // date picker plus a time picker over the microsecond value.
        auto eventInputType = [&](std::string_view column) {
            return transaction.RunSqlStatementReturningOneValue(
                "SELECT html_input_type FROM admin_column_data_info "
                "WHERE table_name = $1 AND column_name = $2",
                std::string(DbSchema::kEventsTable), std::string(column));
        };
        EXPECT_EQ(eventInputType(DbSchema::kEventsStartsAtUs), "date");
        EXPECT_EQ(eventInputType(DbSchema::kEventsEndsAtUs), "date");
    });
}

// The assignment tables open under their event or series row. They are ALSO
// top-level, because the column metadata FKs onto admin_top_level_tables
// (knottyyoga's #1 trap) — RegistersEveryAppTableWithTheAdminEditor covers that.
TEST(CreateAppTablesTest, NestsTheAssignmentTablesUnderTheirParents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NestedTables", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> nested = NamesIn(transaction,
            DbSchema::kAdminNestedTablesTable, DbSchema::kAdminNestedTablesName);
        for (std::string_view table : kAssignmentTables) {
            EXPECT_EQ(nested.count(std::string(table)), 1u) << table;
        }
        // The parents themselves are not nested.
        EXPECT_EQ(nested.count(std::string(DbSchema::kEventsTable)), 0u);
        EXPECT_EQ(nested.count(std::string(DbSchema::kSeriesTable)), 0u);
    });
}

// EV1 §2.6: manage_events exists, the Administrator role holds it, and it opens
// exactly the eleven events-domain tables in the generic CRUD editor — so a
// future curator role can work without being a full admin.
TEST(CreateAppTablesTest, GrantsManageEventsToTheAdministratorRole) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ManageEvents", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT is_pricing_eligible FROM permissions WHERE name = $1",
                      std::string(DbSchema::kPermissionManageEvents)),
                  "f");
        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT count(*) FROM role_permissions rp "
                      "JOIN roles r ON r.id = rp.role_id "
                      "JOIN permissions p ON p.id = rp.permission_id "
                      "WHERE r.name = $1 AND p.name = $2",
                      std::string(DbSchema::kRoleNameAdmin),
                      std::string(DbSchema::kPermissionManageEvents)),
                  "1");

        std::set<std::string> gated;
        for (const auto& row : Query(transaction,
                 "SELECT atp.table_name FROM admin_table_permissions atp "
                 "JOIN permissions p ON p.id = atp.permission_id "
                 "WHERE p.name = 'manage_events'")) {
            gated.insert(row.at(0));
        }
        EXPECT_EQ(gated, kEventsDomainTables);
    });
}

// Venue and event images ride the framework photo pipeline; it serves only the
// tables listed in photo_support_tables.
TEST(CreateAppTablesTest, EnablesPhotosForVenuesAndEvents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PhotoSupport", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> photoTables = NamesIn(transaction,
            DbSchema::kPhotoSupportTables, DbSchema::kPhotoSupportTablesTableName);
        EXPECT_EQ(photoTables.count(std::string(DbSchema::kVenuesTable)), 1u);
        EXPECT_EQ(photoTables.count(std::string(DbSchema::kEventsTable)), 1u);
        // The framework's own entry survives alongside the app's.
        EXPECT_EQ(photoTables.count("people"), 1u);
    });
}

// The generic table reads serve allowed_tables to anyone. Pending and rejected
// events, review notes, and source scan notes must never be among them; the
// public sees approved events only, through the events endpoints.
TEST(CreateAppTablesTest, KeepsTheEventPipelineOffThePublicList) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PipelineNotPublic", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> allowed = NamesIn(
            transaction, DbSchema::kAllowedTablesTable, DbSchema::kAllowedTablesName);
        for (std::string_view table :
             {DbSchema::kEventSourcesTable, DbSchema::kSeriesTable,
              DbSchema::kEventsTable}) {
            EXPECT_EQ(allowed.count(std::string(table)), 0u) << table;
        }
        for (std::string_view table : kAssignmentTables) {
            EXPECT_EQ(allowed.count(std::string(table)), 0u) << table;
        }
    });
}

// Every column of every app table gets a list-view header, and every column but
// the id gets an edit form entry. A column left out still shows, but unlabeled
// and with no hint, and nothing else in the gate would notice.
TEST(CreateAppTablesTest, DescribesEveryColumnOfEveryAppTable) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EveryColumn", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        std::set<std::string> described;
        for (const auto& row : Query(transaction,
                 "SELECT table_name || '.' || column_name FROM admin_column_data_info")) {
            described.insert(row.at(0));
        }
        std::set<std::string> named;
        for (const auto& row : Query(transaction,
                 "SELECT table_name || '.' || column_name FROM "
                 "admin_column_friendly_names")) {
            named.insert(row.at(0));
        }

        for (std::string_view table : AppTableCreationOrder()) {
            for (const auto& column :
                 testDb.GetDatabaseInfo().GetTableInfo(table).GetColumns()) {
                const std::string name =
                    std::string(table) + "." + column.GetColumnName();
                EXPECT_EQ(named.count(name), 1u) << name;
                if (column.GetColumnName() != "id") {
                    EXPECT_EQ(described.count(name), 1u) << name;
                }
            }
        }
    });
}

// A new row's checkbox shows its data-info default_value, so every checkbox
// whose column defaults TRUE must say "true", and no other may. Without it the
// box shows unchecked while an untouched box still saves TRUE (GD2 OQ9 (e)).
TEST(CreateAppTablesTest, CheckboxesStartAsTheirColumnDefaults) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CheckboxDefaults", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        std::size_t checkedByDefault = 0;
        for (std::string_view table : AppTableCreationOrder()) {
            for (const auto& column :
                 testDb.GetDatabaseInfo().GetTableInfo(table).GetColumns()) {
                if (column.GetDatabaseType() != DB_TYPE_BOOL) continue;
                std::string defaultValue;
                const bool defaultsTrue = column.IsDefault(defaultValue) &&
                    defaultValue == DbSchema::kDatabaseInfoDefaultTrue;
                checkedByDefault += defaultsTrue ? 1 : 0;
                EXPECT_EQ(ColumnDataInfo(transaction, "default_value", table,
                                         column.GetColumnName()),
                          defaultsTrue ? "true" : "")
                    << table << "." << column.GetColumnName();
            }
        }
        // EV1's four Enabled boxes, a guide page's Draft, a listing's Health Care.
        EXPECT_EQ(checkedByDefault, 6u);
    });
}

// --- GD2: the guide ----------------------------------------------------------

// GD2 §1.9 and OQ3: manage_guide exists, the Administrator role holds it, and
// it opens exactly the ten guide tables plus venues in the generic CRUD editor.
TEST(CreateAppTablesTest, GrantsManageGuideToTheAdministratorRole) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ManageGuide", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT is_pricing_eligible FROM permissions WHERE name = $1",
                      std::string(DbSchema::kPermissionManageGuide)),
                  "f");
        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT count(*) FROM role_permissions rp "
                      "JOIN roles r ON r.id = rp.role_id "
                      "JOIN permissions p ON p.id = rp.permission_id "
                      "WHERE r.name = $1 AND p.name = $2",
                      std::string(DbSchema::kRoleNameAdmin),
                      std::string(DbSchema::kPermissionManageGuide)),
                  "1");

        std::set<std::string> gated;
        for (const auto& row : Query(transaction,
                 "SELECT atp.table_name FROM admin_table_permissions atp "
                 "JOIN permissions p ON p.id = atp.permission_id "
                 "WHERE p.name = 'manage_guide'")) {
            gated.insert(row.at(0));
        }
        std::set<std::string> expected = kGuideTables;
        expected.insert(std::string(DbSchema::kVenuesTable));
        EXPECT_EQ(gated, expected);

        // Both curator permissions reach venues.
        EXPECT_EQ(transaction.RunSqlStatementReturningOneValue(
                      "SELECT count(*) FROM admin_table_permissions "
                      "WHERE table_name = $1",
                      std::string(DbSchema::kVenuesTable)),
                  "2");
    });
}

// GD2 OQ4 (draft safety): the generic table reads serve allowed_tables to
// anyone, so no guide table may be on it. The public reads the guide through its
// own endpoints, which serve only published pages.
TEST(CreateAppTablesTest, KeepsTheGuideOffThePublicList) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GuideNotPublic", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> allowed = NamesIn(
            transaction, DbSchema::kAllowedTablesTable, DbSchema::kAllowedTablesName);
        for (const std::string& table : kGuideTables) {
            EXPECT_EQ(allowed.count(table), 0u) << table;
        }
    });
}

// Organization, place, and guide page images ride the framework photo pipeline.
// A venue profile shows its venue's photos, and service listings have none.
TEST(CreateAppTablesTest, EnablesPhotosForOrganizationsPlacesAndGuidePages) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GuidePhotos", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> photoTables = NamesIn(transaction,
            DbSchema::kPhotoSupportTables, DbSchema::kPhotoSupportTablesTableName);
        for (std::string_view table :
             {DbSchema::kOrganizationsTable, DbSchema::kPlacesTable,
              DbSchema::kGuidePagesTable}) {
            EXPECT_EQ(photoTables.count(std::string(table)), 1u) << table;
        }
        EXPECT_EQ(photoTables.count(std::string(DbSchema::kVenueProfilesTable)), 0u);
        EXPECT_EQ(photoTables.count(std::string(DbSchema::kServiceListingsTable)), 0u);
    });
}

// The scene-tag assignments and the links open under their parents' rows, and a
// venue profile opens under its venue. The entities themselves stay top-level
// only.
TEST(CreateAppTablesTest, NestsTheGuideAssignmentsLinksAndProfiles) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GuideNested", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        const std::set<std::string> nested = NamesIn(transaction,
            DbSchema::kAdminNestedTablesTable, DbSchema::kAdminNestedTablesName);
        for (std::string_view table :
             {DbSchema::kVenueProfilesTable,
              DbSchema::kVenueProfileSceneTagAssignmentsTable,
              DbSchema::kOrganizationSceneTagAssignmentsTable,
              DbSchema::kPlaceSceneTagAssignmentsTable,
              DbSchema::kGuidePageSceneTagAssignmentsTable,
              DbSchema::kGuideLinksTable}) {
            EXPECT_EQ(nested.count(std::string(table)), 1u) << table;
        }
        for (std::string_view table :
             {DbSchema::kOrganizationsTable, DbSchema::kPlacesTable,
              DbSchema::kServiceListingsTable, DbSchema::kGuidePagesTable}) {
            EXPECT_EQ(nested.count(std::string(table)), 0u) << table;
        }
    });
}

// How the guide's columns edit in the admin editor.
TEST(CreateAppTablesTest, DescribesTheGuideColumnsForTheAdminEditor) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GuideColumns", [&](Transaction& transaction) {
        SeedAsRecreateDatabaseDoes(transaction, testDb);

        auto displayTemplate = [&](std::string_view table) {
            return transaction.RunSqlStatementReturningOneValue(
                "SELECT " + std::string(DbSchema::kAdminTableDisplayTemplateTemplate) +
                    " FROM " + std::string(DbSchema::kAdminTableDisplayTemplateTable) +
                    " WHERE " + std::string(DbSchema::kAdminTableDisplayTemplateName) +
                    " = $1",
                std::string(table));
        };
        // A profile has no name of its own, so its picker shows the slug.
        EXPECT_EQ(displayTemplate(DbSchema::kVenueProfilesTable), "{slug}");
        EXPECT_EQ(displayTemplate(DbSchema::kOrganizationsTable), "{name}");
        EXPECT_EQ(displayTemplate(DbSchema::kPlacesTable), "{name}");
        EXPECT_EQ(displayTemplate(DbSchema::kServiceListingsTable), "{name}");
        EXPECT_EQ(displayTemplate(DbSchema::kGuidePagesTable), "{title}");
        EXPECT_EQ(displayTemplate(DbSchema::kGuideLinksTable), "{label}");

        // The page body edits in a tall Markdown box.
        EXPECT_EQ(ColumnDataInfo(transaction, "html_input_type",
                                 DbSchema::kGuidePagesTable,
                                 DbSchema::kGuidePagesBodyMarkdown),
                  "long-text");
        EXPECT_EQ(ColumnDataInfo(transaction, "rows", DbSchema::kGuidePagesTable,
                                 DbSchema::kGuidePagesBodyMarkdown),
                  "20");

        // The publish time and every verification stamp edit as dates.
        EXPECT_EQ(ColumnDataInfo(transaction, "html_input_type",
                                 DbSchema::kGuidePagesTable,
                                 DbSchema::kGuidePagesPublishAtUs),
                  "date");
        for (std::string_view table :
             {DbSchema::kVenueProfilesTable, DbSchema::kOrganizationsTable,
              DbSchema::kPlacesTable, DbSchema::kServiceListingsTable,
              DbSchema::kGuidePagesTable}) {
            EXPECT_EQ(ColumnDataInfo(transaction, "html_input_type", table,
                                     "last_verified_at_us"),
                      "date")
                << table;
        }
    });
}

}  // namespace
