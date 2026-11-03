#include "business_logic/events/submission_settings.h"

#include <charconv>

#include "business_logic/app_secret_keys.h"
#include "util/types.h"

namespace Events {

namespace {

constexpr std::string_view kStoredTrue = "true";
constexpr std::string_view kStoredFalse = "false";

bool InRange(int64_t value, const SubmissionLimitRange& range) {
    return value >= range.min && value <= range.max;
}

// The whole string must be a number: " 10" and "10x" are junk, not 10.
int64_t ReadLimit(Secrets::SecretsHelper& secrets, Transaction& transaction,
                  std::string_view key, const SubmissionLimitRange& range) {
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

SubmissionSettings LoadSubmissionSettings(Secrets::SecretsHelper& secrets, Transaction& transaction) {
    SubmissionSettings settings;
    settings.maxPendingDates = ReadLimit(
        secrets, transaction, Secrets::kEventSubmissionMaxPendingDates, kMaxPendingDatesRange);
    settings.maxSubmissionsPerDay = ReadLimit(
        secrets, transaction, Secrets::kEventSubmissionMaxPerDay, kMaxSubmissionsPerDayRange);
    settings.submissionsPaused =
        secrets.LookupSecret(transaction, Secrets::kEventSubmissionsPaused) == kStoredTrue;
    return settings;
}

SaveSettingsResult SaveSubmissionSettings(
    Secrets::SecretsHelper& secrets, Transaction& transaction, const SubmissionSettingsUpdate& update) {
    SaveSettingsResult result;
    // Every field is checked before any is written.
    if (update.maxPendingDates && !InRange(*update.maxPendingDates, kMaxPendingDatesRange)) {
        result.errorCode = std::string(kSubmissionErrorLimitOutOfRange);
        result.field = std::string(kSubmissionLimitMaxPendingDates);
        return result;
    }
    if (update.maxSubmissionsPerDay
        && !InRange(*update.maxSubmissionsPerDay, kMaxSubmissionsPerDayRange)) {
        result.errorCode = std::string(kSubmissionErrorLimitOutOfRange);
        result.field = std::string(kSubmissionLimitMaxSubmissionsPerDay);
        return result;
    }

    if (update.maxPendingDates) {
        secrets.AddSecret(transaction, Secrets::kEventSubmissionMaxPendingDates,
                          StringFromInt(*update.maxPendingDates));
    }
    if (update.maxSubmissionsPerDay) {
        secrets.AddSecret(transaction, Secrets::kEventSubmissionMaxPerDay,
                          StringFromInt(*update.maxSubmissionsPerDay));
    }
    if (update.submissionsPaused) {
        secrets.AddSecret(transaction, Secrets::kEventSubmissionsPaused,
                          *update.submissionsPaused ? kStoredTrue : kStoredFalse);
    }
    result.ok = true;
    result.settings = LoadSubmissionSettings(secrets, transaction);
    return result;
}

}  // namespace Events
