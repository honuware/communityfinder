#pragma once

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// GD2 — a venue profile's scene tags: whose crowd the venue draws. They
	// hang off the profile, not the venue, because scene tagging is guide data
	// (Interface Contract 2).
	inline constexpr std::string_view kVenueProfileSceneTagAssignmentsTable = "venue_profile_scene_tag_assignments";
	inline constexpr std::string_view kVenueProfileSceneTagAssignmentsId = "id";
	inline constexpr std::string_view kVenueProfileSceneTagAssignmentsVenueProfileId = "venue_profile_id";
	inline constexpr std::string_view kVenueProfileSceneTagAssignmentsSceneTagId = "scene_tag_id";
	inline constexpr std::string_view kVenueProfileSceneTagAssignmentsCreatedUs = "created_us";

	void MakeVenueProfileSceneTagAssignmentsTable(DatabaseInfo databaseInfo);

}  // namespace DbSchema
