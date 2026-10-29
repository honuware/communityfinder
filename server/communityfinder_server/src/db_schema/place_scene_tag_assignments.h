#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — a place's scene tags: whose crowd gathers there.
	inline constexpr std::string_view kPlaceSceneTagAssignmentsTable = "place_scene_tag_assignments";
	inline constexpr std::string_view kPlaceSceneTagAssignmentsId = "id";
	inline constexpr std::string_view kPlaceSceneTagAssignmentsPlaceId = "place_id";
	inline constexpr std::string_view kPlaceSceneTagAssignmentsSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kPlaceSceneTagAssignmentsCreatedUs = "created_us";

	void MakePlaceSceneTagAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
