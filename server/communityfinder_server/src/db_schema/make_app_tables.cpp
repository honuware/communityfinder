#include "make_app_tables.h"

#include "categories.h"
#include "event_category_assignments.h"
#include "event_scene_tag_assignments.h"
#include "event_sources.h"
#include "events.h"
#include "guide_links.h"
#include "guide_page_scene_tag_assignments.h"
#include "guide_pages.h"
#include "neighborhoods.h"
#include "organization_scene_tag_assignments.h"
#include "organizations.h"
#include "place_scene_tag_assignments.h"
#include "places.h"
#include "scene_tags.h"
#include "series.h"
#include "series_category_assignments.h"
#include "series_scene_tag_assignments.h"
#include "service_listings.h"
#include "venue_profile_scene_tag_assignments.h"
#include "venue_profiles.h"
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

		// --- Stream B (the guide) -------------------------------------------
		// GD2 — the directory entities, the CMS-lite pages, their scene-tag
		// assignments, then the links (each hangs off a venue profile, an
		// organization, a place, or a scene tag). Into Stream A's tables these
		// are FK references only: venues and the vocabularies.
		MakeVenueProfilesTable(databaseInfo);
		MakeOrganizationsTable(databaseInfo);
		MakePlacesTable(databaseInfo);
		MakeServiceListingsTable(databaseInfo);
		MakeGuidePagesTable(databaseInfo);
		MakeVenueProfileSceneTagAssignmentsTable(databaseInfo);
		MakeOrganizationSceneTagAssignmentsTable(databaseInfo);
		MakePlaceSceneTagAssignmentsTable(databaseInfo);
		MakeGuidePageSceneTagAssignmentsTable(databaseInfo);
		MakeGuideLinksTable(databaseInfo);
	}

}  // namespace DbSchema
