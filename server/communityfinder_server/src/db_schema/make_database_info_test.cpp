#include "make_database_info.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "make_app_tables.h"
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

TEST(MakeDatabaseInfoTest, AppStreamIsEmpty) {
    // CommunityFinder has no app tables yet (they arrive in Phase 10's events
    // domain), so MakeAppTables adds nothing on its own.
    DatabaseInfo appOnly("test_db");
    MakeAppTables(appOnly);
    EXPECT_TRUE(appOnly.GetAllTables().empty());
}

TEST(MakeDatabaseInfoTest, ComposedSchemaIsExactlyTheFrameworkTableSet) {
    // Framework-only schema.
    DatabaseInfo frameworkOnly("test_db");
    MakeFrameworkTables(frameworkOnly);
    StringArray frameworkTables = frameworkOnly.GetAllTables();

    // Full composed schema.
    DatabaseInfo full = MakeDatabaseInfo("test_db");
    StringArray fullTables = full.GetAllTables();

    // Because the app stream is empty, the composed schema is exactly the
    // framework table set — same tables, same order. This simultaneously
    // establishes that the composed set contains precisely the framework tables
    // and that framework tables precede app tables (the app suffix is empty).
    EXPECT_EQ(fullTables, frameworkTables);
}

TEST(MakeDatabaseInfoTest, NoDuplicateTables) {
    DatabaseInfo databaseInfo = MakeDatabaseInfo("test_db");
    StringArray tables = databaseInfo.GetAllTables();
    std::set<std::string> unique(tables.begin(), tables.end());
    EXPECT_EQ(unique.size(), tables.size());
}

}  // namespace
}  // namespace DbSchema
