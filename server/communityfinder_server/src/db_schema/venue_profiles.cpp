#include "venue_profiles.h"

#include <string>

#include "freshness.h"
#include "venues.h"

namespace DbSchema {

	void MakeVenueProfilesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kVenueProfilesTable);
		databaseInfo.AddColumnPrimaryKey(
			kVenueProfilesTable,
			kVenueProfilesId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kVenuesTable,
			kVenuesId,
			kVenueProfilesTable,
			kVenueProfilesVenueId);
		// The 1:1 satellite: one profile per venue.
		databaseInfo.AddUniqueConstraint(kVenueProfilesTable, kVenueProfilesVenueId);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kVenueProfilesTable,
			kVenueProfilesSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kVenueProfilesTable, kVenueProfilesSlug);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenueProfilesTable,
			kVenueProfilesCrowdVibe,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNullable(
			kVenueProfilesTable,
			kVenueProfilesPriceBand,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenueProfilesTable,
			kVenueProfilesAdult,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultFalse);
		databaseInfo.AddColumnNullable(
			kVenueProfilesTable,
			kVenueProfilesClosureNote,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNullable(
			kVenueProfilesTable,
			kVenueProfilesLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5's default tier for a venue profile: crowd and vibe are durable.
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenueProfilesTable,
			kVenueProfilesVolatilityTier,
			DB_TYPE_STRING,
			"'quarterly'");
		databaseInfo.AddCheckConstraint(
			kVenueProfilesTable,
			kVenueProfilesVolatilityTierCheck,
			std::string(kVenueProfilesVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenueProfilesTable,
			kVenueProfilesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenueProfilesTable,
			kVenueProfilesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
