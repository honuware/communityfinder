#include "business_logic/events/past_event_settings.h"

#include <charconv>

#include "business_logic/app_secret_keys.h"
#include "util/types.h"

namespace Events {

namespace {

constexpr std::string_view kStoredTrue = "true";
constexpr std::string_view kStoredFalse = "false";

bool InRange(int64_t value, const PastEventDaysRange& range) {
    return value >= range.min && value <= range.max;
}

// The whole string must be a number: " 30" and "30x" are junk, not 30.
int64_t ReadDays(Secrets::SecretsHelper& secrets, Transaction& transaction,
                 std::string_view key, const PastEventDaysRange& range) {
    const std::string stored = secrets.LookupSecret(transaction, key);
    int64_t value = 0;
    const char* end = stored.data() + stored.size();
    auto [parsedTo, error] = std::from_chars(stored.data(), end, value);
    if (stored.empty() || error != std::errc() || parsedTo != end || !InRange(value, range)) {
        return range.defaultValue;
    }
    return value;
}

}  // namespace

PastEventSettings LoadPastEventSettings(Secrets::SecretsHelper& secrets, Transaction& transaction) {
    PastEventSettings settings;
    settings.archivedEventsVisibleDays = ReadDays(
        secrets, transaction, Secrets::kArchivedEventsVisibleDays, kArchivedEventsVisibleDaysRange);
    settings.showPastByDefault =
        secrets.LookupSecret(transaction, Secrets::kEventsShowPastByDefault) == kStoredTrue;
    return settings;
}

SavePastEventSettingsResult SavePastEventSettings(
    Secrets::SecretsHelper& secrets, Transaction& transaction, const PastEventSettingsUpdate& update) {
    SavePastEventSettingsResult result;
    if (update.archivedEventsVisibleDays
        && !InRange(*update.archivedEventsVisibleDays, kArchivedEventsVisibleDaysRange)) {
        result.errorCode = std::string(kPastEventErrorDaysOutOfRange);
        result.field = std::string(kPastEventSettingVisibleDays);
        return result;
    }

    if (update.archivedEventsVisibleDays) {
        secrets.AddSecret(transaction, Secrets::kArchivedEventsVisibleDays,
                          StringFromInt(*update.archivedEventsVisibleDays));
    }
    if (update.showPastByDefault) {
        secrets.AddSecret(transaction, Secrets::kEventsShowPastByDefault,
                          *update.showPastByDefault ? kStoredTrue : kStoredFalse);
    }
    result.ok = true;
    result.settings = LoadPastEventSettings(secrets, transaction);
    return result;
}

}  // namespace Events
