#include "event_submissions.h"

#include <string>

#include "db_schema/event_submissions.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

// Served by event_submissions_person_created_idx.
constexpr std::string_view kSqlGetForPerson =
    "SELECT * FROM event_submissions WHERE submitted_by_person_id = $1 "
    "ORDER BY created_us DESC, id DESC";

// Served by event_submissions_person_created_idx.
constexpr std::string_view kSqlCountSince =
    "SELECT count(*) FROM event_submissions "
    "WHERE submitted_by_person_id = $1 AND created_us >= $2";

constexpr std::string_view kSqlGetForUpdate =
    "SELECT * FROM event_submissions WHERE id = $1 FOR UPDATE";

// The IS NULL guard is what makes the stamp a claim: RETURNING yields a row only
// when this statement changed it.
constexpr std::string_view kSqlMarkDecisionNotified =
    "UPDATE event_submissions SET decision_notified_us = $2, updated_us = now_us() "
    "WHERE id = $1 AND decision_notified_us IS NULL RETURNING id";

}  // namespace

EventSubmissions::EventSubmissions(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t EventSubmissions::AddSubmission(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kEventSubmissionsTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable EventSubmissions::GetSubmission(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventSubmissionsTable,
        DbSchema::kEventSubmissionsId,
        StringFromInt(id));
}

KeyValueTable EventSubmissions::GetSubmissionForUpdate(Transaction& transaction, int64_t id) {
    return transaction.RunSqlStatementReturningOneRow(
        std::string(kSqlGetForUpdate), StringFromInt(id));
}

KeyValueTableArray EventSubmissions::GetSubmissionsForPerson(
    Transaction& transaction, int64_t personId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForPerson), StringFromInt(personId));
}

int64_t EventSubmissions::CountSubmissionsSince(
    Transaction& transaction, int64_t personId, int64_t sinceUs) const {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        std::string(kSqlCountSince), StringFromInt(personId), StringFromInt(sinceUs)));
}

void EventSubmissions::UpdateSubmission(
    Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kEventSubmissionsUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventSubmissionsTable,
        DbSchema::kEventSubmissionsId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

bool EventSubmissions::MarkDecisionNotified(
    Transaction& transaction, int64_t id, int64_t nowUs) {
    return !transaction.RunSqlStatementReturningOneRow(
        std::string(kSqlMarkDecisionNotified), StringFromInt(id), StringFromInt(nowUs))
        .empty();
}

}  // namespace TableHelpers
