#include "series.h"

#include <string>

#include "freshness.h"
#include "venues.h"

namespace DbSchema {

	void MakeSeriesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kSeriesTable);
		databaseInfo.AddColumnPrimaryKey(
			kSeriesTable,
			kSeriesId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kSeriesTable,
			kSeriesTitle,
			DB_TYPE_STRING);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kSeriesTable,
			kSeriesSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kSeriesTable, kSeriesSlug);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesCadenceText,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesUrl,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnForeignKeyRefNullable(
			kVenuesTable,
			kVenuesId,
			kSeriesTable,
			kSeriesVenueId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesStatus,
			DB_TYPE_STRING,
			"'open'");
		databaseInfo.AddCheckConstraint(
			kSeriesTable,
			kSeriesStatusCheck,
			std::string(kSeriesStatus) + " IN (" +
				std::string(kFreshnessStatusSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kSeriesTable,
			kSeriesLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5's default tier for a series: the pipeline keeps each week's
		// lineup current, while the series itself (still running? still 2nd
		// Saturdays?) drifts more slowly.
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesVolatilityTier,
			DB_TYPE_STRING,
			"'monthly'");
		databaseInfo.AddCheckConstraint(
			kSeriesTable,
			kSeriesVolatilityTierCheck,
			std::string(kSeriesVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesTable,
			kSeriesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
