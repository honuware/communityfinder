#include "business_logic/events/gallery_settings.h"

#include <charconv>

#include "business_logic/app_secret_keys.h"
#include "util/types.h"

namespace Events {

namespace {

bool InRange(int64_t value) {
    return value >= kEventGalleryMaxPhotosRange.min && value <= kEventGalleryMaxPhotosRange.max;
}

}  // namespace

GallerySettings LoadGallerySettings(Secrets::SecretsHelper& secrets, Transaction& transaction) {
    GallerySettings settings;
    // The whole string must be a number: " 12" and "12x" are junk, not 12.
    const std::string stored = secrets.LookupSecret(transaction, Secrets::kEventGalleryMaxPhotos);
    int64_t value = 0;
    const char* end = stored.data() + stored.size();
    auto [parsedTo, error] = std::from_chars(stored.data(), end, value);
    if (!stored.empty() && error == std::errc() && parsedTo == end && InRange(value)) {
        settings.maxPhotos = value;
    }
    return settings;
}

SaveGallerySettingsResult SaveGallerySettings(
    Secrets::SecretsHelper& secrets, Transaction& transaction, const GallerySettingsUpdate& update) {
    SaveGallerySettingsResult result;
    if (update.maxPhotos && !InRange(*update.maxPhotos)) {
        result.errorCode = std::string(kGalleryErrorMaxPhotosOutOfRange);
        result.field = std::string(kGallerySettingMaxPhotos);
        return result;
    }
    if (update.maxPhotos) {
        secrets.AddSecret(transaction, Secrets::kEventGalleryMaxPhotos, StringFromInt(*update.maxPhotos));
    }
    result.ok = true;
    result.settings = LoadGallerySettings(secrets, transaction);
    return result;
}

}  // namespace Events
