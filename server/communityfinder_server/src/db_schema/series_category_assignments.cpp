#include "series_category_assignments.h"

#include "categories.h"
#include "series.h"

namespace DbSchema {

	void MakeSeriesCategoryAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kSeriesCategoryAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kSeriesCategoryAssignmentsTable,
			kSeriesCategoryAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kSeriesTable,
			kSeriesId,
			kSeriesCategoryAssignmentsTable,
			kSeriesCategoryAssignmentsSeriesId);
		databaseInfo.AddColumnForeignKeyRef(
			kCategoriesTable,
			kCategoriesId,
			kSeriesCategoryAssignmentsTable,
			kSeriesCategoryAssignmentsCategoryId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kSeriesCategoryAssignmentsTable,
			kSeriesCategoryAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (series, category). The leading column also serves
		// the per-series lookups.
		databaseInfo.AddUniqueConstraint(
			kSeriesCategoryAssignmentsTable,
			kSeriesCategoryAssignmentsSeriesId,
			kSeriesCategoryAssignmentsCategoryId);
	}

}  // namespace DbSchema
