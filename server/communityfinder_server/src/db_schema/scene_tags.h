#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 1 — the shared scene-tag vocabulary (GD1 §2): whose crowd it is
	// (bear, leather, sober, ...). Applies to events, series, and every guide
	// entity alike — the axis "By Scene" views and scene landing pages share.
	// Seeded from GD1 verbatim; slugs need a cross-stream ping to rename.
	inline constexpr std::string_view kSceneTagsTable = "scene_tags";
	inline constexpr std::string_view kSceneTagsId = "id";
	inline constexpr std::string_view kSceneTagsSlug = "slug";
	inline constexpr std::string_view kSceneTagsDisplayName = "display_name";
	inline constexpr std::string_view kSceneTagsDescription = "description";
	// The coverage posture: `full` = we curate this scene ourselves; `link_out`
	// = we list the shared anchors and link out, with credit, to the scene's own
	// curators. GD3 renders link-out scene landings from this column.
	inline constexpr std::string_view kSceneTagsCoverage = "coverage";
	inline constexpr std::string_view kSceneTagsSortOrder = "sort_order";
	inline constexpr std::string_view kSceneTagsEnabled = "enabled";
	inline constexpr std::string_view kSceneTagsCreatedUs = "created_us";
	inline constexpr std::string_view kSceneTagsUpdatedUs = "updated_us";

	inline constexpr std::string_view kSceneTagsCoverageFull = "full";
	inline constexpr std::string_view kSceneTagsCoverageLinkOut = "link_out";
	inline constexpr std::string_view kSceneTagsCoverageCheck = "scene_tags_coverage_check";

	void MakeSceneTagsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
