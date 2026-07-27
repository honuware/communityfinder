#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// Registers every app (CommunityFinder domain) table into the shared
	// DatabaseInfo. **Currently empty** — CommunityFinder has no app tables yet;
	// its first domain tables (events) arrive in Phase 10. The function exists so
	// the app composition root (MakeDatabaseInfo) can call it unconditionally.
	//
	// Must be called AFTER MakeFrameworkTables: app tables reference framework
	// tables (people, permissions, ...) as FK parents, which are validated
	// eagerly, so the framework schema must already be registered.
	//
	// DatabaseInfo is a shared-pImpl handle, so passing by value mutates the same
	// underlying schema — matching the MakeXxxTable convention.
	void MakeAppTables(DatabaseInfo databaseInfo);

}  // namespace DbSchema
