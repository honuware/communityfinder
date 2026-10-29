#include "organizations.h"

#include "db_schema/freshness.h"
#include "db_schema/organizations.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetByKind =
    "SELECT * FROM organizations WHERE org_kind = $1 ORDER BY name ASC, id ASC";

constexpr std::string_view kSqlGetByNeighborhood =
    "SELECT * FROM organizations WHERE neighborhood_id = $1 ORDER BY name ASC, id ASC";

constexpr std::string_view kSqlGetAll =
    "SELECT * FROM organizations ORDER BY name ASC, id ASC";

// $1, $2 = the two retired statuses (closed, moved).
constexpr std::string_view kSqlGetActive =
    "SELECT * FROM organizations WHERE status <> $1 AND status <> $2 "
    "ORDER BY name ASC, id ASC";

}  // namespace

Organizations::Organizations(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Organizations::AddOrganization(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kOrganizationsTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable Organizations::GetOrganization(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kOrganizationsTable,
        DbSchema::kOrganizationsId,
        StringFromInt(id));
}

KeyValueTable Organizations::GetOrganizationBySlug(
    Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kOrganizationsTable,
        DbSchema::kOrganizationsSlug,
        slug);
}

KeyValueTableArray Organizations::GetOrganizationsByKind(
    Transaction& transaction, std::string_view orgKind) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByKind), std::string(orgKind));
}

KeyValueTableArray Organizations::GetOrganizationsByNeighborhood(
    Transaction& transaction, int64_t neighborhoodId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByNeighborhood), StringFromInt(neighborhoodId));
}

KeyValueTableArray Organizations::GetOrganizations(
    Transaction& transaction, bool includeClosed) const {
    if (includeClosed) {
        return transaction.RunSqlStatementReturningKeyValueTableArray(std::string(kSqlGetAll));
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetActive),
        DbSchema::kFreshnessStatusClosed,
        DbSchema::kFreshnessStatusMoved);
}

void Organizations::UpdateOrganization(
    Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kOrganizationsUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kOrganizationsTable,
        DbSchema::kOrganizationsId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
