#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined scene tag's slug on every GetFor* row.
inline constexpr std::string_view kPlaceSceneTagAssignmentsSceneTagSlug = "scene_tag_slug";

// GD2 §2.5 — CRUD for place_scene_tag_assignments: the scenes a place matters
// to. Reads join scene_tags for the slug.
class PlaceSceneTagAssignments {
public:
    explicit PlaceSceneTagAssignments(DatabaseHelper databaseHelper);
    PlaceSceneTagAssignments(const PlaceSceneTagAssignments&) = default;
    PlaceSceneTagAssignments& operator=(const PlaceSceneTagAssignments&) = default;
    ~PlaceSceneTagAssignments() = default;

    // CREATE — throws on a duplicate (place, scene tag), so a caller inside a
    // transaction it wants to keep checks GetForPlace first.
    int64_t Assign(Transaction& transaction, int64_t placeId, int64_t sceneTagId);

    // DELETE — removes one (place, scene tag) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t placeId, int64_t sceneTagId);

    // READ — one place's scene tags in the tags' display order.
    KeyValueTableArray GetForPlace(Transaction& transaction, int64_t placeId) const;

    // READ — many places' scene tags in one query, ordered by place and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForPlaces(
        Transaction& transaction, const std::vector<int64_t>& placeIds) const;

    // READ — the ids of the places that carry the scene tag, ascending.
    std::vector<int64_t> GetPlaceIdsForSceneTag(
        Transaction& transaction, int64_t sceneTagId) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
