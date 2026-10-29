#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// The joined scene tag's slug on every GetFor* row.
inline constexpr std::string_view kOrganizationSceneTagAssignmentsSceneTagSlug = "scene_tag_slug";

// GD2 §2.5 — CRUD for organization_scene_tag_assignments: the scenes an
// organization belongs to. Reads join scene_tags for the slug.
class OrganizationSceneTagAssignments {
public:
    explicit OrganizationSceneTagAssignments(DatabaseHelper databaseHelper);
    OrganizationSceneTagAssignments(const OrganizationSceneTagAssignments&) = default;
    OrganizationSceneTagAssignments& operator=(const OrganizationSceneTagAssignments&) = default;
    ~OrganizationSceneTagAssignments() = default;

    // CREATE — throws on a duplicate (organization, scene tag), so a caller
    // inside a transaction it wants to keep checks GetForOrganization first.
    int64_t Assign(Transaction& transaction, int64_t organizationId, int64_t sceneTagId);

    // DELETE — removes one (organization, scene tag) row; nothing happens when there is none.
    void Remove(Transaction& transaction, int64_t organizationId, int64_t sceneTagId);

    // READ — one organization's scene tags in the tags' display order.
    KeyValueTableArray GetForOrganization(Transaction& transaction, int64_t organizationId) const;

    // READ — many organizations' scene tags in one query, ordered by
    // organization and then display order. Empty input, empty result.
    KeyValueTableArray GetForOrganizations(
        Transaction& transaction, const std::vector<int64_t>& organizationIds) const;

    // READ — the ids of the organizations that carry the scene tag, ascending.
    std::vector<int64_t> GetOrganizationIdsForSceneTag(
        Transaction& transaction, int64_t sceneTagId) const;

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
