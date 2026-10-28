#pragma once

#include <cstdint>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for venues, Stream A's places table. The guide's profile
// (GD2) satellites onto venues.id and has its own helper; nothing guide-shaped
// lives here.
class Venues {
public:
    explicit Venues(DatabaseHelper databaseHelper);
    Venues(const Venues&) = default;
    Venues& operator=(const Venues&) = default;
    ~Venues() = default;

    // CREATE
    int64_t AddVenue(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such venue.
    KeyValueTable GetVenue(Transaction& transaction, int64_t id) const;

    // READ — many venues in one query (event-list assembly), ordered by id.
    // Unknown ids are skipped; empty input, empty result.
    KeyValueTableArray GetVenuesByIds(
        Transaction& transaction, const std::vector<int64_t>& ids) const;

    // READ — the venues in a neighborhood, any status, ordered by name then id.
    KeyValueTableArray GetVenuesByNeighborhood(
        Transaction& transaction, int64_t neighborhoodId) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateVenue(Transaction& transaction, int64_t id, const KeyValueTable& updates);

    // DELETE — cascades to the venue's events, series and sources (honuware FKs
    // always cascade), so it is for data-entry mistakes only; a venue that
    // closes gets status `closed` instead.
    void DeleteVenue(Transaction& transaction, int64_t id);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
