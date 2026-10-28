#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — an event's own scene tags. Same override rule as its
	// categories, applied independently: an event can keep its series'
	// categories while replacing its scene tags, or the reverse (EV1 OQ5).
	inline constexpr std::string_view kEventSceneTagAssignmentsTable = "event_scene_tag_assignments";
	inline constexpr std::string_view kEventSceneTagAssignmentsId = "id";
	inline constexpr std::string_view kEventSceneTagAssignmentsEventId = "event_id";
	inline constexpr std::string_view kEventSceneTagAssignmentsSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kEventSceneTagAssignmentsCreatedUs = "created_us";

	void MakeEventSceneTagAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
