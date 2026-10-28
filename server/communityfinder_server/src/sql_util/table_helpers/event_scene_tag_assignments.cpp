#include "event_scene_tag_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/event_scene_tag_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForEvent =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM event_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.event_id = $1 "
    "ORDER BY t.sort_order ASC, t.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForEvents =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM event_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.event_id = ANY($1::bigint[]) "
    "ORDER BY a.event_id ASC, t.sort_order ASC, t.id ASC";

constexpr std::string_view kSqlGetEventIdsForSceneTag =
    "SELECT event_id FROM event_scene_tag_assignments "
    "WHERE scene_tag_id = $1 ORDER BY event_id ASC";

}  // namespace

EventSceneTagAssignments::EventSceneTagAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t EventSceneTagAssignments::Assign(
    Transaction& transaction, int64_t eventId, int64_t sceneTagId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kEventSceneTagAssignmentsTable,
        {
            { std::string(DbSchema::kEventSceneTagAssignmentsEventId), StringFromInt(eventId) },
            { std::string(DbSchema::kEventSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

void EventSceneTagAssignments::Remove(
    Transaction& transaction, int64_t eventId, int64_t sceneTagId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventSceneTagAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kEventSceneTagAssignmentsEventId), StringFromInt(eventId) },
            { std::string(DbSchema::kEventSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

KeyValueTableArray EventSceneTagAssignments::GetForEvent(
    Transaction& transaction, int64_t eventId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForEvent), StringFromInt(eventId));
}

KeyValueTableArray EventSceneTagAssignments::GetForEvents(
    Transaction& transaction, const std::vector<int64_t>& eventIds) const {
    if (eventIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForEvents), BigintArrayLiteral(eventIds));
}

std::vector<int64_t> EventSceneTagAssignments::GetEventIdsForSceneTag(
    Transaction& transaction, int64_t sceneTagId) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetEventIdsForSceneTag), StringFromInt(sceneTagId));
    std::vector<int64_t> eventIds;
    eventIds.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        eventIds.push_back(
            std::stoll(row.at(std::string(DbSchema::kEventSceneTagAssignmentsEventId))));
    }
    return eventIds;
}

}  // namespace TableHelpers
