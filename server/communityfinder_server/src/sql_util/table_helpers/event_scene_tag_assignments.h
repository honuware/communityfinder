#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined scene tag's slug on every GetFor* row.
inline constexpr std::string_view kEventSceneTagAssignmentsSceneTagSlug = "scene_tag_slug";

// EV1 Slice 3 — CRUD for event_scene_tag_assignments, an event's own scene tags.
// An event with none inherits its series' (the OQ5 inheritance pin, resolved in
// EventHelper). Reads join scene_tags for the slug, because every caller — the
// public payload and the scene filter — works in slugs.
class EventSceneTagAssignments {
public:
    explicit EventSceneTagAssignments(DatabaseHelper databaseHelper);
    EventSceneTagAssignments(const EventSceneTagAssignments&) = default;
    EventSceneTagAssignments& operator=(const EventSceneTagAssignments&) = default;
    ~EventSceneTagAssignments() = default;

    // CREATE — throws on a duplicate (event, scene tag), so a caller inside a
    // transaction it wants to keep checks GetForEvent first.
    int64_t Assign(Transaction& transaction, int64_t eventId, int64_t sceneTagId);

    // DELETE — removes one (event, scene tag) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t eventId, int64_t sceneTagId);

    // READ — one event's scene tags in the tags' display order.
    KeyValueTableArray GetForEvent(Transaction& transaction, int64_t eventId) const;

    // READ — many events' scene tags in one query, ordered by event and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForEvents(
        Transaction& transaction, const std::vector<int64_t>& eventIds) const;

    // READ — the ids of the events that carry the scene tag themselves
    // (inherited assignments are not rows here), ascending.
    std::vector<int64_t> GetEventIdsForSceneTag(
        Transaction& transaction, int64_t sceneTagId) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
