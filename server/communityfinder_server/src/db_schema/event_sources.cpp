#include "event_sources.h"

#include <string>

#include "freshness.h"
#include "venues.h"

namespace DbSchema {

	void MakeEventSourcesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kEventSourcesTable);
		databaseInfo.AddColumnPrimaryKey(
			kEventSourcesTable,
			kEventSourcesId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kEventSourcesTable,
			kEventSourcesName,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesUrl,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnSimple(
			kEventSourcesTable,
			kEventSourcesKind,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kEventSourcesTable,
			kEventSourcesKindCheck,
			std::string(kEventSourcesKind) + " IN ('" +
				std::string(kEventSourcesKindIcsFeed) + "', '" +
				std::string(kEventSourcesKindVenueSite) + "', '" +
				std::string(kEventSourcesKindTicketingOrg) + "', '" +
				std::string(kEventSourcesKindAggregator) + "', '" +
				std::string(kEventSourcesKindEditorialMonitor) + "', '" +
				std::string(kEventSourcesKindSubmission) + "', '" +
				std::string(kEventSourcesKindSocial) + "')");
		databaseInfo.AddColumnForeignKeyRefNullable(
			kVenuesTable,
			kVenuesId,
			kEventSourcesTable,
			kEventSourcesVenueId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesEnabled,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultTrue);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesScanHints,
			DB_TYPE_STRING,
			"''");
		// Weekly by default: a source is a lineup, and GD1 §5 puts recurring-event
		// lineups in the weekly tier.
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesVolatilityTier,
			DB_TYPE_STRING,
			"'weekly'");
		databaseInfo.AddCheckConstraint(
			kEventSourcesTable,
			kEventSourcesVolatilityTierCheck,
			std::string(kEventSourcesVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kEventSourcesTable,
			kEventSourcesLastScannedAtUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesNotes,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSourcesTable,
			kEventSourcesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
