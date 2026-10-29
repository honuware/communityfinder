#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — "where this scene talks" (GD1 §4): outbound links to the Discord,
	// the Instagram, the Meetup group a scene or group actually uses. Always
	// outbound, always labeled with the platform, never an embedded feed.
	//
	// One table for all four kinds of parent (GD2 OQ2). Each link has EXACTLY
	// ONE parent among the four nullable FKs, which a CHECK enforces, so every
	// parent keeps a real foreign key and the curators get one editor.
	inline constexpr std::string_view kGuideLinksTable = "guide_links";
	inline constexpr std::string_view kGuideLinksId = "id";
	// The link's text, e.g. "Seattle Bears Discord".
	inline constexpr std::string_view kGuideLinksLabel = "label";
	inline constexpr std::string_view kGuideLinksUrl = "url";
	inline constexpr std::string_view kGuideLinksPlatform = "platform";
	// Order among one parent's links; lower lists first.
	inline constexpr std::string_view kGuideLinksSortOrder = "sort_order";
	inline constexpr std::string_view kGuideLinksVenueProfileId = "venue_profile_id";
	inline constexpr std::string_view kGuideLinksOrganizationId = "organization_id";
	inline constexpr std::string_view kGuideLinksPlaceId = "place_id";
	inline constexpr std::string_view kGuideLinksSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kGuideLinksCreatedUs = "created_us";
	inline constexpr std::string_view kGuideLinksUpdatedUs = "updated_us";

	// GD1 §4's platforms, verbatim.
	inline constexpr std::string_view kGuideLinksPlatformDiscord = "discord";
	inline constexpr std::string_view kGuideLinksPlatformBluesky = "bluesky";
	inline constexpr std::string_view kGuideLinksPlatformInstagram = "instagram";
	inline constexpr std::string_view kGuideLinksPlatformFacebookGroup = "facebook-group";
	inline constexpr std::string_view kGuideLinksPlatformMeetup = "meetup";
	inline constexpr std::string_view kGuideLinksPlatformWebsite = "website";
	inline constexpr std::string_view kGuideLinksPlatformOther = "other";

	inline constexpr std::string_view kGuideLinksPlatformCheck = "guide_links_platform_check";
	inline constexpr std::string_view kGuideLinksExactlyOneParentCheck = "guide_links_exactly_one_parent";

	void MakeGuideLinksTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
