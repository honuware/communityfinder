#include "neighborhoods.h"

#include "db_schema/neighborhoods.h"
#include "sql_util/database_access/database_crud_helpers.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetAllEnabled =
    "SELECT * FROM neighborhoods WHERE enabled = true "
    "ORDER BY sort_order ASC, id ASC";

}  // namespace

Neighborhoods::Neighborhoods(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Neighborhoods::AddNeighborhood(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kNeighborhoodsTable,
        values);
}

KeyValueTableArray Neighborhoods::GetAll(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetAllEnabled));
}

KeyValueTable Neighborhoods::GetBySlug(Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kNeighborhoodsTable,
        DbSchema::kNeighborhoodsSlug,
        slug);
}

}  // namespace TableHelpers
