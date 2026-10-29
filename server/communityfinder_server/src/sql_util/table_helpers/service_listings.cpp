#include "service_listings.h"

#include "db_schema/freshness.h"
#include "db_schema/service_listings.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetByKind =
    "SELECT * FROM service_listings WHERE service_kind = $1 ORDER BY name ASC, id ASC";

constexpr std::string_view kSqlGetByNeighborhood =
    "SELECT * FROM service_listings WHERE neighborhood_id = $1 ORDER BY name ASC, id ASC";

constexpr std::string_view kSqlGetAll =
    "SELECT * FROM service_listings ORDER BY name ASC, id ASC";

// $1, $2 = the two retired statuses (closed, moved).
constexpr std::string_view kSqlGetActive =
    "SELECT * FROM service_listings WHERE status <> $1 AND status <> $2 "
    "ORDER BY name ASC, id ASC";

}  // namespace

ServiceListings::ServiceListings(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t ServiceListings::AddServiceListing(
    Transaction& transaction, const KeyValueTable& values) {
    KeyValueTable row = values;
    const std::string isHealthColumn(DbSchema::kServiceListingsIsHealth);
    const std::string tierColumn(DbSchema::kServiceListingsVolatilityTier);
    if (!row.contains(tierColumn) && row.contains(isHealthColumn) &&
        !StringToBool(row.at(isHealthColumn))) {
        row[tierColumn] = std::string(DbSchema::kVolatilityTierAnnual);
    }
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kServiceListingsTable,
        row,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable ServiceListings::GetServiceListing(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kServiceListingsTable,
        DbSchema::kServiceListingsId,
        StringFromInt(id));
}

KeyValueTable ServiceListings::GetServiceListingBySlug(
    Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kServiceListingsTable,
        DbSchema::kServiceListingsSlug,
        slug);
}

KeyValueTableArray ServiceListings::GetServiceListingsByKind(
    Transaction& transaction, std::string_view serviceKind) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByKind), std::string(serviceKind));
}

KeyValueTableArray ServiceListings::GetServiceListingsByNeighborhood(
    Transaction& transaction, int64_t neighborhoodId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByNeighborhood), StringFromInt(neighborhoodId));
}

KeyValueTableArray ServiceListings::GetServiceListings(
    Transaction& transaction, bool includeClosed) const {
    if (includeClosed) {
        return transaction.RunSqlStatementReturningKeyValueTableArray(std::string(kSqlGetAll));
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetActive),
        DbSchema::kFreshnessStatusClosed,
        DbSchema::kFreshnessStatusMoved);
}

void ServiceListings::UpdateServiceListing(
    Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kServiceListingsUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kServiceListingsTable,
        DbSchema::kServiceListingsId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
