#include "places.h"

#include "db_schema/freshness.h"
#include "db_schema/places.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetByKind =
    "SELECT * FROM places WHERE place_kind = $1 ORDER BY name ASC, id ASC";

constexpr std::string_view kSqlGetByNeighborhood =
    "SELECT * FROM places WHERE neighborhood_id = $1 ORDER BY name ASC, id ASC";

constexpr std::string_view kSqlGetAll =
    "SELECT * FROM places ORDER BY name ASC, id ASC";

// $1, $2 = the two retired statuses (closed, moved).
constexpr std::string_view kSqlGetActive =
    "SELECT * FROM places WHERE status <> $1 AND status <> $2 "
    "ORDER BY name ASC, id ASC";

}  // namespace

Places::Places(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Places::AddPlace(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kPlacesTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable Places::GetPlace(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kPlacesTable,
        DbSchema::kPlacesId,
        StringFromInt(id));
}

KeyValueTable Places::GetPlaceBySlug(Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kPlacesTable,
        DbSchema::kPlacesSlug,
        slug);
}

KeyValueTableArray Places::GetPlacesByKind(
    Transaction& transaction, std::string_view placeKind) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByKind), std::string(placeKind));
}

KeyValueTableArray Places::GetPlacesByNeighborhood(
    Transaction& transaction, int64_t neighborhoodId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByNeighborhood), StringFromInt(neighborhoodId));
}

KeyValueTableArray Places::GetPlaces(Transaction& transaction, bool includeClosed) const {
    if (includeClosed) {
        return transaction.RunSqlStatementReturningKeyValueTableArray(std::string(kSqlGetAll));
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetActive),
        DbSchema::kFreshnessStatusClosed,
        DbSchema::kFreshnessStatusMoved);
}

void Places::UpdatePlace(Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kPlacesUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kPlacesTable,
        DbSchema::kPlacesId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
