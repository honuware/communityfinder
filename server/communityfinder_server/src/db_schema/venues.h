#pragma once

#include "sql_util/database_access/transaction.h"
#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 1 — physical venues events happen at. Stream A's table
	// (Interface Contract 2): the guide's editorial data about a venue lives in
	// GD2's satellite `venue_profiles`, FK'd to venues.id, so this table stays
	// guide-free — no crowd/vibe, no adult flag, and no slug (the guide's URL
	// identity lives on the profile).
	//
	// Carries the freshness convention (GD1 §5): status + last_verified_at_us +
	// volatility_tier. Closed venues are closed, never deleted — FKs cascade, so
	// deleting a venue would delete its events.
	inline constexpr std::string_view kVenuesTable = "venues";
	inline constexpr std::string_view kVenuesId = "id";
	inline constexpr std::string_view kVenuesName = "name";
	inline constexpr std::string_view kVenuesDescription = "description";
	inline constexpr std::string_view kVenuesAddress = "address";
	inline constexpr std::string_view kVenuesCity = "city";
	inline constexpr std::string_view kVenuesState = "state";
	inline constexpr std::string_view kVenuesZip = "zip";
	inline constexpr std::string_view kVenuesUrl = "url";
	// Decimal degrees. Written but not yet read — there is no map view at MVP.
	inline constexpr std::string_view kVenuesLatitude = "latitude";
	inline constexpr std::string_view kVenuesLongitude = "longitude";
	// NULL = location varies / not yet placed.
	inline constexpr std::string_view kVenuesNeighborhoodId = "neighborhood_id";
	inline constexpr std::string_view kVenuesStatus = "status";
	// Set only by a person confirming the venue's facts — routine edits leave
	// it alone. NULL = never verified.
	inline constexpr std::string_view kVenuesLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kVenuesVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kVenuesCreatedUs = "created_us";
	inline constexpr std::string_view kVenuesUpdatedUs = "updated_us";

	inline constexpr std::string_view kVenuesStatusCheck = "venues_status_check";
	inline constexpr std::string_view kVenuesVolatilityTierCheck = "venues_volatility_tier_check";

	void MakeVenuesTable(DatabaseInfo databaseInfo);

	// No index yet: the house rule is one index per named query, added with the
	// query it serves, and no venue query exists until the table helpers land.
	// Called from CreateAppTables right after the venues table is created.
	void CreateVenuesIndexes(Transaction& transaction);

}  // namespace DbSchema
