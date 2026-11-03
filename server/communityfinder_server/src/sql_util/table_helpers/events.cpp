#include "events.h"

#include <string>

#include "bigint_array_literal.h"
#include "db_schema/events.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

// Served by the events_source_id_external_key_key unique index.
constexpr std::string_view kSqlGetBySourceAndExternalKey =
    "SELECT * FROM events WHERE source_id = $1 AND external_key = $2";

// Served by events_status_starts_at_idx.
constexpr std::string_view kSqlGetInRange =
    "SELECT * FROM events "
    "WHERE status = $1 AND starts_at_us >= $2 AND starts_at_us < $3 "
    "ORDER BY starts_at_us ASC, id ASC";

// Served by events_series_idx.
constexpr std::string_view kSqlGetBySeries =
    "SELECT * FROM events WHERE series_id = $1 ORDER BY starts_at_us ASC, id ASC";

constexpr std::string_view kSqlGetByStatus =
    "SELECT * FROM events WHERE status = $1 ORDER BY starts_at_us ASC, id ASC";

// Served by events_submission_idx.
constexpr std::string_view kSqlGetBySubmission =
    "SELECT * FROM events WHERE submission_id = $1 ORDER BY starts_at_us ASC, id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetBySubmissions =
    "SELECT * FROM events WHERE submission_id = ANY($1::bigint[]) "
    "ORDER BY submission_id ASC, starts_at_us ASC, id ASC";

constexpr std::string_view kSqlCountPendingSubmittedBy =
    "SELECT count(*) FROM events e "
    "JOIN event_submissions s ON s.id = e.submission_id "
    "WHERE s.submitted_by_person_id = $1 AND e.status = $2";

// $1 is a Postgres text array literal, "{"pending","approved"}". Served by
// events_status_starts_at_idx.
constexpr std::string_view kSqlGetStartingBetween =
    "SELECT * FROM events "
    "WHERE status = ANY($1::text[]) AND starts_at_us >= $2 AND starts_at_us < $3 "
    "ORDER BY starts_at_us ASC, id ASC";

// "{"a","b"}": each element double-quoted, with \ and " escaped, so the one
// bound parameter is a valid text[] whatever the strings hold.
std::string TextArrayLiteral(const std::vector<std::string_view>& values) {
    std::string literal = "{";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            literal += ',';
        }
        literal += '"';
        for (char c : values[i]) {
            if (c == '"' || c == '\\') {
                literal += '\\';
            }
            literal += c;
        }
        literal += '"';
    }
    literal += '}';
    return literal;
}

}  // namespace

Events::Events(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Events::AddEvent(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kEventsTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable Events::GetEvent(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventsTable,
        DbSchema::kEventsId,
        StringFromInt(id));
}

KeyValueTable Events::GetEventBySourceAndExternalKey(
    Transaction& transaction, int64_t sourceId, std::string_view externalKey) const {
    return transaction.RunSqlStatementReturningOneRow(
        std::string(kSqlGetBySourceAndExternalKey), StringFromInt(sourceId), externalKey);
}

KeyValueTableArray Events::GetEventsInRange(
    Transaction& transaction, int64_t fromUs, int64_t toUs, std::string_view status) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetInRange), status, StringFromInt(fromUs), StringFromInt(toUs));
}

KeyValueTableArray Events::GetEventsBySeries(Transaction& transaction, int64_t seriesId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetBySeries), StringFromInt(seriesId));
}

KeyValueTableArray Events::GetPendingEvents(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetByStatus), DbSchema::kEventsStatusPending);
}

KeyValueTableArray Events::GetEventsBySubmission(
    Transaction& transaction, int64_t submissionId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetBySubmission), StringFromInt(submissionId));
}

KeyValueTableArray Events::GetEventsBySubmissions(
    Transaction& transaction, const std::vector<int64_t>& submissionIds) const {
    if (submissionIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetBySubmissions), BigintArrayLiteral(submissionIds));
}

int64_t Events::CountPendingSubmittedBy(Transaction& transaction, int64_t personId) const {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        std::string(kSqlCountPendingSubmittedBy), StringFromInt(personId),
        DbSchema::kEventsStatusPending));
}

KeyValueTableArray Events::GetEventsStartingBetween(
    Transaction& transaction, int64_t fromUs, int64_t toUs,
    const std::vector<std::string_view>& statuses) const {
    if (statuses.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetStartingBetween), TextArrayLiteral(statuses),
        StringFromInt(fromUs), StringFromInt(toUs));
}

void Events::UpdateEvent(Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kEventsUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventsTable,
        DbSchema::kEventsId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
