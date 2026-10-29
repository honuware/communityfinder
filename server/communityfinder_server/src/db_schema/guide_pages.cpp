#include "guide_pages.h"

#include <string>

#include "freshness.h"

namespace DbSchema {

	void MakeGuidePagesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kGuidePagesTable);
		databaseInfo.AddColumnPrimaryKey(
			kGuidePagesTable,
			kGuidePagesId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kGuidePagesTable,
			kGuidePagesTitle,
			DB_TYPE_STRING);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL. Unique across sections, so a slug alone names
		// a page.
		databaseInfo.AddColumnSimple(
			kGuidePagesTable,
			kGuidePagesSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kGuidePagesTable, kGuidePagesSlug);
		// No default: every page belongs to exactly one section.
		databaseInfo.AddColumnSimple(
			kGuidePagesTable,
			kGuidePagesSection,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kGuidePagesTable,
			kGuidePagesSectionCheck,
			std::string(kGuidePagesSection) + " IN ('" +
				std::string(kGuidePagesSectionScenes) + "', '" +
				std::string(kGuidePagesSectionHealth) + "', '" +
				std::string(kGuidePagesSectionServices) + "', '" +
				std::string(kGuidePagesSectionNewToSeattle) + "', '" +
				std::string(kGuidePagesSectionFreeze) + "', '" +
				std::string(kGuidePagesSectionNewTo) + "', '" +
				std::string(kGuidePagesSectionAdult) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesBodyMarkdown,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesAdult,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultFalse);
		// Everything in the 18+ section sits behind the 18+ gate: a page filed
		// there without the flag would render ungated.
		databaseInfo.AddCheckConstraint(
			kGuidePagesTable,
			kGuidePagesAdultSectionCheck,
			std::string(kGuidePagesSection) + " <> '" +
				std::string(kGuidePagesSectionAdult) + "' OR " +
				std::string(kGuidePagesAdult));
		// A new page starts as a draft: nothing is public until someone
		// publishes it on purpose.
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesDraft,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultTrue);
		databaseInfo.AddColumnNullable(
			kGuidePagesTable,
			kGuidePagesPublishAtUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesStatus,
			DB_TYPE_STRING,
			"'open'");
		databaseInfo.AddCheckConstraint(
			kGuidePagesTable,
			kGuidePagesStatusCheck,
			std::string(kGuidePagesStatus) + " IN (" +
				std::string(kFreshnessStatusSqlValues) + ")");
		databaseInfo.AddColumnNullable(
			kGuidePagesTable,
			kGuidePagesLastVerifiedAtUs,
			DB_TYPE_BIGINT);
		// GD1 §5 defaults a page's tier by its section: adult → monthly, health
		// → semiannual, everything else → annual. A column default can't read
		// another column, so the column defaults to the common case and the page
		// helper sets the other two when it creates a page.
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesVolatilityTier,
			DB_TYPE_STRING,
			"'annual'");
		databaseInfo.AddCheckConstraint(
			kGuidePagesTable,
			kGuidePagesVolatilityTierCheck,
			std::string(kGuidePagesVolatilityTier) + " IN (" +
				std::string(kVolatilityTierSqlValues) + ")");
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kGuidePagesTable,
			kGuidePagesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
