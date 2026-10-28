#pragma once

#include <cstdint>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for event_sources, the feeds and pages events are ingested
// from. EV5's scanner walks GetEnabledSources and stamps each one it visits with
// TouchLastScanned.
class EventSources {
public:
    explicit EventSources(DatabaseHelper databaseHelper);
    EventSources(const EventSources&) = default;
    EventSources& operator=(const EventSources&) = default;
    ~EventSources() = default;

    // CREATE
    int64_t AddSource(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such source.
    KeyValueTable GetSource(Transaction& transaction, int64_t id) const;

    // READ — the sources the scanner should visit, ordered by name then id.
    KeyValueTableArray GetEnabledSources(Transaction& transaction) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateSource(Transaction& transaction, int64_t id, const KeyValueTable& updates);

    // UPDATE — records a scan. Sets last_scanned_at_us only: updated_us keeps
    // meaning "a curator changed this source", so a weekly scan doesn't touch it.
    void TouchLastScanned(Transaction& transaction, int64_t id, int64_t nowUs);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
