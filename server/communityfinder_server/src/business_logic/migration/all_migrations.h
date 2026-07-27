#pragma once

#include <vector>

#include "business_logic/migration/migration_runner.h"

namespace Migration {

// Returns the canonical list of all migrations the application knows about,
// in the order they should be applied. Each migration carries a unique,
// *namespaced* id ("honuware/0002_…" or "app/0002_…", see
// migration_namespace.h) and an `apply` callback that performs the schema
// change inside a transaction.
//
// This is the app-side **composition root**: it concatenates the honuware
// framework stream (BuildFrameworkMigrations, from honuware_platform) followed
// by the app stream (BuildAppMigrations, app-side). Because the two streams live
// in disjoint id namespaces, honuware upgrades and app schema changes can be
// evolved independently and applied to every tenant DB in a loop without
// colliding.
//
// **Currently empty** (both streams). The fresh-install schema is established by
// the `--recreate_database` path of the database helper. Migrations are added
// only to evolve the schema *between* released versions on a database that
// already has customer data — which can't happen until after the first
// production deploy.
//
// When you add a migration, add it to the correct stream (framework_migrations
// or app_migrations), NOT here:
//   1. Pick the next local id in that stream ("0002_xxx", "0003_xxx", …).
//   2. Append `{ NamespacedMigrationId(<namespace>, "000N_xxx"), apply }`.
//   3. Add a test in that stream's test coverage verifying the new id.
std::vector<Migration> BuildAllMigrations();

}  // namespace Migration
