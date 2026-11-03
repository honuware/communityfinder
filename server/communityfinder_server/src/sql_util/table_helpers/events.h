#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for events, one row per dated occurrence. Single-table
// reads only: venue, series and category/scene-tag resolution is EventHelper's
// job (business_logic), batched through the other helpers.
class Events {
public:
    explicit Events(DatabaseHelper databaseHelper);
    Events(const Events&) = default;
    Events& operator=(const Events&) = default;
    ~Events() = default;

    // CREATE — throws when (source_id, external_key) is already taken. Events
    // with a NULL source_id or external_key never collide.
    int64_t AddEvent(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such event.
    KeyValueTable GetEvent(Transaction& transaction, int64_t id) const;

    // READ — the ingest's match: the event a source already gave this key, or
    // empty when it is new.
    KeyValueTable GetEventBySourceAndExternalKey(
        Transaction& transaction, int64_t sourceId, std::string_view externalKey) const;

    // READ — events in one status starting in [fromUs, toUs), ordered by start
    // then id. Shaped for the (status, starts_at_us) index; the public calendar
    // passes `approved`.
    KeyValueTableArray GetEventsInRange(
        Transaction& transaction, int64_t fromUs, int64_t toUs, std::string_view status) const;

    // READ — a series' instances in every status, ordered by start then id. The
    // caller picks the statuses and window it shows.
    KeyValueTableArray GetEventsBySeries(Transaction& transaction, int64_t seriesId) const;

    // READ — the review queue: pending events, soonest first (then id).
    KeyValueTableArray GetPendingEvents(Transaction& transaction) const;

    // READ — EV3: one submission's dates in every status, ordered by start then
    // id.
    KeyValueTableArray GetEventsBySubmission(
        Transaction& transaction, int64_t submissionId) const;

    // READ — EV3: many submissions' dates in one query ("My submissions", the
    // grouped queue), ordered by submission, then start, then id. Empty input,
    // empty result.
    KeyValueTableArray GetEventsBySubmissions(
        Transaction& transaction, const std::vector<int64_t>& submissionIds) const;

    // READ — EV3's pending-dates rate limit: how many dates are still waiting
    // for review across every submission the person sent. Counted through the
    // submission, not created_by_person_id, so an admin's own ingested or
    // hand-entered pending events never count against their submissions.
    int64_t CountPendingSubmittedBy(Transaction& transaction, int64_t personId) const;

    // READ — EV3's duplicate-hint scan: events in any of `statuses` starting in
    // [fromUs, toUs), ordered by start then id. Shaped for the
    // (status, starts_at_us) index. No statuses, no rows.
    KeyValueTableArray GetEventsStartingBetween(
        Transaction& transaction, int64_t fromUs, int64_t toUs,
        const std::vector<std::string_view>& statuses) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateEvent(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
