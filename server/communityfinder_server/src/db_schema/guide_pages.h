#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — the CMS-lite layer the Content Pool publishes through: scene intros,
	// neighborhood rundowns, health explainers, the Freeze guide. The body is
	// markdown, stored and served verbatim; GD3 renders and sanitizes it.
	//
	// A page is public only when it is not a draft AND has a publish time that
	// has arrived (knottyyoga's blog predicate):
	//     NOT draft AND publish_at_us IS NOT NULL AND publish_at_us <= now_us()
	// This table is deliberately not on the public allowed_tables list, so a
	// draft never leaks through the generic table reads.
	//
	// Slug conventions GD3 and the Pool rely on: a scene's intro is the
	// `scenes` page whose slug is the scene tag's slug; a neighborhood's rundown
	// is the `new-to-seattle` page whose slug is the neighborhood's slug.
	inline constexpr std::string_view kGuidePagesTable = "guide_pages";
	inline constexpr std::string_view kGuidePagesId = "id";
	inline constexpr std::string_view kGuidePagesTitle = "title";
	// The page's URL identity (/guide/pages/{slug}).
	inline constexpr std::string_view kGuidePagesSlug = "slug";
	inline constexpr std::string_view kGuidePagesSection = "section";
	inline constexpr std::string_view kGuidePagesBodyMarkdown = "body_markdown";
	// Drives the 18+ gate. Always true in the adult section.
	inline constexpr std::string_view kGuidePagesAdult = "adult";
	inline constexpr std::string_view kGuidePagesDraft = "draft";
	// NULL = no publish time set, so not public even when not a draft.
	inline constexpr std::string_view kGuidePagesPublishAtUs = "publish_at_us";
	// `unverified` = the page's facts need a check.
	inline constexpr std::string_view kGuidePagesStatus = "status";
	// Set only by a person confirming the page's facts. NULL = never verified.
	inline constexpr std::string_view kGuidePagesLastVerifiedAtUs = "last_verified_at_us";
	inline constexpr std::string_view kGuidePagesVolatilityTier = "volatility_tier";
	inline constexpr std::string_view kGuidePagesCreatedUs = "created_us";
	inline constexpr std::string_view kGuidePagesUpdatedUs = "updated_us";

	// GD1 §4's guide sections, verbatim.
	inline constexpr std::string_view kGuidePagesSectionScenes = "scenes";
	inline constexpr std::string_view kGuidePagesSectionHealth = "health";
	inline constexpr std::string_view kGuidePagesSectionServices = "services";
	inline constexpr std::string_view kGuidePagesSectionNewToSeattle = "new-to-seattle";
	inline constexpr std::string_view kGuidePagesSectionFreeze = "freeze";
	inline constexpr std::string_view kGuidePagesSectionNewTo = "new-to";
	inline constexpr std::string_view kGuidePagesSectionAdult = "adult";

	inline constexpr std::string_view kGuidePagesSectionCheck = "guide_pages_section_check";
	inline constexpr std::string_view kGuidePagesAdultSectionCheck = "guide_pages_adult_section_check";
	inline constexpr std::string_view kGuidePagesStatusCheck = "guide_pages_status_check";
	inline constexpr std::string_view kGuidePagesVolatilityTierCheck = "guide_pages_volatility_tier_check";

	// Gates editing the guide: through admin_table_permissions, every guide
	// table plus `venues` (verifying venue facts is a guide curator's job, GD2
	// OQ3) in the generic CRUD editor; and previewing unpublished pages. Lives
	// with the table whose preview it gates, as manage_events lives in events.h.
	inline constexpr std::string_view kPermissionManageGuide = "manage_guide";

	void MakeGuidePagesTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
