#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — where events come from: an ICS feed, a venue's own site, a
	// ticketing org, ... One row per place the scanner (EV5) or a person checks.
	// Ingested events point back here, and (source_id, external_key) is what
	// makes re-ingesting the same item an update rather than a duplicate.
	inline constexpr std::string_view kEventSourcesTable = "event_sources";
	inline constexpr std::string_view kEventSourcesId = "id";
	inline constexpr std::string_view kEventSourcesName = "name";
	inline constexpr std::string_view kEventSourcesUrl = "url";
	inline constexpr std::string_view kEventSourcesKind = "kind";
	// The venue this source covers, when it covers exactly one. NULL for
	// multi-venue sources (aggregators, ticketing orgs, editorial monitors).
	inline constexpr std::string_view kEventSourcesVenueId = "venue_id";
	inline constexpr std::string_view kEventSourcesEnabled = "enabled";
	// Free-form notes for the scanner (EV5): selectors, quirks, what to skip.
	inline constexpr std::string_view kEventSourcesScanHints = "scan_hints";
	// How often EV5 re-scans this source (GD1 §5's tiers).
	inline constexpr std::string_view kEventSourcesVolatilityTier = "volatility_tier";
	// NULL = never scanned.
	inline constexpr std::string_view kEventSourcesLastScannedAtUs = "last_scanned_at_us";
	inline constexpr std::string_view kEventSourcesNotes = "notes";
	inline constexpr std::string_view kEventSourcesCreatedUs = "created_us";
	inline constexpr std::string_view kEventSourcesUpdatedUs = "updated_us";

	inline constexpr std::string_view kEventSourcesKindIcsFeed = "ics_feed";
	inline constexpr std::string_view kEventSourcesKindVenueSite = "venue_site";
	inline constexpr std::string_view kEventSourcesKindTicketingOrg = "ticketing_org";
	inline constexpr std::string_view kEventSourcesKindAggregator = "aggregator";
	inline constexpr std::string_view kEventSourcesKindEditorialMonitor = "editorial_monitor";
	inline constexpr std::string_view kEventSourcesKindSubmission = "submission";
	inline constexpr std::string_view kEventSourcesKindSocial = "social";

	inline constexpr std::string_view kEventSourcesKindCheck = "event_sources_kind_check";
	inline constexpr std::string_view kEventSourcesVolatilityTierCheck = "event_sources_volatility_tier_check";

	void MakeEventSourcesTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
