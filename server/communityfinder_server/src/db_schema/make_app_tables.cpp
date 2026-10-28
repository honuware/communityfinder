#include "make_app_tables.h"

#include "categories.h"
#include "event_category_assignments.h"
#include "event_scene_tag_assignments.h"
#include "event_sources.h"
#include "events.h"
#include "neighborhoods.h"
#include "scene_tags.h"
#include "series.h"
#include "series_category_assignments.h"
#include "series_scene_tag_assignments.h"
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

		// EV1 Slice 2 — the events core, then the four assignment tables.
		MakeEventSourcesTable(databaseInfo);
		MakeSeriesTable(databaseInfo);
		MakeEventsTable(databaseInfo);
		MakeEventCategoryAssignmentsTable(databaseInfo);
		MakeEventSceneTagAssignmentsTable(databaseInfo);
		MakeSeriesCategoryAssignmentsTable(databaseInfo);
		MakeSeriesSceneTagAssignmentsTable(databaseInfo);
	}

}  // namespace DbSchema
