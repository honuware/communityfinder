#include "event_category_assignments.h"

#include "categories.h"
#include "events.h"

namespace DbSchema {

	void MakeEventCategoryAssignmentsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kEventCategoryAssignmentsTable);
		databaseInfo.AddColumnPrimaryKey(
			kEventCategoryAssignmentsTable,
			kEventCategoryAssignmentsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRef(
			kEventsTable,
			kEventsId,
			kEventCategoryAssignmentsTable,
			kEventCategoryAssignmentsEventId);
		databaseInfo.AddColumnForeignKeyRef(
			kCategoriesTable,
			kCategoriesId,
			kEventCategoryAssignmentsTable,
			kEventCategoryAssignmentsCategoryId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventCategoryAssignmentsTable,
			kEventCategoryAssignmentsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		// One assignment per (event, category). The leading column also serves
		// the per-event lookups.
		databaseInfo.AddUniqueConstraint(
			kEventCategoryAssignmentsTable,
			kEventCategoryAssignmentsEventId,
			kEventCategoryAssignmentsCategoryId);
	}

}  // namespace DbSchema
