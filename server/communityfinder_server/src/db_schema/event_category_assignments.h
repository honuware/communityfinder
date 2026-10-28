#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — an event's own categories. When an event has any, they
	// replace its series' categories for that event; when it has none, the
	// series' set applies (EV1 OQ5: override per axis, never merge).
	inline constexpr std::string_view kEventCategoryAssignmentsTable = "event_category_assignments";
	inline constexpr std::string_view kEventCategoryAssignmentsId = "id";
	inline constexpr std::string_view kEventCategoryAssignmentsEventId = "event_id";
	inline constexpr std::string_view kEventCategoryAssignmentsCategoryId = "category_id";
	inline constexpr std::string_view kEventCategoryAssignmentsCreatedUs = "created_us";

	void MakeEventCategoryAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
