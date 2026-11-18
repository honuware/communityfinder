#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/series.h"

namespace Events {

// CM3 §3.3 — an event's or a series' photo gallery. Each picture is an
// event_gallery_photos row holding one photo in honuware's pipeline: the row is
// added here (POST /api/admin/event_gallery_photos), and its photo is uploaded next
// through honuware's /api/upload_photo. Edits, reordering and deletes go
// through generic CRUD, whose delete also deletes the row's photo.

// Add error codes. `field` names the API field at fault.
inline constexpr std::string_view kGalleryErrorOneParent = "ONE_PARENT_REQUIRED";
inline constexpr std::string_view kGalleryErrorParentNotFound = "PARENT_NOT_FOUND";
inline constexpr std::string_view kGalleryErrorAltTextRequired = "ALT_TEXT_REQUIRED";
inline constexpr std::string_view kGalleryErrorTooLong = "TOO_LONG";
inline constexpr std::string_view kGalleryErrorConsentRequired = "CONSENT_REQUIRED";
inline constexpr std::string_view kGalleryErrorFull = "GALLERY_FULL";

// The API's field names.
inline constexpr std::string_view kGalleryFieldEventId = "event_id";
inline constexpr std::string_view kGalleryFieldSeriesId = "series_id";
inline constexpr std::string_view kGalleryFieldAltText = "alt_text";
inline constexpr std::string_view kGalleryFieldCaption = "caption";
inline constexpr std::string_view kGalleryFieldCredit = "credit";
inline constexpr std::string_view kGalleryFieldConsentConfirmed = "consent_confirmed";

// One gallery picture as a page shows it. The photo itself is
// event_gallery_photos/<id> in honuware's pipeline.
struct GalleryPhoto {
    int64_t id = 0;
    std::string altText;
    std::string caption;
    std::string credit;
};

struct NewGalleryPhoto {
    // Exactly one is set (> 0).
    int64_t eventId = 0;
    int64_t seriesId = 0;
    std::string altText;  // required; trimmed, like caption and credit
    std::string caption;
    std::string credit;
    // "Everyone recognizable in this photo agreed to be shown" (CM3 OQ6).
    bool consentConfirmed = false;
};

struct AddGalleryPhotoResult {
    bool ok = false;
    std::string errorCode;
    std::string field;
    int64_t id = 0;
    // On GALLERY_FULL, the cap that was reached.
    int64_t maxPhotos = 0;
};

class EventGalleryHelper {
public:
    explicit EventGalleryHelper(DatabaseHelper databaseHelper);
    EventGalleryHelper(const EventGalleryHelper&) = default;
    EventGalleryHelper& operator=(const EventGalleryHelper&) = default;
    ~EventGalleryHelper() = default;

    // A parent's own gallery as the public sees it: the rows whose photo has
    // been uploaded, in display order (sort_order, then id).
    std::vector<GalleryPhoto> GetEventGallery(Transaction& transaction, int64_t eventId) const;
    std::vector<GalleryPhoto> GetSeriesGallery(Transaction& transaction, int64_t seriesId) const;

    // Adds a row at the end of its parent's gallery, recording `personId` and
    // now as the consent record. Refused when the parent's gallery already
    // holds `maxPhotos` rows (with their photos or not). Writes nothing on
    // failure.
    AddGalleryPhotoResult AddGalleryPhoto(
        Transaction& transaction, const NewGalleryPhoto& photo, int64_t personId,
        int64_t maxPhotos);

    // Deletes the row and its photo; false when there is no such row.
    bool DeleteGalleryPhoto(Transaction& transaction, int64_t id);

private:
    DatabaseHelper databaseHelper_;
    TableHelpers::EventGalleryPhotos gallery_;
    TableHelpers::Events events_;
    TableHelpers::Series series_;
};

}  // namespace Events
