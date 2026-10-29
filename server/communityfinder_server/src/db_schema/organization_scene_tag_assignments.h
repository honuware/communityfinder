#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — an organization's scene tags: whose crowd it serves.
	inline constexpr std::string_view kOrganizationSceneTagAssignmentsTable = "organization_scene_tag_assignments";
	inline constexpr std::string_view kOrganizationSceneTagAssignmentsId = "id";
	inline constexpr std::string_view kOrganizationSceneTagAssignmentsOrganizationId = "organization_id";
	inline constexpr std::string_view kOrganizationSceneTagAssignmentsSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kOrganizationSceneTagAssignmentsCreatedUs = "created_us";

	void MakeOrganizationSceneTagAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
