// communityfinder_helper (EV4 §6): runs the scheduled jobs. Each job is a POST
// to one of the server's /api/admin/<job> endpoints as the scheduler service
// account (scheduler@communityfinder.local, seeded by `database_helper
// --recreate_database` when SCHEDULER_SERVICE_ACCOUNT_PASSWORD is set). Fixed
// mode only: one server, one catalog. Multi-community control mode is PL4.
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "business_logic/auth/service_account.h"
#include "communityfinder_job_catalog.h"
#include "scheduler/scheduler.h"
#include "util/env.h"
#include "util/http/http_client.h"
#include "util/logging.h"

ABSL_FLAG(std::string, server_url, "http://localhost:18081",
    "Web server base URL for API calls");
ABSL_FLAG(std::string, service_account_email, "scheduler@communityfinder.local",
    "Email for authenticating to the job endpoints");
ABSL_FLAG(std::string, service_account_password, "",
    "Password for the service account. If empty, the helper falls back to "
    "the SCHEDULER_SERVICE_ACCOUNT_PASSWORD environment variable.");

// One interval flag per catalog job (CommunityFinder::JobIntervals), in
// seconds. 0 disables the job.
ABSL_FLAG(int64_t, archive_past_events_interval, 3600,
    "Interval (seconds) between archive_past_events calls: approved events that "
    "ended more than six hours ago become archived. 0 disables.");
ABSL_FLAG(int64_t, expire_unreviewed_events_interval, 3600,
    "Interval (seconds) between expire_unreviewed_events calls: pending events "
    "whose start has passed are rejected. 0 disables.");
ABSL_FLAG(int64_t, token_cleanup_interval, 86400,
    "Interval (seconds) between cleanup_expired_tokens calls. 0 disables.");
ABSL_FLAG(int64_t, login_attempts_cleanup_interval, 86400,
    "Interval (seconds) between cleanup_login_attempts calls. 0 disables.");
ABSL_FLAG(int64_t, photo_cleanup_interval, 86400,
    "Interval (seconds) between cleanup_scaled_photos calls. 0 disables.");
ABSL_FLAG(int64_t, idempotency_cleanup_interval, 86400,
    "Interval (seconds) between cleanup_idempotency_keys calls. 0 disables.");
ABSL_FLAG(int64_t, admin_alerts_notify_interval, 3600,
    "Interval (seconds) between notify_admin_alerts calls. The digest mails "
    "only when there are new alerts, so hourly is right. 0 disables.");
ABSL_FLAG(int64_t, import_event_feeds_interval, 0,
    "Reserved for EV6 (feed imports); its endpoint doesn't exist yet, so leave "
    "at 0.");
ABSL_FLAG(int64_t, run_event_scan_interval, 0,
    "Reserved for EV5 (the event scan); its endpoint doesn't exist yet, so "
    "leave at 0.");

namespace {

// Once a CDN guards the origin (PL2), the helper talks to the origin directly
// and must send the shared secret on every request, the login included. Read
// the same way as the server's guard (CloudFrontOriginGuard).
void MaybeAddOriginSecretHeader(CommunityFinder::JobTarget& target) {
    const char* secret = Util::GetEnvWithFallback("HONUWARE_ORIGIN_SECRET", "KNOTTYYOGA_ORIGIN_SECRET");
    if (secret != nullptr && secret[0] != '\0') {
        target.headers.emplace_back("X-Origin-Secret", std::string(secret));
    }
}

CommunityFinder::JobTarget BuildTargetFromFlags() {
    CommunityFinder::JobTarget target;
    target.serverUrl = absl::GetFlag(FLAGS_server_url);
    target.serviceAccountEmail = absl::GetFlag(FLAGS_service_account_email);
    // The variable's name lives in service_account.h, so the seed and the
    // helper can't disagree on it.
    target.serviceAccountPassword = CommunityFinder::ResolveServiceAccountPassword(
        absl::GetFlag(FLAGS_service_account_password), Auth::kSchedulerServiceAccountPasswordEnvVar);
    MaybeAddOriginSecretHeader(target);
    return target;
}

CommunityFinder::JobIntervals BuildIntervalsFromFlags() {
    CommunityFinder::JobIntervals intervals;
    intervals.archivePastEventsSeconds = absl::GetFlag(FLAGS_archive_past_events_interval);
    intervals.expireUnreviewedEventsSeconds = absl::GetFlag(FLAGS_expire_unreviewed_events_interval);
    intervals.tokenCleanupSeconds = absl::GetFlag(FLAGS_token_cleanup_interval);
    intervals.loginAttemptsCleanupSeconds = absl::GetFlag(FLAGS_login_attempts_cleanup_interval);
    intervals.photoCleanupSeconds = absl::GetFlag(FLAGS_photo_cleanup_interval);
    intervals.idempotencyCleanupSeconds = absl::GetFlag(FLAGS_idempotency_cleanup_interval);
    intervals.adminAlertsNotifySeconds = absl::GetFlag(FLAGS_admin_alerts_notify_interval);
    intervals.importEventFeedsSeconds = absl::GetFlag(FLAGS_import_event_feeds_interval);
    intervals.runEventScanSeconds = absl::GetFlag(FLAGS_run_event_scan_interval);
    return intervals;
}

// One line with the whole configuration (never the password itself).
void LogConfigSummary(const CommunityFinder::JobTarget& target,
                      const CommunityFinder::JobIntervals& intervals) {
    LogInfo() << "[scheduler] event=startup_config"
              << " server_url=" << target.serverUrl
              << " service_account_email=" << target.serviceAccountEmail
              << " service_account_password=" << (target.serviceAccountPassword.empty() ? "unset" : "set")
              << " origin_secret=" << (target.headers.empty() ? "unset" : "set")
              << " archive_past_events=" << intervals.archivePastEventsSeconds
              << " expire_unreviewed_events=" << intervals.expireUnreviewedEventsSeconds
              << " token_cleanup=" << intervals.tokenCleanupSeconds
              << " login_attempts_cleanup=" << intervals.loginAttemptsCleanupSeconds
              << " photo_cleanup=" << intervals.photoCleanupSeconds
              << " idempotency_cleanup=" << intervals.idempotencyCleanupSeconds
              << " admin_alerts_notify=" << intervals.adminAlertsNotifySeconds
              << " import_event_feeds=" << intervals.importEventFeedsSeconds
              << " run_event_scan=" << intervals.runEventScanSeconds
              << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    InitializeLogging();
    absl::ParseCommandLine(argc, argv);

    CommunityFinder::JobTarget target = BuildTargetFromFlags();
    CommunityFinder::JobIntervals intervals = BuildIntervalsFromFlags();

    std::vector<std::string> errors = CommunityFinder::ValidateCommunityFinderConfig(target, intervals);
    if (!errors.empty()) {
        std::cerr << "Invalid scheduler configuration:\n";
        for (const std::string& error : errors) {
            std::cerr << "  - " << error << "\n";
        }
        return 1;
    }

    LogConfigSummary(target, intervals);

    Scheduler::Scheduler scheduler(CommunityFinder::BuildCommunityFinderJobs(target, intervals),
                                   Http::MakeHttpClient());
    // Initialize() returns false when the server refuses the login, and throws
    // when it can't be reached at all (the HTTP client's transport error), as in
    // knottyyoga's main. Uncaught, that throw aborts the helper with no message.
    std::string failure;
    try {
        if (!scheduler.Initialize()) failure = "the login was refused";
    } catch (const std::exception& e) {
        failure = e.what();
    }
    if (!failure.empty()) {
        // Is the server up, and was the account seeded with the same password?
        std::cerr << "Scheduler initialization failed (could not log in as "
                  << target.serviceAccountEmail << " at " << target.serverUrl
                  << ": " << failure << "). Is the server running, and was the "
                     "database seeded (--recreate_database) with the same "
                     "SCHEDULER_SERVICE_ACCOUNT_PASSWORD?\n";
        return 1;
    }

    // Blocks until SIGINT/SIGTERM, then shuts down gracefully.
    scheduler.Run();
    return 0;
}
