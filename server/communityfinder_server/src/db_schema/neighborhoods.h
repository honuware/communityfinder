#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 1 — the shared neighborhood vocabulary (GD1 §3): King County
	// neighborhoods, grouped by region, with the Eastside first-class. Venues
	// and the guide's entities reference it by nullable FK (null = location
	// varies / online / countywide). Seeded from GD1 verbatim.
	inline constexpr std::string_view kNeighborhoodsTable = "neighborhoods";
	inline constexpr std::string_view kNeighborhoodsId = "id";
	inline constexpr std::string_view kNeighborhoodsSlug = "slug";
	inline constexpr std::string_view kNeighborhoodsDisplayName = "display_name";
	// Groups the neighborhood pickers and GD3's neighborhood index.
	inline constexpr std::string_view kNeighborhoodsRegion = "region";
	inline constexpr std::string_view kNeighborhoodsSortOrder = "sort_order";
	inline constexpr std::string_view kNeighborhoodsEnabled = "enabled";
	inline constexpr std::string_view kNeighborhoodsCreatedUs = "created_us";
	inline constexpr std::string_view kNeighborhoodsUpdatedUs = "updated_us";

	inline constexpr std::string_view kNeighborhoodsRegionSeattleCore = "seattle-core";
	inline constexpr std::string_view kNeighborhoodsRegionSeattleNorth = "seattle-north";
	inline constexpr std::string_view kNeighborhoodsRegionSeattleSouth = "seattle-south";
	inline constexpr std::string_view kNeighborhoodsRegionEastside = "eastside";
	inline constexpr std::string_view kNeighborhoodsRegionSouthKing = "south-king";
	inline constexpr std::string_view kNeighborhoodsRegionCheck = "neighborhoods_region_check";

	void MakeNeighborhoodsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
