#include "organization_scene_tag_assignments.h"

#include "organizations.h"
#include "scene_tags.h"

namespace DbSchema {

	void MakeOrganizationSceneTagAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kOrganizationSceneTagAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kOrganizationSceneTagAssignmentsTable,
			kOrganizationSceneTagAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kOrganizationsTable,
			kOrganizationsId,
			kOrganizationSceneTagAssignmentsTable,
			kOrganizationSceneTagAssignmentsOrganizationId);
		databaseInfo.AddColumnForeignKeyRef(
			kSceneTagsTable,
			kSceneTagsId,
			kOrganizationSceneTagAssignmentsTable,
			kOrganizationSceneTagAssignmentsSceneTagId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationSceneTagAssignmentsTable,
			kOrganizationSceneTagAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (organization, scene tag). The leading column also
		// serves the per-organization lookups.
		databaseInfo.AddUniqueConstraint(
			kOrganizationSceneTagAssignmentsTable,
			kOrganizationSceneTagAssignmentsOrganizationId,
			kOrganizationSceneTagAssignmentsSceneTagId);
	}

}  // namespace DbSchema
