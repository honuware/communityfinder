#include "make_database_info.h"

#include "make_app_tables.h"
#include "db_schema/make_framework_tables.h"

namespace DbSchema {

/*
When adding an app table, please be sure to modify all three:
	make_app_tables.cpp    - MakeAppTables
	create_app_tables.cpp  - AppTableCreationOrder (what CreateTables builds)
	create_app_tables.cpp  - PopulateAppTables (admin metadata + seed)
*/
	// MakeDatabaseInfo is the *app* composition root. It stacks the two disjoint
	// table streams — framework (honuware) first, then app — into one
	// DatabaseInfo. Framework-first is a valid topological order because app
	// tables may reference framework tables as FK parents but never the reverse
	// (zero framework->app foreign keys), and DatabaseInfo validates FK parents
	// eagerly.
	DatabaseInfo MakeDatabaseInfo(std::string_view databaseName) {
		DatabaseInfo databaseInfo(databaseName);
		MakeFrameworkTables(databaseInfo);
		MakeAppTables(databaseInfo);
		return databaseInfo;
	}

}  // namespace DbSchema
