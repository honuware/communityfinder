#pragma once

#include <cstdint>
#include <string_view>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// EV1 Slice 3 — CRUD for scene_tags, the shared "whose scene is this" vocabulary
// (GD1 §2). Seeded, edited in the admin CRUD editor; AddSceneTag is for tests and
// test_helper commands.
class SceneTags {
public:
    explicit SceneTags(DatabaseHelper databaseHelper);
    SceneTags(const SceneTags&) = default;
    SceneTags& operator=(const SceneTags&) = default;
    ~SceneTags() = default;

    // CREATE — throws on a duplicate slug.
    int64_t AddSceneTag(Transaction& transaction, const KeyValueTable& values);

    // READ — the enabled scene tags in display order (sort_order, then id).
    KeyValueTableArray GetAll(Transaction& transaction) const;

    // READ — one scene tag by slug, enabled or not; empty when there is none.
    KeyValueTable GetBySlug(Transaction& transaction, std::string_view slug) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
