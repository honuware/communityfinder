#include "guide_links.h"

#include <string>

#include "organizations.h"
#include "places.h"
#include "scene_tags.h"
#include "venue_profiles.h"

namespace DbSchema {

	void MakeGuideLinksTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kGuideLinksTable);
		databaseInfo.AddColumnPrimaryKey(
			kGuideLinksTable,
			kGuideLinksId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kGuideLinksTable,
			kGuideLinksLabel,
			DB_TYPE_STRING);
		databaseInfo.AddColumnSimple(
			kGuideLinksTable,
			kGuideLinksUrl,
			DB_TYPE_STRING);
		// No default: GD1 §4 labels every link with its platform.
		databaseInfo.AddColumnSimple(
			kGuideLinksTable,
			kGuideLinksPlatform,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kGuideLinksTable,
			kGuideLinksPlatformCheck,
			std::string(kGuideLinksPlatform) + " IN ('" +
				std::string(kGuideLinksPlatformDiscord) + "', '" +
				std::string(kGuideLinksPlatformBluesky) + "', '" +
				std::string(kGuideLinksPlatformInstagram) + "', '" +
				std::string(kGuideLinksPlatformFacebookGroup) + "', '" +
				std::string(kGuideLinksPlatformMeetup) + "', '" +
				std::string(kGuideLinksPlatformWebsite) + "', '" +
				std::string(kGuideLinksPlatformOther) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuideLinksTable,
			kGuideLinksSortOrder,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultZero);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kVenueProfilesTable,
			kVenueProfilesId,
			kGuideLinksTable,
			kGuideLinksVenueProfileId);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kOrganizationsTable,
			kOrganizationsId,
			kGuideLinksTable,
			kGuideLinksOrganizationId);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kPlacesTable,
			kPlacesId,
			kGuideLinksTable,
			kGuideLinksPlaceId);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kSceneTagsTable,
			kSceneTagsId,
			kGuideLinksTable,
			kGuideLinksSceneTagId);
		// Exactly one parent: a link with none would hang off nothing, and one
		// with two would show on both pages.
		databaseInfo.AddCheckConstraint(
			kGuideLinksTable,
			kGuideLinksExactlyOneParentCheck,
			"num_nonnulls(" + std::string(kGuideLinksVenueProfileId) + ", " +
				std::string(kGuideLinksOrganizationId) + ", " +
				std::string(kGuideLinksPlaceId) + ", " +
				std::string(kGuideLinksSceneTagId) + ") = 1");
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuideLinksTable,
			kGuideLinksCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuideLinksTable,
			kGuideLinksUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
