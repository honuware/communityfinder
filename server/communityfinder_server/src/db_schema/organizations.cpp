#include "organizations.h"

#include <string>

#include "freshness.h"
#include "neighborhoods.h"

namespace DbSchema {

	void MakeOrganizationsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kOrganizationsTable);
		databaseInfo.AddColumnPrimaryKey(
			kOrganizationsTable,
			kOrganizationsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kOrganizationsTable,
			kOrganizationsName,
			DB_TYPE_STRING);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kOrganizationsTable,
			kOrganizationsSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kOrganizationsTable, kOrganizationsSlug);
		// No default: every organization is exactly one kind.
		databaseInfo.AddColumnSimple(
			kOrganizationsTable,
			kOrganizationsOrgKind,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kOrganizationsTable,
			kOrganizationsOrgKindCheck,
			std::string(kOrganizationsOrgKind) + " IN ('" +
				std::string(kOrganizationsOrgKindSportsLeague) + "', '" +
				std::string(kOrganizationsOrgKindChorusPerformance) + "', '" +
				std::string(kOrganizationsOrgKindClubHobby) + "', '" +
				std::string(kOrganizationsOrgKindRecovery) + "', '" +
				std::string(kOrganizationsOrgKindFaith) + "', '" +
				std::string(kOrganizationsOrgKindProfessional) + "', '" +
				std::string(kOrganizationsOrgKindCommunitySupport) + "', '" +
				std::string(kOrganizationsOrgKindSocialMeetup) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsUrl,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsMeetingCadenceText,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnForeignKeyRefNullable(
			kNeighborhoodsTable,
			kNeighborhoodsId,
			kOrganizationsTable,
			kOrganizationsNeighborhoodId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsStatus,
			DB_TYPE_STRING,
			"'open'");
		databaseInfo.AddCheckConstraint(
			kOrganizationsTable,
			kOrganizationsStatusCheck,
			std::string(kOrganizationsStatus) + " IN (" +
				std::string(kFreshnessStatusSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kOrganizationsTable,
			kOrganizationsClosureNote,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNullable(
			kOrganizationsTable,
			kOrganizationsLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5's default tier for an organization: leagues and groups change
		// slowly.
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsVolatilityTier,
			DB_TYPE_STRING,
			"'annual'");
		databaseInfo.AddCheckConstraint(
			kOrganizationsTable,
			kOrganizationsVolatilityTierCheck,
			std::string(kOrganizationsVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kOrganizationsTable,
			kOrganizationsUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
