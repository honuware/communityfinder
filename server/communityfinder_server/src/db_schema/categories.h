#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 1 — the shared category vocabulary (GD1 §1): what kind of thing
	// is happening (bar night, drag, sports, ...). Orthogonal to scene tags; an
	// event may carry several. Seeded from GD1 verbatim and admin-editable after
	// that. Slugs are URLs and cross-stream references — renaming one needs a
	// cross-stream ping; display names change freely.
	inline constexpr std::string_view kCategoriesTable = "categories";
	inline constexpr std::string_view kCategoriesId = "id";
	inline constexpr std::string_view kCategoriesSlug = "slug";
	inline constexpr std::string_view kCategoriesDisplayName = "display_name";
	inline constexpr std::string_view kCategoriesDescription = "description";
	inline constexpr std::string_view kCategoriesSortOrder = "sort_order";
	inline constexpr std::string_view kCategoriesEnabled = "enabled";
	inline constexpr std::string_view kCategoriesCreatedUs = "created_us";
	inline constexpr std::string_view kCategoriesUpdatedUs = "updated_us";

	void MakeCategoriesTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
