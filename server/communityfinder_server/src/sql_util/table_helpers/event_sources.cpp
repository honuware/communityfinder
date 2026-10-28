#include "event_sources.h"

#include "db_schema/event_sources.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetEnabled =
    "SELECT * FROM event_sources WHERE enabled = true ORDER BY name ASC, id ASC";

}  // namespace

EventSources::EventSources(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t EventSources::AddSource(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kEventSourcesTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable EventSources::GetSource(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventSourcesTable,
        DbSchema::kEventSourcesId,
        StringFromInt(id));
}

KeyValueTableArray EventSources::GetEnabledSources(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetEnabled));
}

void EventSources::UpdateSource(
    Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kEventSourcesUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventSourcesTable,
        DbSchema::kEventSourcesId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

void EventSources::TouchLastScanned(Transaction& transaction, int64_t id, int64_t nowUs) {
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventSourcesTable,
        DbSchema::kEventSourcesId,
        StringFromInt(id),
        { { std::string(DbSchema::kEventSourcesLastScannedAtUs), StringFromInt(nowUs) } });
}

}  // namespace TableHelpers
