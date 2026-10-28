#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined scene tag's slug on every GetFor* row.
inline constexpr std::string_view kSeriesSceneTagAssignmentsSceneTagSlug = "scene_tag_slug";

// EV1 Slice 3 — CRUD for series_scene_tag_assignments, a series' scene tags
// (decided OQ5). Its instances inherit them unless they carry their own;
// EventHelper resolves that with GetForSeriesIds.
class SeriesSceneTagAssignments {
public:
    explicit SeriesSceneTagAssignments(DatabaseHelper databaseHelper);
    SeriesSceneTagAssignments(const SeriesSceneTagAssignments&) = default;
    SeriesSceneTagAssignments& operator=(const SeriesSceneTagAssignments&) = default;
    ~SeriesSceneTagAssignments() = default;

    // CREATE — throws on a duplicate (series, scene tag), so a caller inside a
    // transaction it wants to keep checks GetForSeries first.
    int64_t Assign(Transaction& transaction, int64_t seriesId, int64_t sceneTagId);

    // DELETE — removes one (series, scene tag) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t seriesId, int64_t sceneTagId);

    // READ — one series' scene tags in the tags' display order.
    KeyValueTableArray GetForSeries(Transaction& transaction, int64_t seriesId) const;

    // READ — many series' scene tags in one query, ordered by series and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForSeriesIds(
        Transaction& transaction, const std::vector<int64_t>& seriesIds) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
