#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — parks, beaches, landmarks, and historical sites: places with a
	// story but no business behind them (Denny Blaine, Howell Park). The
	// structured row; the editorial itself is a guide page.
	//
	// A listable table, so it carries the freshness convention (GD1 §5).
	inline constexpr std::string_view kPlacesTable = "places";
	inline constexpr std::string_view kPlacesId = "id";
	inline constexpr std::string_view kPlacesName = "name";
	// The place page's URL identity (/guide/places/{slug}).
	inline constexpr std::string_view kPlacesSlug = "slug";
	inline constexpr std::string_view kPlacesPlaceKind = "place_kind";
	inline constexpr std::string_view kPlacesDescription = "description";
	// Where it is, in words: a park has no street address to split up.
	inline constexpr std::string_view kPlacesAddressText = "address_text";
	inline constexpr std::string_view kPlacesNeighborhoodId = "neighborhood_id";
	// Drives the 18+ gate.
	inline constexpr std::string_view kPlacesAdult = "adult";
	inline constexpr std::string_view kPlacesStatus = "status";
	// Set only by a person confirming the facts. NULL = never verified.
	inline constexpr std::string_view kPlacesLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kPlacesVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kPlacesCreatedUs = "created_us";
	inline constexpr std::string_view kPlacesUpdatedUs = "updated_us";

	// GD1 §4's place kinds, verbatim.
	inline constexpr std::string_view kPlacesPlaceKindParkBeach = "park-beach";
	inline constexpr std::string_view kPlacesPlaceKindLandmark = "landmark";
	inline constexpr std::string_view kPlacesPlaceKindHistorical = "historical";

	inline constexpr std::string_view kPlacesPlaceKindCheck = "places_place_kind_check";
	inline constexpr std::string_view kPlacesStatusCheck = "places_status_check";
	inline constexpr std::string_view kPlacesVolatilityTierCheck = "places_volatility_tier_check";

	void MakePlacesTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
