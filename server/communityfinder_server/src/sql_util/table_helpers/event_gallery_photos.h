#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// On every GetGalleryFor* row: "t" when the row's picture has been uploaded,
// "f" while it hasn't (the row is created first, then its photo).
inline constexpr std::string_view kGalleryPhotoHasPhoto = "has_photo";

// CM3 §2.1 — CRUD for event_gallery_photos, an event's or a series' gallery.
// Each row has exactly one parent; the database rejects none or both, blank
// alt text, and a row without its consent record. The row's picture is its
// one photo in honuware's pipeline (ImageHelper), which this helper never
// touches: deleting a row's photo is the caller's job.
class EventGalleryPhotos {
public:
    explicit EventGalleryPhotos(DatabaseHelper databaseHelper);
    EventGalleryPhotos(const EventGalleryPhotos&) = default;
    EventGalleryPhotos& operator=(const EventGalleryPhotos&) = default;
    ~EventGalleryPhotos() = default;

    // CREATE — `values` sets exactly one of event_id and series_id, the alt
    // text, and both consent columns. Throws when the database refuses it.
    int64_t AddGalleryPhoto(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such row.
    KeyValueTable GetGalleryPhoto(Transaction& transaction, int64_t id) const;

    // READ — one parent's gallery in display order: sort_order, then id. Each
    // row also has kGalleryPhotoHasPhoto. A gallery is at most the cap's rows,
    // so callers count and find the last sort_order from this read.
    KeyValueTableArray GetGalleryForEvent(Transaction& transaction, int64_t eventId) const;
    KeyValueTableArray GetGalleryForSeries(Transaction& transaction, int64_t seriesId) const;

    // UPDATE — bumps updated_us.
    void UpdateGalleryPhoto(Transaction& transaction, int64_t id, const KeyValueTable& updates);

    // DELETE — nothing happens when there is no such row.
    void DeleteGalleryPhoto(Transaction& transaction, int64_t id);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
