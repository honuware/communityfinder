#include "categories.h"

#include "db_schema/categories.h"
#include "sql_util/database_access/database_crud_helpers.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetAllEnabled =
    "SELECT * FROM categories WHERE enabled = true "
    "ORDER BY sort_order ASC, id ASC";

}  // namespace

Categories::Categories(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t Categories::AddCategory(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kCategoriesTable,
        values);
}

KeyValueTableArray Categories::GetAll(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetAllEnabled));
}

KeyValueTable Categories::GetBySlug(Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kCategoriesTable,
        DbSchema::kCategoriesSlug,
        slug);
}

}  // namespace TableHelpers
