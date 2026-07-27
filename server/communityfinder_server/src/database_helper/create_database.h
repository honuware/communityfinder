#pragma once

#include "sql_util/database_access/database_helper.h"
#include "sql_util/schema/database_info.h"

// Drops + recreates the app's PRIMARY database and fills it with the full schema
// (framework + app tables) and seed data — the `--recreate_database` path.
void CreateAndPopulateDatabases();

// Creates the full schema + seed data in the GIVEN (already-created, empty)
// database, inside its own transaction. Does NOT create or drop the database.
// This is the reusable create+populate callable the multi-community tenant
// provisioner (Phase 14) will run against each new community database, so every
// community gets the identical schema + seed as `--recreate_database` produces
// for the primary DB.
void CreateSchemaAndPopulate(
    DatabaseHelper databaseHelper, DbSchema::DatabaseInfo databaseInfo);
