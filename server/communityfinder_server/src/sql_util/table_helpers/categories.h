#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for categories, the shared "what kind of thing" vocabulary
// (GD1 §1). The seed fills it; curators edit it in the admin CRUD editor, so the
// only write here is AddCategory, for tests and test_helper commands.
class Categories {
public:
    explicit Categories(DatabaseHelper databaseHelper);
    Categories(const Categories&) = default;
    Categories& operator=(const Categories&) = default;
    ~Categories() = default;

    // CREATE — throws on a duplicate slug.
    int64_t AddCategory(Transaction& transaction, const KeyValueTable& values);

    // READ — the enabled categories in display order (sort_order, then id).
    KeyValueTableArray GetAll(Transaction& transaction) const;

    // READ — one category by slug, enabled or not; empty when there is none.
    KeyValueTable GetBySlug(Transaction& transaction, std::string_view slug) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
