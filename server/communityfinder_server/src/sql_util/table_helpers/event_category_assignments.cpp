#include "event_category_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/event_category_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForEvent =
    "SELECT a.*, c.slug AS category_slug "
    "FROM event_category_assignments a "
    "JOIN categories c ON c.id = a.category_id "
    "WHERE a.event_id = $1 "
    "ORDER BY c.sort_order ASC, c.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForEvents =
    "SELECT a.*, c.slug AS category_slug "
    "FROM event_category_assignments a "
    "JOIN categories c ON c.id = a.category_id "
    "WHERE a.event_id = ANY($1::bigint[]) "
    "ORDER BY a.event_id ASC, c.sort_order ASC, c.id ASC";

constexpr std::string_view kSqlGetEventIdsForCategory =
    "SELECT event_id FROM event_category_assignments "
    "WHERE category_id = $1 ORDER BY event_id ASC";

}  // namespace

EventCategoryAssignments::EventCategoryAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t EventCategoryAssignments::Assign(
    Transaction& transaction, int64_t eventId, int64_t categoryId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kEventCategoryAssignmentsTable,
        {
            { std::string(DbSchema::kEventCategoryAssignmentsEventId), StringFromInt(eventId) },
            { std::string(DbSchema::kEventCategoryAssignmentsCategoryId),
              StringFromInt(categoryId) }
        });
}

void EventCategoryAssignments::Remove(
    Transaction& transaction, int64_t eventId, int64_t categoryId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventCategoryAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kEventCategoryAssignmentsEventId), StringFromInt(eventId) },
            { std::string(DbSchema::kEventCategoryAssignmentsCategoryId),
              StringFromInt(categoryId) }
        });
}

KeyValueTableArray EventCategoryAssignments::GetForEvent(
    Transaction& transaction, int64_t eventId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForEvent), StringFromInt(eventId));
}

KeyValueTableArray EventCategoryAssignments::GetForEvents(
    Transaction& transaction, const std::vector<int64_t>& eventIds) const {
    if (eventIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForEvents), BigintArrayLiteral(eventIds));
}

std::vector<int64_t> EventCategoryAssignments::GetEventIdsForCategory(
    Transaction& transaction, int64_t categoryId) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetEventIdsForCategory), StringFromInt(categoryId));
    std::vector<int64_t> eventIds;
    eventIds.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        eventIds.push_back(
            std::stoll(row.at(std::string(DbSchema::kEventCategoryAssignmentsEventId))));
    }
    return eventIds;
}

}  // namespace TableHelpers
