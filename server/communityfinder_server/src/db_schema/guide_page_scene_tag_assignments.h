#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — a guide page's scene tags: which scenes' pages it belongs on.
	inline constexpr std::string_view kGuidePageSceneTagAssignmentsTable = "guide_page_scene_tag_assignments";
	inline constexpr std::string_view kGuidePageSceneTagAssignmentsId = "id";
	inline constexpr std::string_view kGuidePageSceneTagAssignmentsGuidePageId = "guide_page_id";
	inline constexpr std::string_view kGuidePageSceneTagAssignmentsSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kGuidePageSceneTagAssignmentsCreatedUs = "created_us";

	void MakeGuidePageSceneTagAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
