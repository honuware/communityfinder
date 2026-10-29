#include "organization_scene_tag_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/organization_scene_tag_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForOrganization =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM organization_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.organization_id = $1 "
    "ORDER BY t.sort_order ASC, t.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForOrganizations =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM organization_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.organization_id = ANY($1::bigint[]) "
    "ORDER BY a.organization_id ASC, t.sort_order ASC, t.id ASC";

constexpr std::string_view kSqlGetOrganizationIdsForSceneTag =
    "SELECT organization_id FROM organization_scene_tag_assignments "
    "WHERE scene_tag_id = $1 ORDER BY organization_id ASC";

}  // namespace

OrganizationSceneTagAssignments::OrganizationSceneTagAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t OrganizationSceneTagAssignments::Assign(
    Transaction& transaction, int64_t organizationId, int64_t sceneTagId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kOrganizationSceneTagAssignmentsTable,
        {
            { std::string(DbSchema::kOrganizationSceneTagAssignmentsOrganizationId),
              StringFromInt(organizationId) },
            { std::string(DbSchema::kOrganizationSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

void OrganizationSceneTagAssignments::Remove(
    Transaction& transaction, int64_t organizationId, int64_t sceneTagId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kOrganizationSceneTagAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kOrganizationSceneTagAssignmentsOrganizationId),
              StringFromInt(organizationId) },
            { std::string(DbSchema::kOrganizationSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

KeyValueTableArray OrganizationSceneTagAssignments::GetForOrganization(
    Transaction& transaction, int64_t organizationId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForOrganization), StringFromInt(organizationId));
}

KeyValueTableArray OrganizationSceneTagAssignments::GetForOrganizations(
    Transaction& transaction, const std::vector<int64_t>& organizationIds) const {
    if (organizationIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForOrganizations), BigintArrayLiteral(organizationIds));
}

std::vector<int64_t> OrganizationSceneTagAssignments::GetOrganizationIdsForSceneTag(
    Transaction& transaction, int64_t sceneTagId) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetOrganizationIdsForSceneTag), StringFromInt(sceneTagId));
    std::vector<int64_t> organizationIds;
    organizationIds.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        organizationIds.push_back(std::stoll(
            row.at(std::string(DbSchema::kOrganizationSceneTagAssignmentsOrganizationId))));
    }
    return organizationIds;
}

}  // namespace TableHelpers
