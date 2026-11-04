#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "scheduler/scheduled_job.h"

// EV4 §6 — CommunityFinder's scheduler job catalog: the app half of the
// honuware_scheduler split (knottyyoga's knottyyoga_job_catalog, with CF's
// jobs). The engine is brand-free and runs whatever list of
// Scheduler::ScheduledJob it is given; this file owns which admin endpoints CF
// calls, how often, and the shared login target. Multi-community fan-out (PL4)
// is a loop over BuildCommunityFinderJobs with one JobTarget each, as
// knottyyoga's BuildMultiTenantKnottyYogaJobs shows.
namespace CommunityFinder {

// Per-job intervals in seconds. 0 disables a job.
struct JobIntervals {
    // Approved events that ended more than six hours ago → archived.
    int64_t archivePastEventsSeconds = 3600;        // hourly
    // Pending events whose start passed → rejected ("Date passed before review").
    int64_t expireUnreviewedEventsSeconds = 3600;   // hourly
    // Hygiene, mirrored from knottyyoga (SUTP Q12).
    int64_t tokenCleanupSeconds = 86400;            // daily
    int64_t loginAttemptsCleanupSeconds = 86400;    // daily
    int64_t photoCleanupSeconds = 86400;            // daily
    int64_t idempotencyCleanupSeconds = 86400;      // daily
    // The admin-alerts digest; it mails only when there is something new.
    int64_t adminAlertsNotifySeconds = 3600;        // hourly
    // Reserved, off: EV6 adds /api/admin/import_event_feeds and EV5
    // /api/admin/run_event_scan. Until then the routes don't exist, so leave
    // these at 0.
    int64_t importEventFeedsSeconds = 0;
    int64_t runEventScanSeconds = 0;
};

// The login target and headers stamped onto every job.
struct JobTarget {
    std::string serverUrl = "http://localhost:18081";
    std::string serviceAccountEmail = "scheduler@communityfinder.local";
    std::string serviceAccountPassword;
    // Extra request headers on every job, e.g. the X-Origin-Secret once a CDN
    // sits in front (PL2).
    std::vector<std::pair<std::string, std::string>> headers;
};

// One Scheduler::ScheduledJob per enabled job: a POST to /api/admin/<name>,
// stamped with the target's URL, credentials and headers. A job whose interval
// is <= 0 is left out.
std::vector<Scheduler::ScheduledJob> BuildCommunityFinderJobs(
    const JobTarget& target, const JobIntervals& intervals);

// Every problem with the configuration, one message each, named by its flag;
// empty when it is usable.
std::vector<std::string> ValidateCommunityFinderConfig(
    const JobTarget& target, const JobIntervals& intervals);

// The flag's value when it is set, else the named environment variable's, else
// "" (which ValidateCommunityFinderConfig reports).
std::string ResolveServiceAccountPassword(
    std::string_view flagValue, std::string_view envVarName);

}  // namespace CommunityFinder
