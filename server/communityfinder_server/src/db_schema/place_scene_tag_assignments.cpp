#include "place_scene_tag_assignments.h"

#include "places.h"
#include "scene_tags.h"

namespace DbSchema {

	void MakePlaceSceneTagAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kPlaceSceneTagAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kPlaceSceneTagAssignmentsTable,
			kPlaceSceneTagAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kPlacesTable,
			kPlacesId,
			kPlaceSceneTagAssignmentsTable,
			kPlaceSceneTagAssignmentsPlaceId);
		databaseInfo.AddColumnForeignKeyRef(
			kSceneTagsTable,
			kSceneTagsId,
			kPlaceSceneTagAssignmentsTable,
			kPlaceSceneTagAssignmentsSceneTagId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlaceSceneTagAssignmentsTable,
			kPlaceSceneTagAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (place, scene tag). The leading column also serves
		// the per-place lookups.
		databaseInfo.AddUniqueConstraint(
			kPlaceSceneTagAssignmentsTable,
			kPlaceSceneTagAssignmentsPlaceId,
			kPlaceSceneTagAssignmentsSceneTagId);
	}

}  // namespace DbSchema
