#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// GD2 §2.3 — CRUD for places: parks, beaches, landmarks, and historical sites.
class Places {
public:
    explicit Places(DatabaseHelper databaseHelper);
    Places(const Places&) = default;
    Places& operator=(const Places&) = default;
    ~Places() = default;

    // CREATE — throws on a duplicate slug or a place_kind outside GD1's list.
    int64_t AddPlace(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such place.
    KeyValueTable GetPlace(Transaction& transaction, int64_t id) const;

    // READ — by slug (the place page's URL key); empty when there is none.
    KeyValueTable GetPlaceBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — one kind's places, any status, ordered by name then id.
    KeyValueTableArray GetPlacesByKind(
        Transaction& transaction, std::string_view placeKind) const;

    // READ — the places in a neighborhood, any status, ordered by name then id.
    KeyValueTableArray GetPlacesByNeighborhood(
        Transaction& transaction, int64_t neighborhoodId) const;

    // READ — every place, ordered by name then id. Without includeClosed, the
    // retired statuses (closed, moved) are left out; seasonal and unverified stay.
    KeyValueTableArray GetPlaces(Transaction& transaction, bool includeClosed) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdatePlace(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
