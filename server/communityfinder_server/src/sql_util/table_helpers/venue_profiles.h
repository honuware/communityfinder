#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// GD2 §2.1 — CRUD for venue_profiles, the guide's satellite on a venue
// (Interface Contract 2). The venue's name and status live on its venues row;
// GuideHelper joins the two.
class VenueProfiles {
public:
    explicit VenueProfiles(DatabaseHelper databaseHelper);
    VenueProfiles(const VenueProfiles&) = default;
    VenueProfiles& operator=(const VenueProfiles&) = default;
    ~VenueProfiles() = default;

    // CREATE — throws when the venue already has a profile or the slug is taken.
    int64_t AddProfile(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such profile.
    KeyValueTable GetProfile(Transaction& transaction, int64_t id) const;

    // READ — the venue's profile; empty when it has none.
    KeyValueTable GetProfileByVenueId(Transaction& transaction, int64_t venueId) const;

    // READ — by slug (the guide page's URL key); empty when there is none.
    KeyValueTable GetProfileBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — every profile, ordered by slug. Has no status of its own, so a
    // closed venue's profile is here too (the graveyard reads it).
    KeyValueTableArray GetProfiles(Transaction& transaction) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateProfile(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
