#pragma once

#include <cstdint>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV3 §2.1 — CRUD for event_submissions, the envelope of one public submission.
// Its dates are ordinary events rows (Events::GetEventsBySubmission); validation,
// rate limits and the decision email are SubmissionHelper's (business_logic).
class EventSubmissions {
public:
    explicit EventSubmissions(DatabaseHelper databaseHelper);
    EventSubmissions(const EventSubmissions&) = default;
    EventSubmissions& operator=(const EventSubmissions&) = default;
    ~EventSubmissions() = default;

    // CREATE — throws on an unknown relationship or reject reason (the CHECKs).
    int64_t AddSubmission(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such submission.
    KeyValueTable GetSubmission(Transaction& transaction, int64_t id) const;

    // READ — GetSubmission, holding the row's lock until the transaction ends.
    // Every path that decides a submission's dates takes it first, so a second
    // curator waits and then sees the first one's decisions: without it, two
    // curators deciding the last two dates at once would each still see the
    // other's date as pending, and neither would send the decision email.
    // Taking it before touching the dates also keeps the lock order the same
    // everywhere (submission, then events), so they can't deadlock.
    KeyValueTable GetSubmissionForUpdate(Transaction& transaction, int64_t id);

    // READ — "My submissions": one person's, newest first (then id). Shaped for
    // the (submitted_by_person_id, created_us) index.
    KeyValueTableArray GetSubmissionsForPerson(
        Transaction& transaction, int64_t personId) const;

    // READ — the rolling-window rate limit: how many submissions the person sent
    // at or after sinceUs.
    int64_t CountSubmissionsSince(
        Transaction& transaction, int64_t personId, int64_t sinceUs) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateSubmission(Transaction& transaction, int64_t id, const KeyValueTable& updates);

    // UPDATE — stamps decision_notified_us, but only if it is still empty, and
    // says whether this call stamped it. The decision email is sent only by the
    // caller that wins the stamp, so it goes out once even when two curators
    // decide the last two dates at the same moment (the second waits on the
    // row lock, then finds it stamped). Bumps updated_us when it stamps.
    bool MarkDecisionNotified(Transaction& transaction, int64_t id, int64_t nowUs);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
