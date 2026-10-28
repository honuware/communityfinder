#include "series_scene_tag_assignments.h"

#include "scene_tags.h"
#include "series.h"

namespace DbSchema {

	void MakeSeriesSceneTagAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kSeriesSceneTagAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kSeriesSceneTagAssignmentsTable,
			kSeriesSceneTagAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kSeriesTable,
			kSeriesId,
			kSeriesSceneTagAssignmentsTable,
			kSeriesSceneTagAssignmentsSeriesId);
		databaseInfo.AddColumnForeignKeyRef(
			kSceneTagsTable,
			kSceneTagsId,
			kSeriesSceneTagAssignmentsTable,
			kSeriesSceneTagAssignmentsSceneTagId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesSceneTagAssignmentsTable,
			kSeriesSceneTagAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (series, scene tag). The leading column also serves
		// the per-series lookups.
		databaseInfo.AddUniqueConstraint(
			kSeriesSceneTagAssignmentsTable,
			kSeriesSceneTagAssignmentsSeriesId,
			kSeriesSceneTagAssignmentsSceneTagId);
	}

}  // namespace DbSchema
