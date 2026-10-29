#pragma once

#include <cstdint>

#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_common.h"

namespace TableHelpers {

// GD2 §2.6 — CRUD for guide_links, "where this scene talks". Each link has
// exactly one parent among a venue profile, an organization, a place, and a
// scene tag; the database rejects none or several.
class GuideLinks {
public:
    explicit GuideLinks(DatabaseHelper databaseHelper);
    GuideLinks(const GuideLinks&) = default;
    GuideLinks& operator=(const GuideLinks&) = default;
    ~GuideLinks() = default;

    // CREATE — `values` sets exactly one of venue_profile_id, organization_id,
    // place_id, scene_tag_id. Throws when it sets none or several, or a
    // platform outside GD1's list.
    int64_t AddLink(Transaction& transaction, const KeyValueTable& values);

    // READ — one parent's links in display order: sort_order, then id.
    KeyValueTableArray GetLinksForVenueProfile(
        Transaction& transaction, int64_t venueProfileId) const;
    KeyValueTableArray GetLinksForOrganization(
        Transaction& transaction, int64_t organizationId) const;
    KeyValueTableArray GetLinksForPlace(Transaction& transaction, int64_t placeId) const;
    KeyValueTableArray GetLinksForSceneTag(Transaction& transaction, int64_t sceneTagId) const;

    // DELETE — nothing happens when there is no such link.
    void DeleteLink(Transaction& transaction, int64_t id);

private:
    DatabaseHelper databaseHelper_;
};

}  // namespace TableHelpers
