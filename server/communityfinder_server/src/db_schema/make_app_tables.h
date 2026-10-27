#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// Registers every app (CommunityFinder domain) table into the shared
	// DatabaseInfo, in FK order.
	//
	// Must be called AFTER MakeFrameworkTables: app tables reference framework
	// tables (people, permissions, ...) as FK parents, which are validated
	// eagerly, so the framework schema must already be registered.
	//
	// DatabaseInfo is a shared-pImpl handle, so passing by value mutates the same
	// underlying schema — matching the MakeXxxTable convention.
	void MakeAppTables(DatabaseInfo databaseInfo);

}  // namespace DbSchema
