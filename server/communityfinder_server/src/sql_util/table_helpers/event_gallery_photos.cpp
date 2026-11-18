#include "event_gallery_photos.h"

#include "db_schema/event_gallery_photos.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "util/types.h"

namespace TableHelpers {

namespace {

// Served by event_gallery_photos_event_idx and _series_idx. $2 is the table's
// own name, for the has-photo check against honuware's table_item_photos.
constexpr std::string_view kSqlGetForEvent =
    "SELECT g.*, EXISTS (SELECT 1 FROM table_item_photos p "
    "WHERE p.table_name = $2 AND p.table_item_id = g.id) AS has_photo "
    "FROM event_gallery_photos g WHERE g.event_id = $1 ORDER BY g.sort_order ASC, g.id ASC";

constexpr std::string_view kSqlGetForSeries =
    "SELECT g.*, EXISTS (SELECT 1 FROM table_item_photos p "
    "WHERE p.table_name = $2 AND p.table_item_id = g.id) AS has_photo "
    "FROM event_gallery_photos g WHERE g.series_id = $1 ORDER BY g.sort_order ASC, g.id ASC";

}  // namespace

EventGalleryPhotos::EventGalleryPhotos(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper) {
}

int64_t EventGalleryPhotos::AddGalleryPhoto(Transaction& transaction, const KeyValueTable& values) {
    return DbCrud::AddRowToTableFetchInt64PrimaryKey(
        transaction,
        databaseHelper_,
        DbSchema::kEventGalleryPhotosTable,
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

KeyValueTable EventGalleryPhotos::GetGalleryPhoto(Transaction& transaction, int64_t id) const {
    return DbCrud::GetRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventGalleryPhotosTable,
        DbSchema::kEventGalleryPhotosId,
        StringFromInt(id));
}

KeyValueTableArray EventGalleryPhotos::GetGalleryForEvent(
    Transaction& transaction, int64_t eventId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForEvent), StringFromInt(eventId),
        std::string(DbSchema::kEventGalleryPhotosTable));
}

KeyValueTableArray EventGalleryPhotos::GetGalleryForSeries(
    Transaction& transaction, int64_t seriesId) const {
    return transaction.RunSqlStatementReturningKeyValueTableArray(
        std::string(kSqlGetForSeries), StringFromInt(seriesId),
        std::string(DbSchema::kEventGalleryPhotosTable));
}

void EventGalleryPhotos::UpdateGalleryPhoto(
    Transaction& transaction, int64_t id, const KeyValueTable& updates) {
    KeyValueTable values = updates;
    values[std::string(DbSchema::kEventGalleryPhotosUpdatedUs)] =
        std::string(DbSchema::kDatabaseInfoDefaultNow);
    DbCrud::UpdateRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventGalleryPhotosTable,
        DbSchema::kEventGalleryPhotosId,
        StringFromInt(id),
        values,
        { std::string(DbSchema::kDatabaseInfoDefaultNow), "NULL" });
}

void EventGalleryPhotos::DeleteGalleryPhoto(Transaction& transaction, int64_t id) {
    DbCrud::DeleteRow(
        transaction,
        databaseHelper_,
        DbSchema::kEventGalleryPhotosTable,
        DbSchema::kEventGalleryPhotosId,
        StringFromInt(id));
}

}  // namespace TableHelpers
