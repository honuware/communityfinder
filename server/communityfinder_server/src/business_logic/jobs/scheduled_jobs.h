#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "business_logic/scheduled_jobs/platform_jobs.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_access/transaction.h"
#include "util/json_value.h"
#include "util/mail/mail_helper.h"
#include "util/secrets/secrets_helper.h"

namespace Jobs {

// EV4 — the scheduled jobs by name: what each /api/admin/<name> endpoint runs
// (inside its run_scheduled_jobs gate) and what the test_helper's run-job runs
// without a server. Each runs in the caller's one transaction, decides for
// itself whether there is work, and returns its counts.
//
// The app's own two are below. The cleanups and the admin-alerts digest are
// honuware's (PlatformJobs, EV4 §9); they're listed here under the names the
// app has always used, so the helper's catalog and the test_helper are
// unchanged.

inline constexpr std::string_view kJobArchivePastEvents = "archive_past_events";
inline constexpr std::string_view kJobExpireUnreviewedEvents = "expire_unreviewed_events";
inline constexpr std::string_view kJobCleanupExpiredTokens = PlatformJobs::kJobCleanupExpiredTokens;
inline constexpr std::string_view kJobCleanupLoginAttempts = PlatformJobs::kJobCleanupLoginAttempts;
inline constexpr std::string_view kJobCleanupScaledPhotos = PlatformJobs::kJobCleanupScaledPhotos;
inline constexpr std::string_view kJobCleanupIdempotencyKeys = PlatformJobs::kJobCleanupIdempotencyKeys;
inline constexpr std::string_view kJobNotifyAdminAlerts = PlatformJobs::kJobNotifyAdminAlerts;

// Every job above, in that order.
const std::vector<std::string_view>& ScheduledJobNames();

// Runs the named job and returns its counts; nullopt for an unknown name.
//   archive_past_events      {"archived"}
//   expire_unreviewed_events {"expired", "decision_emails_sent"}
//   the other five           as PlatformJobs::RunPlatformJob documents
// Throws when a job fails (only the digest's send can); the caller's
// transaction then rolls back.
std::optional<Json::Value> RunScheduledJob(
    Transaction& transaction,
    DatabaseHelper databaseHelper,
    Secrets::SecretsHelper& secrets,
    const ::Mail::MailHelperPtr& mailHelper,
    std::string_view jobName);

}  // namespace Jobs
