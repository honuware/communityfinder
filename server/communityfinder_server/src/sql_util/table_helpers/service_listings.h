#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// GD2 §2.3 — CRUD for service_listings: clinics, testing sites, therapists,
// and everyday services.
class ServiceListings {
public:
    explicit ServiceListings(DatabaseHelper databaseHelper);
    ServiceListings(const ServiceListings&) = default;
    ServiceListings& operator=(const ServiceListings&) = default;
    ~ServiceListings() = default;

    // CREATE — throws on a duplicate slug or a service_kind outside GD1's list.
    // GD1 §5's cadences: health listings re-check semiannually (the column
    // default) and everyday services annually. A column default can't read
    // is_health, so a listing created with is_health false and no
    // volatility_tier of its own gets annual here (GD2 OQ9 (a)). is_health
    // itself defaults to true.
    int64_t AddServiceListing(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such listing.
    KeyValueTable GetServiceListing(Transaction& transaction, int64_t id) const;

    // READ — by slug; empty when there is none.
    KeyValueTable GetServiceListingBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — one kind's listings, any status, ordered by name then id.
    KeyValueTableArray GetServiceListingsByKind(
        Transaction& transaction, std::string_view serviceKind) const;

    // READ — the listings in a neighborhood, any status, ordered by name then id.
    KeyValueTableArray GetServiceListingsByNeighborhood(
        Transaction& transaction, int64_t neighborhoodId) const;

    // READ — every listing, ordered by name then id. Without includeClosed, the
    // retired statuses (closed, moved) are left out; seasonal and unverified stay.
    KeyValueTableArray GetServiceListings(Transaction& transaction, bool includeClosed) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    // Leaves volatility_tier alone, even when is_health changes.
    void UpdateServiceListing(
        Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
