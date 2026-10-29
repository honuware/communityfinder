#include "guide_page_scene_tag_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/guide_page_scene_tag_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForGuidePage =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM guide_page_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.guide_page_id = $1 "
    "ORDER BY t.sort_order ASC, t.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForGuidePages =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM guide_page_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.guide_page_id = ANY($1::bigint[]) "
    "ORDER BY a.guide_page_id ASC, t.sort_order ASC, t.id ASC";

constexpr std::string_view kSqlGetGuidePageIdsForSceneTag =
    "SELECT guide_page_id FROM guide_page_scene_tag_assignments "
    "WHERE scene_tag_id = $1 ORDER BY guide_page_id ASC";

}  // namespace

GuidePageSceneTagAssignments::GuidePageSceneTagAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t GuidePageSceneTagAssignments::Assign(
    Transaction& transaction, int64_t guidePageId, int64_t sceneTagId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kGuidePageSceneTagAssignmentsTable,
        {
            { std::string(DbSchema::kGuidePageSceneTagAssignmentsGuidePageId),
              StringFromInt(guidePageId) },
            { std::string(DbSchema::kGuidePageSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

void GuidePageSceneTagAssignments::Remove(
    Transaction& transaction, int64_t guidePageId, int64_t sceneTagId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kGuidePageSceneTagAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kGuidePageSceneTagAssignmentsGuidePageId),
              StringFromInt(guidePageId) },
            { std::string(DbSchema::kGuidePageSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

KeyValueTableArray GuidePageSceneTagAssignments::GetForGuidePage(
    Transaction& transaction, int64_t guidePageId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForGuidePage), StringFromInt(guidePageId));
}

KeyValueTableArray GuidePageSceneTagAssignments::GetForGuidePages(
    Transaction& transaction, const std::vector<int64_t>& guidePageIds) const {
    if (guidePageIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForGuidePages), BigintArrayLiteral(guidePageIds));
}

std::vector<int64_t> GuidePageSceneTagAssignments::GetGuidePageIdsForSceneTag(
    Transaction& transaction, int64_t sceneTagId) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetGuidePageIdsForSceneTag), StringFromInt(sceneTagId));
    std::vector<int64_t> guidePageIds;
    guidePageIds.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        guidePageIds.push_back(std::stoll(
            row.at(std::string(DbSchema::kGuidePageSceneTagAssignmentsGuidePageId))));
    }
    return guidePageIds;
}

}  // namespace TableHelpers
