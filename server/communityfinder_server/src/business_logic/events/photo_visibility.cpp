#include "business_logic/events/photo_visibility.h"

#include <string>

#include "business_logic/events/event_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "sql_util/table_helpers/events.h"

namespace Events {

namespace {

bool IsEventPublic(const DatabaseHelper& databaseHelper, Transaction& transaction, int64_t eventId) {
    KeyValueTable event = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
    return !event.empty() && IsPublicEventStatus(event.at(std::string(DbSchema::kEventsStatus)));
}

}  // namespace

bool IsPhotoRowPublic(const DatabaseHelper& databaseHelper, Transaction& transaction,
                      std::string_view tableName, int64_t tableItemId) {
    if (tableName == DbSchema::kEventsTable) {
        return IsEventPublic(databaseHelper, transaction, tableItemId);
    }
    if (tableName == DbSchema::kEventGalleryPhotosTable) {
        KeyValueTable row =
            TableHelpers::EventGalleryPhotos(databaseHelper).GetGalleryPhoto(transaction, tableItemId);
        if (row.empty()) {
            return false;
        }
        // NULL reads back as "".
        const std::string eventId = row.at(std::string(DbSchema::kEventGalleryPhotosEventId));
        return eventId.empty() || IsEventPublic(databaseHelper, transaction, std::stoll(eventId));
    }
    return true;
}

}  // namespace Events
