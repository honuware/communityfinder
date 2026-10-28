#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined category's slug on every GetFor* row.
inline constexpr std::string_view kEventCategoryAssignmentsCategorySlug = "category_slug";

// EV1 Slice 3 — CRUD for event_category_assignments, an event's own categories.
// An event with none inherits its series' (the OQ5 inheritance pin, resolved in
// EventHelper). Reads join categories for the slug, because every caller — the
// public payload and the category filter — works in slugs.
class EventCategoryAssignments {
public:
    explicit EventCategoryAssignments(DatabaseHelper databaseHelper);
    EventCategoryAssignments(const EventCategoryAssignments&) = default;
    EventCategoryAssignments& operator=(const EventCategoryAssignments&) = default;
    ~EventCategoryAssignments() = default;

    // CREATE — throws on a duplicate (event, category), so a caller inside a
    // transaction it wants to keep checks GetForEvent first.
    int64_t Assign(Transaction& transaction, int64_t eventId, int64_t categoryId);

    // DELETE — removes one (event, category) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t eventId, int64_t categoryId);

    // READ — one event's categories in the categories' display order.
    KeyValueTableArray GetForEvent(Transaction& transaction, int64_t eventId) const;

    // READ — many events' categories in one query, ordered by event and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForEvents(
        Transaction& transaction, const std::vector<int64_t>& eventIds) const;

    // READ — the ids of the events that carry the category themselves
    // (inherited assignments are not rows here), ascending.
    std::vector<int64_t> GetEventIdsForCategory(
        Transaction& transaction, int64_t categoryId) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
