#include "series_category_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/series_category_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForSeries =
    "SELECT a.*, c.slug AS category_slug "
    "FROM series_category_assignments a "
    "JOIN categories c ON c.id = a.category_id "
    "WHERE a.series_id = $1 "
    "ORDER BY c.sort_order ASC, c.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForSeriesIds =
    "SELECT a.*, c.slug AS category_slug "
    "FROM series_category_assignments a "
    "JOIN categories c ON c.id = a.category_id "
    "WHERE a.series_id = ANY($1::bigint[]) "
    "ORDER BY a.series_id ASC, c.sort_order ASC, c.id ASC";

}  // namespace

SeriesCategoryAssignments::SeriesCategoryAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t SeriesCategoryAssignments::Assign(
    Transaction& transaction, int64_t seriesId, int64_t categoryId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesCategoryAssignmentsTable,
        {
            { std::string(DbSchema::kSeriesCategoryAssignmentsSeriesId), StringFromInt(seriesId) },
            { std::string(DbSchema::kSeriesCategoryAssignmentsCategoryId),
              StringFromInt(categoryId) }
        });
}

void SeriesCategoryAssignments::Remove(
    Transaction& transaction, int64_t seriesId, int64_t categoryId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesCategoryAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kSeriesCategoryAssignmentsSeriesId), StringFromInt(seriesId) },
            { std::string(DbSchema::kSeriesCategoryAssignmentsCategoryId),
              StringFromInt(categoryId) }
        });
}

KeyValueTableArray SeriesCategoryAssignments::GetForSeries(
    Transaction& transaction, int64_t seriesId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForSeries), StringFromInt(seriesId));
}

KeyValueTableArray SeriesCategoryAssignments::GetForSeriesIds(
    Transaction& transaction, const std::vector<int64_t>& seriesIds) const {
    if (seriesIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForSeriesIds), BigintArrayLiteral(seriesIds));
}

}  // namespace TableHelpers
