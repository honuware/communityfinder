#include "event_scene_tag_assignments.h"

#include "events.h"
#include "scene_tags.h"

namespace DbSchema {

	void MakeEventSceneTagAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kEventSceneTagAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kEventSceneTagAssignmentsTable,
			kEventSceneTagAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kEventsTable,
			kEventsId,
			kEventSceneTagAssignmentsTable,
			kEventSceneTagAssignmentsEventId);
		databaseInfo.AddColumnForeignKeyRef(
			kSceneTagsTable,
			kSceneTagsId,
			kEventSceneTagAssignmentsTable,
			kEventSceneTagAssignmentsSceneTagId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSceneTagAssignmentsTable,
			kEventSceneTagAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (event, scene tag). The leading column also serves
		// the per-event lookups.
		databaseInfo.AddUniqueConstraint(
			kEventSceneTagAssignmentsTable,
			kEventSceneTagAssignmentsEventId,
			kEventSceneTagAssignmentsSceneTagId);
	}

}  // namespace DbSchema
