#pragma once

#include <string_view>

namespace DbSchema {

	// The freshness convention (GD1 §5, Interface Contract 3). Every listable
	// table — venues and series here, the guide's tables in GD2 — carries a
	// `status`, a `last_verified_at_us`, and a `volatility_tier` constrained to
	// these values. Shared so both streams' CHECK constraints spell the enums
	// identically.
	//
	// `status`: closed things stay listed forever (the closure graveyard);
	// `unverified` = listed from research but not yet checked by a person.
	inline constexpr std::string_view kFreshnessStatusSqlValues =
		"'open', 'closed', 'moved', 'seasonal', 'unverified'";

	inline constexpr std::string_view kFreshnessStatusOpen = "open";
	inline constexpr std::string_view kFreshnessStatusClosed = "closed";
	inline constexpr std::string_view kFreshnessStatusMoved = "moved";
	inline constexpr std::string_view kFreshnessStatusSeasonal = "seasonal";
	inline constexpr std::string_view kFreshnessStatusUnverified = "unverified";

	// `volatility_tier`: how often a listing needs re-verifying (EV5's cadence).
	// Not rendered publicly.
	inline constexpr std::string_view kVolatilityTierSqlValues =
		"'weekly', 'monthly', 'quarterly', 'semiannual', 'annual'";

}  // namespace DbSchema
