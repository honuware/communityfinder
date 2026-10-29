#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — the guide's editorial satellite on a venue (Interface Contract 2):
	// Stream A owns `venues`, and everything the guide says about a venue lives
	// here, one profile per venue. Guide pages join the two.
	//
	// No name and no status: venues.name and venues.status are authoritative.
	// The profile's own last_verified_at_us tracks the GUIDE facts (crowd, vibe,
	// price), which verify on a different cadence than the venue row (GD1 §4).
	inline constexpr std::string_view kVenueProfilesTable = "venue_profiles";
	inline constexpr std::string_view kVenueProfilesId = "id";
	// UNIQUE: a venue has at most one profile.
	inline constexpr std::string_view kVenueProfilesVenueId = "venue_id";
	// The guide's URL identity for a venue (/guide/venues/{slug}). venues has
	// no slug on purpose.
	inline constexpr std::string_view kVenueProfilesSlug = "slug";
	// The editorial heart: whose crowd it is, the vibe, when to go.
	inline constexpr std::string_view kVenueProfilesCrowdVibe = "crowd_vibe";
	// Free text, `$` to `$$$`. NULL = not stated.
	inline constexpr std::string_view kVenueProfilesPriceBand = "price_band";
	// Drives the 18+ gate.
	inline constexpr std::string_view kVenueProfilesAdult = "adult";
	// The graveyard entry's story ("Closed 2021, lease lost; the owners opened
	// X"). NULL until there is one to tell.
	inline constexpr std::string_view kVenueProfilesClosureNote = "closure_note";
	// Set only by a person confirming the guide facts. NULL = never verified.
	inline constexpr std::string_view kVenueProfilesLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kVenueProfilesVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kVenueProfilesCreatedUs = "created_us";
	inline constexpr std::string_view kVenueProfilesUpdatedUs = "updated_us";

	inline constexpr std::string_view kVenueProfilesVolatilityTierCheck = "venue_profiles_volatility_tier_check";

	void MakeVenueProfilesTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
