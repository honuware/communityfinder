#include "scene_tags.h"

#include "db_schema/scene_tags.h"
#include "sql_util/database_access/database_crud_helpers.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetAllEnabled =
    "SELECT * FROM scene_tags WHERE enabled = true "
    "ORDER BY sort_order ASC, id ASC";

}  // namespace

SceneTags::SceneTags(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t SceneTags::AddSceneTag(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kSceneTagsTable,
        values);
}

KeyValueTableArray SceneTags::GetAll(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetAllEnabled));
}

KeyValueTable SceneTags::GetBySlug(Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kSceneTagsTable,
        DbSchema::kSceneTagsSlug,
        slug);
}

}  // namespace TableHelpers
