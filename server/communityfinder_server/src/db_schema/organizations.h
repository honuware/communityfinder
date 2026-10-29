#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — leagues, choruses, clubs, recovery groups, congregations, ...: the
	// groups people join, as distinct from the venues they meet at.
	//
	// A listable table, so it carries the freshness convention (GD1 §5). Close
	// an organization rather than deleting it: closed ones stay listed in the
	// graveyard with their closure note, and FKs cascade.
	inline constexpr std::string_view kOrganizationsTable = "organizations";
	inline constexpr std::string_view kOrganizationsId = "id";
	inline constexpr std::string_view kOrganizationsName = "name";
	// The organization page's URL identity (/guide/orgs/{slug}).
	inline constexpr std::string_view kOrganizationsSlug = "slug";
	inline constexpr std::string_view kOrganizationsOrgKind = "org_kind";
	inline constexpr std::string_view kOrganizationsDescription = "description";
	inline constexpr std::string_view kOrganizationsUrl = "url";
	// When and where it meets, in words ("Mondays 6:30 PM, Cal Anderson").
	// Display only.
	inline constexpr std::string_view kOrganizationsMeetingCadenceText = "meeting_cadence_text";
	// One neighborhood or none: NULL = countywide, or it moves around (GD2 OQ7).
	inline constexpr std::string_view kOrganizationsNeighborhoodId = "neighborhood_id";
	// Drives the 18+ gate (GD1 §8.4's sex-positive orgs; GD2 OQ11 (b)).
	inline constexpr std::string_view kOrganizationsAdult = "adult";
	inline constexpr std::string_view kOrganizationsStatus = "status";
	// The graveyard entry's story. NULL until there is one to tell.
	inline constexpr std::string_view kOrganizationsClosureNote = "closure_note";
	// Set only by a person confirming the facts. NULL = never verified.
	inline constexpr std::string_view kOrganizationsLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kOrganizationsVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kOrganizationsCreatedUs = "created_us";
	inline constexpr std::string_view kOrganizationsUpdatedUs = "updated_us";

	// GD1 §4's organization kinds, verbatim.
	inline constexpr std::string_view kOrganizationsOrgKindSportsLeague = "sports-league";
	inline constexpr std::string_view kOrganizationsOrgKindChorusPerformance = "chorus-performance";
	inline constexpr std::string_view kOrganizationsOrgKindClubHobby = "club-hobby";
	inline constexpr std::string_view kOrganizationsOrgKindRecovery = "recovery";
	inline constexpr std::string_view kOrganizationsOrgKindFaith = "faith";
	inline constexpr std::string_view kOrganizationsOrgKindProfessional = "professional";
	inline constexpr std::string_view kOrganizationsOrgKindCommunitySupport = "community-support";
	inline constexpr std::string_view kOrganizationsOrgKindSocialMeetup = "social-meetup";

	inline constexpr std::string_view kOrganizationsOrgKindCheck = "organizations_org_kind_check";
	inline constexpr std::string_view kOrganizationsStatusCheck = "organizations_status_check";
	inline constexpr std::string_view kOrganizationsVolatilityTierCheck = "organizations_volatility_tier_check";

	void MakeOrganizationsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
