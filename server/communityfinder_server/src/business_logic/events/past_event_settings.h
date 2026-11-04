#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "sql_util/database_access/transaction.h"
#include "util/secrets/secrets_helper.h"

namespace Events {

// EV4 §10 (OQ8, OQ9) — the past-events settings: how long an event stays
// public after it ends, and whether the calendar shows past events to a visitor
// who hasn't chosen. Like the submission settings, they live in config_secrets
// (keys in business_logic/app_secret_keys.h) and change only through
// SavePastEventSettings, behind the admin events settings page.

inline constexpr std::string_view kPastEventErrorDaysOutOfRange = "DAYS_OUT_OF_RANGE";

// The API's names for the settings.
inline constexpr std::string_view kPastEventSettingVisibleDays = "archived_events_visible_days";
inline constexpr std::string_view kPastEventSettingShowByDefault = "events_show_past_by_default";

// The window's default and its allowed range in days, inclusive. The minimum
// of 1 keeps last night's event on the subscribe feed, which reaches back a
// day (kFeedLookBackDays).
struct PastEventDaysRange {
    int64_t defaultValue = 0;
    int64_t min = 0;
    int64_t max = 0;
};
inline constexpr PastEventDaysRange kArchivedEventsVisibleDaysRange{ 365, 1, 730 };
// Off: most visitors don't want a month of events they missed (OQ9).
inline constexpr bool kEventsShowPastByDefaultDefault = false;

struct PastEventSettings {
    // An event that ended more than this many days ago is hidden from the
    // public: its page, its .ics, the calendar and the past-events list. The
    // row stays, so a feed can't re-import it (OQ8).
    int64_t archivedEventsVisibleDays = kArchivedEventsVisibleDaysRange.defaultValue;
    bool showPastByDefault = kEventsShowPastByDefaultDefault;
};

// Absent = leave that setting as it is.
struct PastEventSettingsUpdate {
    std::optional<int64_t> archivedEventsVisibleDays;
    std::optional<bool> showPastByDefault;
};

struct SavePastEventSettingsResult {
    bool ok = false;
    std::string errorCode;
    // On DAYS_OUT_OF_RANGE, the field's API name.
    std::string field;
    // The settings as they read after the save (on success).
    PastEventSettings settings;
};

// The current settings. A window that is missing, blank, not a whole number or
// out of range reads as the default. Only the exact value "true" starts the
// switch on.
PastEventSettings LoadPastEventSettings(Secrets::SecretsHelper& secrets, Transaction& transaction);

// Writes the fields present in `update`. An out-of-range window writes nothing,
// the switch included.
SavePastEventSettingsResult SavePastEventSettings(
    Secrets::SecretsHelper& secrets, Transaction& transaction, const PastEventSettingsUpdate& update);

}  // namespace Events
