#pragma once

#include <cstdint>
#include <string_view>

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

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateEvent(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
