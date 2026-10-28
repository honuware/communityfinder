#include "series_scene_tag_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/series_scene_tag_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForSeries =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM series_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.series_id = $1 "
    "ORDER BY t.sort_order ASC, t.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForSeriesIds =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM series_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.series_id = ANY($1::bigint[]) "
    "ORDER BY a.series_id ASC, t.sort_order ASC, t.id ASC";

}  // namespace

SeriesSceneTagAssignments::SeriesSceneTagAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t SeriesSceneTagAssignments::Assign(
    Transaction& transaction, int64_t seriesId, int64_t sceneTagId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesSceneTagAssignmentsTable,
        {
            { std::string(DbSchema::kSeriesSceneTagAssignmentsSeriesId), StringFromInt(seriesId) },
            { std::string(DbSchema::kSeriesSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

void SeriesSceneTagAssignments::Remove(
    Transaction& transaction, int64_t seriesId, int64_t sceneTagId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesSceneTagAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kSeriesSceneTagAssignmentsSeriesId), StringFromInt(seriesId) },
            { std::string(DbSchema::kSeriesSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

KeyValueTableArray SeriesSceneTagAssignments::GetForSeries(
    Transaction& transaction, int64_t seriesId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForSeries), StringFromInt(seriesId));
}

KeyValueTableArray SeriesSceneTagAssignments::GetForSeriesIds(
    Transaction& transaction, const std::vector<int64_t>& seriesIds) const {
    if (seriesIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForSeriesIds), BigintArrayLiteral(seriesIds));
}

}  // namespace TableHelpers
