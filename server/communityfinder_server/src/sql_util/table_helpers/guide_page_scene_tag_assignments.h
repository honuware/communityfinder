#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined scene tag's slug on every GetFor* row.
inline constexpr std::string_view kGuidePageSceneTagAssignmentsSceneTagSlug = "scene_tag_slug";

// GD2 §2.5 — CRUD for guide_page_scene_tag_assignments: the scenes a guide
// page is about. Reads join scene_tags for the slug. A page's tags say nothing
// about whether it is published; callers filter pages through GuidePages.
class GuidePageSceneTagAssignments {
public:
    explicit GuidePageSceneTagAssignments(DatabaseHelper databaseHelper);
    GuidePageSceneTagAssignments(const GuidePageSceneTagAssignments&) = default;
    GuidePageSceneTagAssignments& operator=(const GuidePageSceneTagAssignments&) = default;
    ~GuidePageSceneTagAssignments() = default;

    // CREATE — throws on a duplicate (page, scene tag), so a caller inside a
    // transaction it wants to keep checks GetForGuidePage first.
    int64_t Assign(Transaction& transaction, int64_t guidePageId, int64_t sceneTagId);

    // DELETE — removes one (page, scene tag) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t guidePageId, int64_t sceneTagId);

    // READ — one page's scene tags in the tags' display order.
    KeyValueTableArray GetForGuidePage(Transaction& transaction, int64_t guidePageId) const;

    // READ — many pages' scene tags in one query, ordered by page and then
    // display order. Empty input, empty result.
    KeyValueTableArray GetForGuidePages(
        Transaction& transaction, const std::vector<int64_t>& guidePageIds) const;

    // READ — the ids of the pages that carry the scene tag, ascending, in any
    // publish state.
    std::vector<int64_t> GetGuidePageIdsForSceneTag(
        Transaction& transaction, int64_t sceneTagId) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
