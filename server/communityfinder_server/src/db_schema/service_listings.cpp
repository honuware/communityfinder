#include "service_listings.h"

#include <string>

#include "freshness.h"
#include "neighborhoods.h"

namespace DbSchema {

	void MakeServiceListingsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kServiceListingsTable);
		databaseInfo.AddColumnPrimaryKey(
			kServiceListingsTable,
			kServiceListingsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kServiceListingsTable,
			kServiceListingsName,
			DB_TYPE_STRING);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kServiceListingsTable,
			kServiceListingsSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kServiceListingsTable, kServiceListingsSlug);
		// No default: every listing is exactly one kind.
		databaseInfo.AddColumnSimple(
			kServiceListingsTable,
			kServiceListingsServiceKind,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kServiceListingsTable,
			kServiceListingsServiceKindCheck,
			std::string(kServiceListingsServiceKind) + " IN ('" +
				std::string(kServiceListingsServiceKindStiTesting) + "', '" +
				std::string(kServiceListingsServiceKindPrep) + "', '" +
				std::string(kServiceListingsServiceKindHivCare) + "', '" +
				std::string(kServiceListingsServiceKindPrimaryCare) + "', '" +
				std::string(kServiceListingsServiceKindMentalHealth) + "', '" +
				std::string(kServiceListingsServiceKindGenderAffirming) + "', '" +
				std::string(kServiceListingsServiceKindDental) + "', '" +
				std::string(kServiceListingsServiceKindEverydayService) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsUrl,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsPhone,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsAddressText,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnForeignKeyRefNullable(
			kNeighborhoodsTable,
			kNeighborhoodsId,
			kServiceListingsTable,
			kServiceListingsNeighborhoodId);
		// TRUE unless a person says otherwise: seven of the eight kinds are
		// health care, and wrongly treating a health listing as everyday is the
		// mistake that would attribute a person to it.
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsIsHealth,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultTrue);
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsStatus,
			DB_TYPE_STRING,
			"'open'");
		databaseInfo.AddCheckConstraint(
			kServiceListingsTable,
			kServiceListingsStatusCheck,
			std::string(kServiceListingsStatus) + " IN (" +
				std::string(kFreshnessStatusSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kServiceListingsTable,
			kServiceListingsLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5's tier for health programs and clinic service menus. GD1 puts
		// everyday services at annual, but a column default can't read is_health,
		// so the listing helper sets annual when it creates an everyday service.
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsVolatilityTier,
			DB_TYPE_STRING,
			"'semiannual'");
		databaseInfo.AddCheckConstraint(
			kServiceListingsTable,
			kServiceListingsVolatilityTierCheck,
			std::string(kServiceListingsVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kServiceListingsTable,
			kServiceListingsUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
