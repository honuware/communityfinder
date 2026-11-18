#include "series.h"

#include "bigint_array_literal.h"
#include "db_schema/freshness.h"
#include "db_schema/series.h"
#include "db_schema/venues.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetByIds =
    "SELECT * FROM series WHERE id = ANY($1::bigint[]) ORDER BY id ASC";

// $1, $2 = the two retired statuses (closed, moved).
constexpr std::string_view kSqlGetActive =
    "SELECT * FROM series WHERE status <> $1 AND status <> $2 "
    "ORDER BY title ASC, id ASC";

// Events::GetPhotoOwners' query with two candidates: $2 the series, $3 its venue.
constexpr std::string_view kSqlGetPhotoOwners =
    "SELECT s.id, owner.photo_table, owner.photo_id FROM series s "
    "LEFT JOIN LATERAL ("
    "SELECT candidate.photo_table, candidate.photo_id FROM (VALUES "
    "(1, $2::text, s.id), (2, $3::text, s.venue_id)"
    ") AS candidate(rank, photo_table, photo_id) "
    "WHERE candidate.photo_id IS NOT NULL AND EXISTS ("
    "SELECT 1 FROM table_item_photos p "
    "WHERE p.table_name = candidate.photo_table AND p.table_item_id = candidate.photo_id) "
    "ORDER BY candidate.rank LIMIT 1"
    ") owner ON TRUE "
    "WHERE s.id = ANY($1::bigint[]) ORDER BY s.id ASC";

}  // namespace

Series::Series(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Series::AddSeries(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable Series::GetSeries(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesTable,
        DbSchema::kSeriesId,
        StringFromInt(id));
}

KeyValueTable Series::GetSeriesBySlug(Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesTable,
        DbSchema::kSeriesSlug,
        slug);
}

KeyValueTableArray Series::GetSeriesByIds(
    Transaction& transaction, const std::vector<int64_t>& ids) const {
    if (ids.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByIds), BigintArrayLiteral(ids));
}

KeyValueTableArray Series::GetActiveSeries(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetActive),
        DbSchema::kFreshnessStatusClosed,
        DbSchema::kFreshnessStatusMoved);
}

KeyValueTableArray Series::GetPhotoOwners(
    Transaction& transaction, const std::vector<int64_t>& seriesIds) const {
    if (seriesIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetPhotoOwners), BigintArrayLiteral(seriesIds),
        DbSchema::kSeriesTable, DbSchema::kVenuesTable);
}

void Series::UpdateSeries(Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kSeriesUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kSeriesTable,
        DbSchema::kSeriesId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
