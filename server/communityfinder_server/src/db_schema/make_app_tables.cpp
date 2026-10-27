#include "make_app_tables.h"

#include "categories.h"
#include "neighborhoods.h"
#include "scene_tags.h"
#include "venues.h"

namespace DbSchema {

	void MakeAppTables(DatabaseInfo databaseInfo) {
		// FK-topological order (parents before children) — DatabaseInfo validates
		// FK parents eagerly. Keep AppTableCreationOrder (create_app_tables.cpp)
		// in the same order; a test compares the two.

		// EV1 Slice 1 — the shared vocabularies (GD1 §1–§3) + venues.
		MakeCategoriesTable(databaseInfo);
		MakeSceneTagsTable(databaseInfo);
		MakeNeighborhoodsTable(databaseInfo);
		MakeVenuesTable(databaseInfo);
	}

}  // namespace DbSchema
