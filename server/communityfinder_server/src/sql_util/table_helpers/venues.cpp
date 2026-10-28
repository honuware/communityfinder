#include "venues.h"

#include "bigint_array_literal.h"
#include "db_schema/venues.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetByIds =
    "SELECT * FROM venues WHERE id = ANY($1::bigint[]) ORDER BY id ASC";

constexpr std::string_view kSqlGetByNeighborhood =
    "SELECT * FROM venues WHERE neighborhood_id = $1 ORDER BY name ASC, id ASC";

}  // namespace

Venues::Venues(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Venues::AddVenue(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kVenuesTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable Venues::GetVenue(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kVenuesTable,
        DbSchema::kVenuesId,
        StringFromInt(id));
}

KeyValueTableArray Venues::GetVenuesByIds(
    Transaction& transaction, const std::vector<int64_t>& ids) const {
    if (ids.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByIds), BigintArrayLiteral(ids));
}

KeyValueTableArray Venues::GetVenuesByNeighborhood(
    Transaction& transaction, int64_t neighborhoodId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByNeighborhood), StringFromInt(neighborhoodId));
}

void Venues::UpdateVenue(Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kVenuesUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kVenuesTable,
        DbSchema::kVenuesId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

void Venues::DeleteVenue(Transaction& transaction, int64_t id) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kVenuesTable,
        DbSchema::kVenuesId,
        StringFromInt(id));
}

}  // namespace TableHelpers
