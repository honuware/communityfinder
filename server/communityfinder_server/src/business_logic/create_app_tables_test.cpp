#include "business_logic/create_app_tables.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/admin_column_data_info.h"
#include "db_schema/admin_table_display_template.h"
#include "db_schema/admin_table_friendly_names.h"
#include "db_schema/admin_top_level_tables.h"
#include "db_schema/allowed_tables.h"
#include "db_schema/categories.h"
#include "db_schema/make_database_info.h"
#include "db_schema/make_framework_tables.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/scene_tags.h"
#include "db_schema/venues.h"
#include "sql_util/database_access/transaction.h"
#include "test/src/util/database_test_helper.h"

// The app halves of create_database. The harness pre-creates tables from
// MakeDatabaseInfo but seeds no rows, so these run PopulateAppTables in a test
// transaction — the only place the gate executes the seed. Running it is itself a
// check: every admin-metadata row lands against the database's own foreign keys.
namespace {

using Rows = StringTable;

Rows Query(Transaction& transaction, const std::string& sql) {
    return transaction.RunSqlStatementReturningStringTable(sql);
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
        PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());

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
        PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());

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
        PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());

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
        PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());

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
        PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());

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
    });
}

}  // namespace
