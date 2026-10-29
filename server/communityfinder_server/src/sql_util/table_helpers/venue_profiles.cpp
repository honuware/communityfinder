#include "venue_profiles.h"

#include "db_schema/venue_profiles.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

constexpr std::string_view kSqlGetAll =
    "SELECT * FROM venue_profiles ORDER BY slug ASC, id ASC";

}  // namespace

VenueProfiles::VenueProfiles(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t VenueProfiles::AddProfile(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfilesTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable VenueProfiles::GetProfile(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfilesTable,
        DbSchema::kVenueProfilesId,
        StringFromInt(id));
}

KeyValueTable VenueProfiles::GetProfileByVenueId(
    Transaction& transaction, int64_t venueId) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfilesTable,
        DbSchema::kVenueProfilesVenueId,
        StringFromInt(venueId));
}

KeyValueTable VenueProfiles::GetProfileBySlug(
    Transaction& transaction, std::string_view slug) const {
    return DbCrud::LookupRowByValue(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfilesTable,
        DbSchema::kVenueProfilesSlug,
        slug);
}

KeyValueTableArray VenueProfiles::GetProfiles(Transaction& transaction) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(std::string(kSqlGetAll));
}

void VenueProfiles::UpdateProfile(
    Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kVenueProfilesUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kVenueProfilesTable,
        DbSchema::kVenueProfilesId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

}  // namespace TableHelpers
