#include "guide_links.h"

#include "db_schema/guide_links.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetForVenueProfile =
    "SELECT * FROM guide_links WHERE venue_profile_id = $1 ORDER BY sort_order ASC, id ASC";

constexpr std::string_view kSqlGetForOrganization =
    "SELECT * FROM guide_links WHERE organization_id = $1 ORDER BY sort_order ASC, id ASC";

constexpr std::string_view kSqlGetForPlace =
    "SELECT * FROM guide_links WHERE place_id = $1 ORDER BY sort_order ASC, id ASC";

constexpr std::string_view kSqlGetForSceneTag =
    "SELECT * FROM guide_links WHERE scene_tag_id = $1 ORDER BY sort_order ASC, id ASC";

}  // namespace

GuideLinks::GuideLinks(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t GuideLinks::AddLink(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kGuideLinksTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTableArray GuideLinks::GetLinksForVenueProfile(
    Transaction& transaction, int64_t venueProfileId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForVenueProfile), StringFromInt(venueProfileId));
}

KeyValueTableArray GuideLinks::GetLinksForOrganization(
    Transaction& transaction, int64_t organizationId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForOrganization), StringFromInt(organizationId));
}

KeyValueTableArray GuideLinks::GetLinksForPlace(Transaction& transaction, int64_t placeId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForPlace), StringFromInt(placeId));
}

KeyValueTableArray GuideLinks::GetLinksForSceneTag(
    Transaction& transaction, int64_t sceneTagId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForSceneTag), StringFromInt(sceneTagId));
}

void GuideLinks::DeleteLink(Transaction& transaction, int64_t id) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kGuideLinksTable,
        DbSchema::kGuideLinksId,
        StringFromInt(id));
}

}  // namespace TableHelpers
