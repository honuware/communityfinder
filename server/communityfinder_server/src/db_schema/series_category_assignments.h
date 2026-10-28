#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — a series' categories, inherited by every instance that has
	// no categories of its own (EV1 OQ5).
	inline constexpr std::string_view kSeriesCategoryAssignmentsTable = "series_category_assignments";
	inline constexpr std::string_view kSeriesCategoryAssignmentsId = "id";
	inline constexpr std::string_view kSeriesCategoryAssignmentsSeriesId = "series_id";
	inline constexpr std::string_view kSeriesCategoryAssignmentsCategoryId = "category_id";
	inline constexpr std::string_view kSeriesCategoryAssignmentsCreatedUs = "created_us";

	void MakeSeriesCategoryAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
