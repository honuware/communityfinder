#include "venue_profile_scene_tag_assignments.h"

#include "bigint_array_literal.h"
#include "db_schema/venue_profile_scene_tag_assignments.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForVenueProfile =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM venue_profile_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.venue_profile_id = $1 "
    "ORDER BY t.sort_order ASC, t.id ASC";

// $1 is a Postgres array literal, "{1,2,3}".
constexpr std::string_view kSqlGetForVenueProfiles =
    "SELECT a.*, t.slug AS scene_tag_slug "
    "FROM venue_profile_scene_tag_assignments a "
    "JOIN scene_tags t ON t.id = a.scene_tag_id "
    "WHERE a.venue_profile_id = ANY($1::bigint[]) "
    "ORDER BY a.venue_profile_id ASC, t.sort_order ASC, t.id ASC";

constexpr std::string_view kSqlGetVenueProfileIdsForSceneTag =
    "SELECT venue_profile_id FROM venue_profile_scene_tag_assignments "
    "WHERE scene_tag_id = $1 ORDER BY venue_profile_id ASC";

}  // namespace

VenueProfileSceneTagAssignments::VenueProfileSceneTagAssignments(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t VenueProfileSceneTagAssignments::Assign(
    Transaction& transaction, int64_t venueProfileId, int64_t sceneTagId) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfileSceneTagAssignmentsTable,
        {
            { std::string(DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId),
              StringFromInt(venueProfileId) },
            { std::string(DbSchema::kVenueProfileSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

void VenueProfileSceneTagAssignments::Remove(
    Transaction& transaction, int64_t venueProfileId, int64_t sceneTagId) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfileSceneTagAssignmentsTable,
        KeyValueTable{
            { std::string(DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId),
              StringFromInt(venueProfileId) },
            { std::string(DbSchema::kVenueProfileSceneTagAssignmentsSceneTagId),
              StringFromInt(sceneTagId) }
        });
}

KeyValueTableArray VenueProfileSceneTagAssignments::GetForVenueProfile(
    Transaction& transaction, int64_t venueProfileId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForVenueProfile), StringFromInt(venueProfileId));
}

KeyValueTableArray VenueProfileSceneTagAssignments::GetForVenueProfiles(
    Transaction& transaction, const std::vector<int64_t>& venueProfileIds) const {
    if (venueProfileIds.empty()) {
        return {};
    }
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForVenueProfiles), BigintArrayLiteral(venueProfileIds));
}

std::vector<int64_t> VenueProfileSceneTagAssignments::GetVenueProfileIdsForSceneTag(
    Transaction& transaction, int64_t sceneTagId) const {
    KeyValueTableArray rows = transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetVenueProfileIdsForSceneTag), StringFromInt(sceneTagId));
    std::vector<int64_t> venueProfileIds;
    venueProfileIds.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        venueProfileIds.push_back(std::stoll(
            row.at(std::string(DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId))));
    }
    return venueProfileIds;
}

}  // namespace TableHelpers
