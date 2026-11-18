#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"

namespace Events {

// CM3 §0.1 (H15) — the per-row rule behind honuware's PublicPhotoTables
// (register_app_services.cpp): may a signed-out visitor read this row's
// scaled photo? It's asked only for the tables the app lists as public.
//   - events: once the event's status is public (approved or archived), so a
//     submitter's flyer and a curator's upload stay private while the event
//     is pending or rejected. The past-events window doesn't hide a photo.
//   - event_gallery_photos: a series' row always; an event's row as its event.
//   - every other listed table (series, venues, …): always, as before H15.
// A row that doesn't exist isn't public. That also covers the photos left
// behind when a parent's delete cascades to its gallery rows.
// A photo the rule refuses still serves to a signed-in reader of the table
// (a curator's preview), privately.
bool IsPhotoRowPublic(const DatabaseHelper& databaseHelper, Transaction& transaction,
                      std::string_view tableName, int64_t tableItemId);

}  // namespace Events
