#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — clinics, testing sites, therapists, and everyday services (the
	// queer-friendly barber, the trans-competent dentist).
	//
	// A listable table, so it carries the freshness convention (GD1 §5).
	inline constexpr std::string_view kServiceListingsTable = "service_listings";
	inline constexpr std::string_view kServiceListingsId = "id";
	inline constexpr std::string_view kServiceListingsName = "name";
	// The listing's URL identity.
	inline constexpr std::string_view kServiceListingsSlug = "slug";
	inline constexpr std::string_view kServiceListingsServiceKind = "service_kind";
	inline constexpr std::string_view kServiceListingsDescription = "description";
	inline constexpr std::string_view kServiceListingsUrl = "url";
	inline constexpr std::string_view kServiceListingsPhone = "phone";
	inline constexpr std::string_view kServiceListingsAddressText = "address_text";
	inline constexpr std::string_view kServiceListingsNeighborhoodId = "neighborhood_id";
	// The MHMDA rendering flag (GD1 §4): no person is ever attributed to a
	// health listing. When CM2's recommendations arrive, they render on these
	// rows as unattributed aggregates only.
	inline constexpr std::string_view kServiceListingsIsHealth = "is_health";
	inline constexpr std::string_view kServiceListingsStatus = "status";
	// Set only by a person confirming the facts. NULL = never verified.
	inline constexpr std::string_view kServiceListingsLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kServiceListingsVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kServiceListingsCreatedUs = "created_us";
	inline constexpr std::string_view kServiceListingsUpdatedUs = "updated_us";

	// GD1 §4's service kinds, verbatim.
	inline constexpr std::string_view kServiceListingsServiceKindStiTesting = "sti-testing";
	inline constexpr std::string_view kServiceListingsServiceKindPrep = "prep";
	inline constexpr std::string_view kServiceListingsServiceKindHivCare = "hiv-care";
	inline constexpr std::string_view kServiceListingsServiceKindPrimaryCare = "primary-care";
	inline constexpr std::string_view kServiceListingsServiceKindMentalHealth = "mental-health";
	inline constexpr std::string_view kServiceListingsServiceKindGenderAffirming = "gender-affirming";
	inline constexpr std::string_view kServiceListingsServiceKindDental = "dental";
	inline constexpr std::string_view kServiceListingsServiceKindEverydayService = "everyday-service";

	inline constexpr std::string_view kServiceListingsServiceKindCheck = "service_listings_service_kind_check";
	inline constexpr std::string_view kServiceListingsStatusCheck = "service_listings_status_check";
	inline constexpr std::string_view kServiceListingsVolatilityTierCheck = "service_listings_volatility_tier_check";

	void MakeServiceListingsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
