#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined category's slug on every GetFor* row.
inline constexpr std::string_view kSeriesCategoryAssignmentsCategorySlug = "category_slug";

// EV1 Slice 3 — CRUD for series_category_assignments, a series' categories
// (decided OQ5). Its instances inherit them unless they carry their own;
// EventHelper resolves that with GetForSeriesIds.
class SeriesCategoryAssignments {
public:
    explicit SeriesCategoryAssignments(DatabaseHelper databaseHelper);
    SeriesCategoryAssignments(const SeriesCategoryAssignments&) = default;
    SeriesCategoryAssignments& operator=(const SeriesCategoryAssignments&) = default;
    ~SeriesCategoryAssignments() = default;

    // CREATE — throws on a duplicate (series, category), so a caller inside a
    // transaction it wants to keep checks GetForSeries first.
    int64_t Assign(Transaction& transaction, int64_t seriesId, int64_t categoryId);

    // DELETE — removes one (series, category) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t seriesId, int64_t categoryId);

    // READ — one series' categories in the categories' display order.
    KeyValueTableArray GetForSeries(Transaction& transaction, int64_t seriesId) const;

    // READ — many series' categories in one query, ordered by series and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForSeriesIds(
        Transaction& transaction, const std::vector<int64_t>& seriesIds) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
