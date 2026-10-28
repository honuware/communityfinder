#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — a recurring event's durable identity ("T4T, 2nd Saturdays"):
	// a label and a page, not a generator. Each occurrence is its own `events`
	// row pointing here. Series carry their own category and scene-tag
	// assignments, which instances inherit unless they have their own (EV1 OQ5).
	//
	// A listable table, so it carries the freshness convention (GD1 §5). End a
	// series by closing it, never by deleting it — FKs cascade, so deleting a
	// series deletes every instance.
	inline constexpr std::string_view kSeriesTable = "series";
	inline constexpr std::string_view kSeriesId = "id";
	inline constexpr std::string_view kSeriesTitle = "title";
	// The series page's URL identity (/events/series/{slug}).
	inline constexpr std::string_view kSeriesSlug = "slug";
	inline constexpr std::string_view kSeriesDescription = "description";
	// How it recurs, in words ("2nd Saturdays"). Display only — nothing
	// generates occurrences from it.
	inline constexpr std::string_view kSeriesCadenceText = "cadence_text";
	inline constexpr std::string_view kSeriesUrl = "url";
	// NULL = the series moves around.
	inline constexpr std::string_view kSeriesVenueId = "venue_id";
	inline constexpr std::string_view kSeriesStatus = "status";
	// Set only by a person confirming the series is still running as
	// described. NULL = never verified.
	inline constexpr std::string_view kSeriesLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kSeriesVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kSeriesCreatedUs = "created_us";
	inline constexpr std::string_view kSeriesUpdatedUs = "updated_us";

	inline constexpr std::string_view kSeriesStatusCheck = "series_status_check";
	inline constexpr std::string_view kSeriesVolatilityTierCheck = "series_volatility_tier_check";

	void MakeSeriesTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
