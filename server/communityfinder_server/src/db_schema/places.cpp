#include "places.h"

#include <string>

#include "freshness.h"
#include "neighborhoods.h"

namespace DbSchema {

	void MakePlacesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kPlacesTable);
		databaseInfo.AddColumnPrimaryKey(
			kPlacesTable,
			kPlacesId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kPlacesTable,
			kPlacesName,
			DB_TYPE_STRING);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kPlacesTable,
			kPlacesSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kPlacesTable, kPlacesSlug);
		// No default: every place is exactly one kind.
		databaseInfo.AddColumnSimple(
			kPlacesTable,
			kPlacesPlaceKind,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kPlacesTable,
			kPlacesPlaceKindCheck,
			std::string(kPlacesPlaceKind) + " IN ('" +
				std::string(kPlacesPlaceKindParkBeach) + "', '" +
				std::string(kPlacesPlaceKindLandmark) + "', '" +
				std::string(kPlacesPlaceKindHistorical) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesAddressText,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnForeignKeyRefNullable(
			kNeighborhoodsTable,
			kNeighborhoodsId,
			kPlacesTable,
			kPlacesNeighborhoodId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesAdult,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultFalse);
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesStatus,
			DB_TYPE_STRING,
			"'open'");
		databaseInfo.AddCheckConstraint(
			kPlacesTable,
			kPlacesStatusCheck,
			std::string(kPlacesStatus) + " IN (" +
				std::string(kFreshnessStatusSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kPlacesTable,
			kPlacesLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5's default tier for a place. A place mid-news-cycle (Denny Blaine
		// while its litigation is live) is set to monthly as a data value.
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesVolatilityTier,
			DB_TYPE_STRING,
			"'quarterly'");
		databaseInfo.AddCheckConstraint(
			kPlacesTable,
			kPlacesVolatilityTierCheck,
			std::string(kPlacesVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kPlacesTable,
			kPlacesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
