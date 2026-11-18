#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "sql_util/database_access/transaction.h"
#include "util/secrets/secrets_helper.h"

namespace Events {

// CM3 OQ5 — the gallery cap: the most photos one event's or series' gallery
// may hold. Mason: "let's make this configurable via the admin portal." Like
// the past-events settings, it lives in config_secrets (key in
// business_logic/app_secret_keys.h) and changes only through
// SaveGallerySettings, behind the admin events settings page.

inline constexpr std::string_view kGalleryErrorMaxPhotosOutOfRange = "MAX_PHOTOS_OUT_OF_RANGE";

// The API's name for the setting.
inline constexpr std::string_view kGallerySettingMaxPhotos = "event_gallery_max_photos";

// The cap's default and its allowed range, inclusive. 12 is enough to market a
// night without becoming an archive; 60 keeps a page's grid sane.
struct GalleryMaxPhotosRange {
    int64_t defaultValue = 0;
    int64_t min = 0;
    int64_t max = 0;
};
inline constexpr GalleryMaxPhotosRange kEventGalleryMaxPhotosRange{ 12, 1, 60 };

struct GallerySettings {
    // A gallery holding this many photos refuses another. Lowering the cap
    // keeps the photos already there.
    int64_t maxPhotos = kEventGalleryMaxPhotosRange.defaultValue;
};

// Absent = leave it as it is.
struct GallerySettingsUpdate {
    std::optional<int64_t> maxPhotos;
};

struct SaveGallerySettingsResult {
    bool ok = false;
    std::string errorCode;
    // On MAX_PHOTOS_OUT_OF_RANGE, the field's API name.
    std::string field;
    // The settings as they read after the save (on success).
    GallerySettings settings;
};

// The current settings. A cap that is missing, blank, not a whole number or out
// of range reads as the default.
GallerySettings LoadGallerySettings(Secrets::SecretsHelper& secrets, Transaction& transaction);

// Writes the fields present in `update`. An out-of-range cap writes nothing.
SaveGallerySettingsResult SaveGallerySettings(
    Secrets::SecretsHelper& secrets, Transaction& transaction, const GallerySettingsUpdate& update);

}  // namespace Events
