#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for neighborhoods, the shared "where" vocabulary (GD1 §3).
// Seeded, edited in the admin CRUD editor; AddNeighborhood is for tests and
// test_helper commands.
class Neighborhoods {
public:
    explicit Neighborhoods(DatabaseHelper databaseHelper);
    Neighborhoods(const Neighborhoods&) = default;
    Neighborhoods& operator=(const Neighborhoods&) = default;
    ~Neighborhoods() = default;

    // CREATE — throws on a duplicate slug.
    int64_t AddNeighborhood(Transaction& transaction, const KeyValueTable& values);

    // READ — the enabled neighborhoods in display order (sort_order, then id).
    KeyValueTableArray GetAll(Transaction& transaction) const;

    // READ — one neighborhood by slug, enabled or not; empty when there is none.
    KeyValueTable GetBySlug(Transaction& transaction, std::string_view slug) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
