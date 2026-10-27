#include "scene_tags.h"

#include <string>

namespace DbSchema {

	void MakeSceneTagsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kSceneTagsTable);
		databaseInfo.AddColumnPrimaryKey(
			kSceneTagsTable,
			kSceneTagsId,
			DB_TYPE_BIGSERIAL);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kSceneTagsTable,
			kSceneTagsSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kSceneTagsTable, kSceneTagsSlug);
		databaseInfo.AddColumnSimple(
			kSceneTagsTable,
			kSceneTagsDisplayName,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSceneTagsTable,
			kSceneTagsDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kSceneTagsTable,
			kSceneTagsCoverage,
			DB_TYPE_STRING,
			"'" + std::string(kSceneTagsCoverageFull) + "'");
		databaseInfo.AddCheckConstraint(
			kSceneTagsTable,
			kSceneTagsCoverageCheck,
			std::string(kSceneTagsCoverage) + " IN ('" +
				std::string(kSceneTagsCoverageFull) + "', '" +
				std::string(kSceneTagsCoverageLinkOut) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kSceneTagsTable,
			kSceneTagsSortOrder,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultZero);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSceneTagsTable,
			kSceneTagsEnabled,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultTrue);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSceneTagsTable,
			kSceneTagsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSceneTagsTable,
			kSceneTagsUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
