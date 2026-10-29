#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined scene tag's slug on every GetFor* row.
inline constexpr std::string_view kVenueProfileSceneTagAssignmentsSceneTagSlug = "scene_tag_slug";

// GD2 §2.5 — CRUD for venue_profile_scene_tag_assignments: whose crowd a venue
// draws. Reads join scene_tags for the slug, because the guide's pages and
// filters work in slugs.
class VenueProfileSceneTagAssignments {
public:
    explicit VenueProfileSceneTagAssignments(DatabaseHelper databaseHelper);
    VenueProfileSceneTagAssignments(const VenueProfileSceneTagAssignments&) = default;
    VenueProfileSceneTagAssignments& operator=(const VenueProfileSceneTagAssignments&) = default;
    ~VenueProfileSceneTagAssignments() = default;

    // CREATE — throws on a duplicate (profile, scene tag), so a caller inside a
    // transaction it wants to keep checks GetForVenueProfile first.
    int64_t Assign(Transaction& transaction, int64_t venueProfileId, int64_t sceneTagId);

    // DELETE — removes one (profile, scene tag) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t venueProfileId, int64_t sceneTagId);

    // READ — one profile's scene tags in the tags' display order.
    KeyValueTableArray GetForVenueProfile(Transaction& transaction, int64_t venueProfileId) const;

    // READ — many profiles' scene tags in one query, ordered by profile and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForVenueProfiles(
        Transaction& transaction, const std::vector<int64_t>& venueProfileIds) const;

    // READ — the ids of the profiles that carry the scene tag, ascending.
    std::vector<int64_t> GetVenueProfileIdsForSceneTag(
        Transaction& transaction, int64_t sceneTagId) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
