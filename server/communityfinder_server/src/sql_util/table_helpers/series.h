#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for series, the durable label + page grouping a recurring
// event's dated instances ("T4T, 2nd Saturdays").
class Series {
public:
    explicit Series(DatabaseHelper databaseHelper);
    Series(const Series&) = default;
    Series& operator=(const Series&) = default;
    ~Series() = default;

    // CREATE — throws on a duplicate slug.
    int64_t AddSeries(Transaction& transaction, const KeyValueTable& values);

    // READ — empty when there is no such series.
    KeyValueTable GetSeries(Transaction& transaction, int64_t id) const;

    // READ — by slug (the public page's URL key); empty when there is none.
    KeyValueTable GetSeriesBySlug(Transaction& transaction, std::string_view slug) const;

    // READ — the series index: every series still running, ordered by title
    // then id. Active = any status but `closed` and `moved` — so `seasonal`
    // (dormant off-season) and `unverified` (listed, not yet checked; GD1 §5)
    // stay listed. A closed series keeps its page; it just leaves the index.
    KeyValueTableArray GetActiveSeries(Transaction& transaction) const;

    // UPDATE — bumps updated_us. A value of "NULL" clears a nullable column.
    void UpdateSeries(Transaction& transaction, int64_t id, const KeyValueTable& updates);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
