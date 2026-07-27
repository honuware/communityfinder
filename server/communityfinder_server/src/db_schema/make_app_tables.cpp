#include "make_app_tables.h"

namespace DbSchema {

	void MakeAppTables(DatabaseInfo databaseInfo) {
		// Intentionally empty. CommunityFinder has no app tables yet; the first
		// domain tables (events) arrive in Phase 10's events domain. When they
		// do, register them here with the MakeXxxTable(databaseInfo) helpers, in
		// FK-topological order (parents before children).
		(void)databaseInfo;
	}

}  // namespace DbSchema
