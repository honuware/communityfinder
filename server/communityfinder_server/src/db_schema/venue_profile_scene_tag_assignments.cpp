#include "venue_profile_scene_tag_assignments.h"

#include "scene_tags.h"
#include "venue_profiles.h"

namespace DbSchema {

	void MakeVenueProfileSceneTagAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kVenueProfileSceneTagAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kVenueProfileSceneTagAssignmentsTable,
			kVenueProfileSceneTagAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kVenueProfilesTable,
			kVenueProfilesId,
			kVenueProfileSceneTagAssignmentsTable,
			kVenueProfileSceneTagAssignmentsVenueProfileId);
		databaseInfo.AddColumnForeignKeyRef(
			kSceneTagsTable,
			kSceneTagsId,
			kVenueProfileSceneTagAssignmentsTable,
			kVenueProfileSceneTagAssignmentsSceneTagId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenueProfileSceneTagAssignmentsTable,
			kVenueProfileSceneTagAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (venue profile, scene tag). The leading column also
		// serves the per-profile lookups.
		databaseInfo.AddUniqueConstraint(
			kVenueProfileSceneTagAssignmentsTable,
			kVenueProfileSceneTagAssignmentsVenueProfileId,
			kVenueProfileSceneTagAssignmentsSceneTagId);
	}

}  // namespace DbSchema
