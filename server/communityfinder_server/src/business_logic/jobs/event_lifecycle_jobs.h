#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "business_logic/events/submission_email.h"
#include "business_logic/events/submission_helper.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/table_helpers/event_submissions.h"
#include "sql_util/table_helpers/events.h"

namespace Jobs {

// EV4 §4.1 — the two event sweeps behind the archive_past_events and
// expire_unreviewed_events jobs. They are separate because they mean different
// things: an approved past event happened, so it is archived and stays readable;
// a pending past event was never decided, so it is rejected and never shows.
// That keeps "archived" meaning "happened and was listed."
//
// Each handles at most `limit` events, oldest first, so a first run on a large
// database can't hold a long transaction; the hourly interval drains a backlog.
// Both are idempotent: a second run finds nothing to do. Each event is re-read
// under its row lock before it changes, so a curator's decision made after the
// scan is seen, not overwritten.

// An approved event is archived once it ended this long ago.
inline constexpr int64_t kArchiveGraceUs = 6LL * 3'600LL * 1'000'000LL;
// The sweeps' batch size.
inline constexpr int64_t kEventSweepBatchLimit = 500;
// The review note an expired date carries.
inline constexpr std::string_view kReviewNoteDatePassed = "Date passed before review";

struct ArchiveResult {
    int64_t archived = 0;
};

struct ExpireResult {
    int64_t expired = 0;
    // One per EV3 submission whose last pending date this run closed, already
    // claimed (OnDateReviewed). Send each with SubmissionHelper::
    // SendDecisionNotice once the sweep is done.
    std::vector<Events::DecisionNotice> submissionNotices;
};

class EventLifecycleJobs {
public:
    explicit EventLifecycleJobs(DatabaseHelper databaseHelper);
    EventLifecycleJobs(const EventLifecycleJobs&) = default;
    EventLifecycleJobs& operator=(const EventLifecycleJobs&) = default;
    ~EventLifecycleJobs() = default;

    // Approved events that ENDED more than kArchiveGraceUs before `nowUs` →
    // archived. "Ended" is ends_at_us, or starts_at_us + two hours without one
    // (EV2's ICS default). Bumps updated_us; the review fields stay.
    ArchiveResult ArchivePastEvents(Transaction& transaction, int64_t nowUs, int64_t limit);

    // Pending events whose START is before `nowUs` → rejected, with no
    // reviewer, reviewed_us = now_us(), and kReviewNoteDatePassed. For a
    // submitted date, the submission's reason becomes `expired` when it has
    // none, and when this closes its last pending date the decision email is
    // claimed and returned for the caller to send.
    ExpireResult ExpireUnreviewedEvents(Transaction& transaction, int64_t nowUs, int64_t limit);

private:
    TableHelpers::Events events_;
    TableHelpers::EventSubmissions submissions_;
    Events::SubmissionHelper submissionHelper_;
};

}  // namespace Jobs
