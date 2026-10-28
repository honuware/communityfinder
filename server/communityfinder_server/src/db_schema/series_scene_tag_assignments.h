#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — a series' scene tags, inherited by every instance that has
	// no scene tags of its own (EV1 OQ5).
	inline constexpr std::string_view kSeriesSceneTagAssignmentsTable = "series_scene_tag_assignments";
	inline constexpr std::string_view kSeriesSceneTagAssignmentsId = "id";
	inline constexpr std::string_view kSeriesSceneTagAssignmentsSeriesId = "series_id";
	inline constexpr std::string_view kSeriesSceneTagAssignmentsSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kSeriesSceneTagAssignmentsCreatedUs = "created_us";

	void MakeSeriesSceneTagAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
