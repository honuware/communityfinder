#include "categories.h"

namespace DbSchema {

	void MakeCategoriesTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kCategoriesTable);
		databaseInfo.AddColumnPrimaryKey(
			kCategoriesTable,
			kCategoriesId,
			DB_TYPE_BIGSERIAL);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kCategoriesTable,
			kCategoriesSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kCategoriesTable, kCategoriesSlug);
		databaseInfo.AddColumnSimple(
			kCategoriesTable,
			kCategoriesDisplayName,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNotNullableWithDefault(
			kCategoriesTable,
			kCategoriesDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kCategoriesTable,
			kCategoriesSortOrder,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultZero);
		databaseInfo.AddColumnNotNullableWithDefault(
			kCategoriesTable,
			kCategoriesEnabled,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultTrue);
		databaseInfo.AddColumnNotNullableWithDefault(
			kCategoriesTable,
			kCategoriesCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kCategoriesTable,
			kCategoriesUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
