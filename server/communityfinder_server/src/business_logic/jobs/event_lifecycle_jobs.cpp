#include "business_logic/jobs/event_lifecycle_jobs.h"

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "business_logic/events/event_ical.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "util/types.h"

namespace Jobs {

namespace {

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

// NULL reads back as "", which is "no value" here.
std::optional<int64_t> OptionalIdColumn(const KeyValueTable& row, std::string_view column) {
    std::string value = Column(row, column);
    if (value.empty()) {
        return std::nullopt;
    }
    return std::stoll(value);
}

int64_t StartOf(const KeyValueTable& event) {
    return std::stoll(Column(event, DbSchema::kEventsStartsAtUs));
}

// The scan's rule, checked again on the locked row.
bool EndedBefore(const KeyValueTable& event, int64_t cutoffUs) {
    const int64_t startsAtUs = StartOf(event);
    const int64_t endsAtUs = OptionalIdColumn(event, DbSchema::kEventsEndsAtUs)
        .value_or(startsAtUs + Events::kICalDefaultDurationUs);
    return startsAtUs < cutoffUs && endsAtUs < cutoffUs;
}

}  // namespace

EventLifecycleJobs::EventLifecycleJobs(DatabaseHelper databaseHelper)
    : events_(databaseHelper),
      submissions_(databaseHelper),
      submissionHelper_(databaseHelper) {
}

ArchiveResult EventLifecycleJobs::ArchivePastEvents(
    Transaction& transaction, int64_t nowUs, int64_t limit) {
    ArchiveResult result;
    const int64_t cutoffUs = nowUs - kArchiveGraceUs;
    for (const KeyValueTable& candidate : events_.GetApprovedEndedBefore(
             transaction, cutoffUs, Events::kICalDefaultDurationUs, limit)) {
        const int64_t eventId = std::stoll(Column(candidate, DbSchema::kEventsId));
        const KeyValueTable event = events_.GetEventForUpdate(transaction, eventId);
        if (Column(event, DbSchema::kEventsStatus) != DbSchema::kEventsStatusApproved
            || !EndedBefore(event, cutoffUs)) {
            continue;
        }
        events_.UpdateEvent(transaction, eventId, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusArchived) },
        });
        ++result.archived;
    }
    return result;
}

ExpireResult EventLifecycleJobs::ExpireUnreviewedEvents(
    Transaction& transaction, int64_t nowUs, int64_t limit) {
    ExpireResult result;
    std::set<int64_t> lockedSubmissions;
    std::set<int64_t> expiredSubmissions;
    // The submissions this run expired a date of, in the order first seen, each
    // with one such date (OnDateReviewed finds the submission through it).
    std::vector<std::pair<int64_t, int64_t>> touched;

    for (const KeyValueTable& candidate :
         events_.GetPendingStartedBefore(transaction, nowUs, limit)) {
        const int64_t eventId = std::stoll(Column(candidate, DbSchema::kEventsId));
        // The submission's lock before the event's, as every path that decides
        // a submitted date takes them.
        const std::optional<int64_t> submissionId =
            OptionalIdColumn(candidate, DbSchema::kEventsSubmissionId);
        if (submissionId && lockedSubmissions.insert(*submissionId).second) {
            submissions_.GetSubmissionForUpdate(transaction, *submissionId);
        }
        const KeyValueTable event = events_.GetEventForUpdate(transaction, eventId);
        if (Column(event, DbSchema::kEventsStatus) != DbSchema::kEventsStatusPending
            || StartOf(event) >= nowUs) {
            continue;
        }
        events_.UpdateEvent(transaction, eventId, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusRejected) },
            { std::string(DbSchema::kEventsReviewedByPersonId), "NULL" },
            { std::string(DbSchema::kEventsReviewedUs), std::string(DbSchema::kDatabaseInfoDefaultNow) },
            { std::string(DbSchema::kEventsReviewNotes), std::string(kReviewNoteDatePassed) },
        });
        ++result.expired;
        if (submissionId && expiredSubmissions.insert(*submissionId).second) {
            touched.emplace_back(*submissionId, eventId);
        }
    }

    for (const auto& [submissionId, eventId] : touched) {
        // A reason a curator already gave stays (EV3 v0.2 §1.1).
        if (Column(submissions_.GetSubmission(transaction, submissionId),
                   DbSchema::kEventSubmissionsRejectReason).empty()) {
            submissions_.UpdateSubmission(transaction, submissionId, {
                { std::string(DbSchema::kEventSubmissionsRejectReason),
                  std::string(DbSchema::kEventSubmissionsRejectReasonExpired) },
            });
        }
        if (std::optional<Events::DecisionNotice> notice =
                submissionHelper_.OnDateReviewed(transaction, eventId)) {
            result.submissionNotices.push_back(std::move(*notice));
        }
    }
    return result;
}

}  // namespace Jobs
