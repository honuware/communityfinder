#pragma once

#include <vector>

#include "business_logic/migration/migration_runner.h"

namespace Migration {

// The **app** migration stream (CommunityFinder). This builder is app-side (it
// evolves app tables — the events domain and whatever follows). Every migration
// it returns is recorded under the `kAppMigrationNamespace` id namespace (see
// NamespacedMigrationId), so it can never collide with a honuware framework
// migration. It is applied *after* the framework stream — see BuildAllMigrations.
//
// **Currently empty** — the fresh-install app schema is built by
// MakeDatabaseInfo + CreateTables; app migrations are added here only to evolve
// app tables between released versions. When you add one:
//   1. Pick the next app local id ("0001_...", "0002_...").
//   2. Append `{ NamespacedMigrationId(kAppMigrationNamespace, "0001_..."), apply }`.
std::vector<Migration> BuildAppMigrations();

}  // namespace Migration
