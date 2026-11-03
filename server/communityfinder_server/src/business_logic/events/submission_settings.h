#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "sql_util/database_access/transaction.h"
#include "util/secrets/secrets_helper.h"

namespace Events {

// EV3 §3.3 (OQ3) and §3.4 (OQ10) — the submission settings: the two rate
// limits and the pause switch. They live in config_secrets (keys in
// business_logic/app_secret_keys.h), out of the generic table editor's reach,
// so the only way to change them is SaveSubmissionSettings behind the
// Submission limits page — the Site Theme editor's pattern.

inline constexpr std::string_view kSubmissionErrorLimitOutOfRange = "LIMIT_OUT_OF_RANGE";

// The API's names for the settings. LIMIT_OUT_OF_RANGE names the field with
// these, and SubmissionHelper's RATE_LIMITED says which limit was hit with them.
inline constexpr std::string_view kSubmissionLimitMaxPendingDates = "max_pending_dates";
inline constexpr std::string_view kSubmissionLimitMaxSubmissionsPerDay = "max_submissions_per_day";
inline constexpr std::string_view kSubmissionSettingSubmissionsPaused = "submissions_paused";

// A limit's default and its allowed range, inclusive. The minimum of 1 means
// the limits can't be used to close the form; the pause switch does that.
struct SubmissionLimitRange {
    int64_t defaultValue = 0;
    int64_t min = 0;
    int64_t max = 0;
};
// Dates one person can have waiting for review, across all their submissions.
inline constexpr SubmissionLimitRange kMaxPendingDatesRange{ 10, 1, 100 };
// Submissions one person can send in a rolling 24 hours.
inline constexpr SubmissionLimitRange kMaxSubmissionsPerDayRange{ 5, 1, 50 };
// The form is open unless a curator pauses it.
inline constexpr bool kSubmissionsPausedDefault = false;

struct SubmissionSettings {
    int64_t maxPendingDates = kMaxPendingDatesRange.defaultValue;
    int64_t maxSubmissionsPerDay = kMaxSubmissionsPerDayRange.defaultValue;
    // True closes the submit form for everyone (a spam wave from many new
    // accounts, which the per-person limits can't stop).
    bool submissionsPaused = kSubmissionsPausedDefault;
};

// Absent = leave that setting as it is.
struct SubmissionSettingsUpdate {
    std::optional<int64_t> maxPendingDates;
    std::optional<int64_t> maxSubmissionsPerDay;
    std::optional<bool> submissionsPaused;
};

struct SaveSettingsResult {
    bool ok = false;
    std::string errorCode;
    // On LIMIT_OUT_OF_RANGE, the field's API name (kSubmissionLimit*).
    std::string field;
    // The settings as they read after the save (on success).
    SubmissionSettings settings;
};

// The current settings. A limit that is missing, blank, not a whole number or
// out of range reads as its default, so a bad row can never switch a limit
// off. Only the exact value "true" pauses submissions: a missing, blank or
// garbled value leaves the form open, so a bad row can never close it.
SubmissionSettings LoadSubmissionSettings(Secrets::SecretsHelper& secrets, Transaction& transaction);

// Writes the fields present in `update`. If any limit is out of range,
// nothing is written — the pause switch included (a half-saved form is worse
// than a refused one) — and the result names the first bad field.
SaveSettingsResult SaveSubmissionSettings(
    Secrets::SecretsHelper& secrets, Transaction& transaction, const SubmissionSettingsUpdate& update);

}  // namespace Events
