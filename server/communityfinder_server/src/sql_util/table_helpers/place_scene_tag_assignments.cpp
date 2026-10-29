#include "place_scene_tag_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/place_scene_tag_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForPlace =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM place_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.place_id = $1 "
    "ORDER BY t.sort_order ASC, t.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForPlaces =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM place_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.place_id = ANY($1::bigint[]) "
    "ORDER BY a.place_id ASC, t.sort_order ASC, t.id ASC";

constexpr std::string_view kSqlGetPlaceIdsForSceneTag =
    "SELECT place_id FROM place_scene_tag_assignments "
    "WHERE scene_tag_id = $1 ORDER BY place_id ASC";

}  // namespace

PlaceSceneTagAssignments::PlaceSceneTagAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t PlaceSceneTagAssignments::Assign(
    Transaction& transaction, int64_t placeId, int64_t sceneTagId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kPlaceSceneTagAssignmentsTable,
        {
            { std::string(DbSchema::kPlaceSceneTagAssignmentsPlaceId), StringFromInt(placeId) },
            { std::string(DbSchema::kPlaceSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

void PlaceSceneTagAssignments::Remove(
    Transaction& transaction, int64_t placeId, int64_t sceneTagId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kPlaceSceneTagAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kPlaceSceneTagAssignmentsPlaceId), StringFromInt(placeId) },
            { std::string(DbSchema::kPlaceSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

KeyValueTableArray PlaceSceneTagAssignments::GetForPlace(
    Transaction& transaction, int64_t placeId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForPlace), StringFromInt(placeId));
}

KeyValueTableArray PlaceSceneTagAssignments::GetForPlaces(
    Transaction& transaction, const std::vector<int64_t>& placeIds) const {
    if (placeIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForPlaces), BigintArrayLiteral(placeIds));
}

std::vector<int64_t> PlaceSceneTagAssignments::GetPlaceIdsForSceneTag(
    Transaction& transaction, int64_t sceneTagId) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetPlaceIdsForSceneTag), StringFromInt(sceneTagId));
    std::vector<int64_t> placeIds;
    placeIds.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        placeIds.push_back(
            std::stoll(row.at(std::string(DbSchema::kPlaceSceneTagAssignmentsPlaceId))));
    }
    return placeIds;
}

}  // namespace TableHelpers
