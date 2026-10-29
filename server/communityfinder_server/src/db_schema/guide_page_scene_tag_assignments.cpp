#include "guide_page_scene_tag_assignments.h"

#include "guide_pages.h"
#include "scene_tags.h"

namespace DbSchema {

	void MakeGuidePageSceneTagAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kGuidePageSceneTagAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kGuidePageSceneTagAssignmentsTable,
			kGuidePageSceneTagAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kGuidePagesTable,
			kGuidePagesId,
			kGuidePageSceneTagAssignmentsTable,
			kGuidePageSceneTagAssignmentsGuidePageId);
		databaseInfo.AddColumnForeignKeyRef(
			kSceneTagsTable,
			kSceneTagsId,
			kGuidePageSceneTagAssignmentsTable,
			kGuidePageSceneTagAssignmentsSceneTagId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePageSceneTagAssignmentsTable,
			kGuidePageSceneTagAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (guide page, scene tag). The leading column also
		// serves the per-page lookups.
		databaseInfo.AddUniqueConstraint(
			kGuidePageSceneTagAssignmentsTable,
			kGuidePageSceneTagAssignmentsGuidePageId,
			kGuidePageSceneTagAssignmentsSceneTagId);
	}

}  // namespace DbSchema
