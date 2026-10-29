#pragma once

#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_access/transaction.h"
#include "sql_util/schema/database_info.h"

// The app halves of create_database, mirroring honuware's
// business_logic/create_framework_tables.h: create_database.cpp composes these
// after the framework halves.
//
// They live in communityfinder_core, not the database_helper executable, so a
// test can run them. The test harness builds its schema from MakeDatabaseInfo
// and never runs the seed, so without this nothing in the gate would execute the
// app's DDL list or its seed rows until a live --recreate_database.

// Creates the app tables (AppTableCreationOrder) and their indexes. Runs after
// CreateFrameworkTables: app tables may reference framework tables as FK
// parents, never the reverse.
void CreateAppTables(
    Transaction& transaction, DbSchema::DatabaseInfo databaseInfo);

// The tables CreateAppTables builds, in FK / creation order.
//
// Exposed so a test can compare it against MakeAppTables. honuware learned this
// on `site_assets`, which was registered in its schema but missing from the
// creation list, so no database ever had the table.
const std::vector<std::string_view>& AppTableCreationOrder();

// Seeds the app's own rows into the just-created tables:
//   - the shared vocabularies (categories, scene tags, neighborhoods), copied
//     verbatim from GD1 §1–§3,
//   - the admin CRUD editor's metadata for every app table: top-level
//     registration, column data-info + friendly names, table friendly names,
//     display templates, and the nested (child) tables,
//   - the app permissions, each granted to the framework Administrator role:
//     `manage_events`, mapped to every events-domain table, and
//     `manage_guide`, mapped to every guide table plus venues,
//   - photo support for venues, events, organizations, places, and guide pages,
//   - the public `allowed_tables` entries — the vocabularies and venues are
//     read anonymously through the framework's generic table reads (EV1 OQ1).
//     No guide table is public this way (GD2 OQ4).
// Runs after PopulateFrameworkTables, which seeds the Administrator role the
// grant looks up by name — so a test must run both, in that order.
void PopulateAppTables(
    Transaction& transaction, DatabaseHelper databaseHelper,
    DbSchema::DatabaseInfo databaseInfo);
