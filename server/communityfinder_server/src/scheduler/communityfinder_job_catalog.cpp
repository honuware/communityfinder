#include "communityfinder_job_catalog.h"

#include <cstdlib>

namespace CommunityFinder {

namespace {

// Every CF job is a POST with an empty body to /api/admin/<name>, gated on
// run_scheduled_jobs (which the seed grants the scheduler service account).
void AppendIfEnabled(std::vector<Scheduler::ScheduledJob>& jobs, const JobTarget& target,
                     std::string_view name, int64_t intervalSeconds) {
    if (intervalSeconds <= 0) {
        return;
    }
    Scheduler::ScheduledJob job;
    job.name = std::string(name);
    job.method = "POST";
    job.path = "/api/admin/" + std::string(name);
    job.intervalSeconds = intervalSeconds;
    job.serverUrl = target.serverUrl;
    job.serviceAccountEmail = target.serviceAccountEmail;
    job.serviceAccountPassword = target.serviceAccountPassword;
    job.headers = target.headers;
    jobs.push_back(std::move(job));
}

void CheckInterval(std::vector<std::string>& errors, std::string_view flag, int64_t value) {
    if (value < 0) {
        errors.push_back(std::string(flag) + " must be >= 0 (0 disables the job)");
    }
}

}  // namespace

std::vector<Scheduler::ScheduledJob> BuildCommunityFinderJobs(
    const JobTarget& target, const JobIntervals& intervals) {
    std::vector<Scheduler::ScheduledJob> jobs;
    AppendIfEnabled(jobs, target, "archive_past_events", intervals.archivePastEventsSeconds);
    AppendIfEnabled(jobs, target, "expire_unreviewed_events", intervals.expireUnreviewedEventsSeconds);
    AppendIfEnabled(jobs, target, "cleanup_expired_tokens", intervals.tokenCleanupSeconds);
    AppendIfEnabled(jobs, target, "cleanup_login_attempts", intervals.loginAttemptsCleanupSeconds);
    AppendIfEnabled(jobs, target, "cleanup_scaled_photos", intervals.photoCleanupSeconds);
    AppendIfEnabled(jobs, target, "cleanup_idempotency_keys", intervals.idempotencyCleanupSeconds);
    AppendIfEnabled(jobs, target, "notify_admin_alerts", intervals.adminAlertsNotifySeconds);
    AppendIfEnabled(jobs, target, "import_event_feeds", intervals.importEventFeedsSeconds);
    AppendIfEnabled(jobs, target, "run_event_scan", intervals.runEventScanSeconds);
    return jobs;
}

std::vector<std::string> ValidateCommunityFinderConfig(
    const JobTarget& target, const JobIntervals& intervals) {
    std::vector<std::string> errors;
    if (target.serverUrl.empty()) {
        errors.push_back("server_url must be set");
    }
    if (target.serviceAccountEmail.empty()) {
        errors.push_back("service_account_email must be set");
    }
    if (target.serviceAccountPassword.empty()) {
        errors.push_back(
            "service_account_password must be supplied via --service_account_password "
            "or the SCHEDULER_SERVICE_ACCOUNT_PASSWORD environment variable");
    }
    CheckInterval(errors, "archive_past_events_interval", intervals.archivePastEventsSeconds);
    CheckInterval(errors, "expire_unreviewed_events_interval", intervals.expireUnreviewedEventsSeconds);
    CheckInterval(errors, "token_cleanup_interval", intervals.tokenCleanupSeconds);
    CheckInterval(errors, "login_attempts_cleanup_interval", intervals.loginAttemptsCleanupSeconds);
    CheckInterval(errors, "photo_cleanup_interval", intervals.photoCleanupSeconds);
    CheckInterval(errors, "idempotency_cleanup_interval", intervals.idempotencyCleanupSeconds);
    CheckInterval(errors, "admin_alerts_notify_interval", intervals.adminAlertsNotifySeconds);
    CheckInterval(errors, "import_event_feeds_interval", intervals.importEventFeedsSeconds);
    CheckInterval(errors, "run_event_scan_interval", intervals.runEventScanSeconds);
    return errors;
}

std::string ResolveServiceAccountPassword(
    std::string_view flagValue, std::string_view envVarName) {
    if (!flagValue.empty()) {
        return std::string(flagValue);
    }
    const char* envValue = std::getenv(std::string(envVarName).c_str());
    return envValue == nullptr ? std::string() : std::string(envValue);
}

}  // namespace CommunityFinder
