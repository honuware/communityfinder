#include "venues.h"

#include <string>

#include "freshness.h"
#include "neighborhoods.h"

namespace DbSchema {

	void MakeVenuesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kVenuesTable);
		databaseInfo.AddColumnPrimaryKey(
			kVenuesTable,
			kVenuesId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kVenuesTable,
			kVenuesName,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesAddress,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesCity,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesState,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesZip,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesUrl,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNullable(
			kVenuesTable,
			kVenuesLatitude,
			DB_TYPE_DOUBLE);
		databaseInfo.AddColumnNullable(
			kVenuesTable,
			kVenuesLongitude,
			DB_TYPE_DOUBLE);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kNeighborhoodsTable,
			kNeighborhoodsId,
			kVenuesTable,
			kVenuesNeighborhoodId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesStatus,
			DB_TYPE_STRING,
			"'open'");
		databaseInfo.AddCheckConstraint(
			kVenuesTable,
			kVenuesStatusCheck,
			std::string(kVenuesStatus) + " IN (" +
				std::string(kFreshnessStatusSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kVenuesTable,
			kVenuesLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5's default tier for a venue: open/closed and ownership drift
		// monthly.
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesVolatilityTier,
			DB_TYPE_STRING,
			"'monthly'");
		databaseInfo.AddCheckConstraint(
			kVenuesTable,
			kVenuesVolatilityTierCheck,
			std::string(kVenuesVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kVenuesTable,
			kVenuesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

	void CreateVenuesIndexes(Transaction& transaction) {
		(void)transaction;
	}

}  // namespace DbSchema
