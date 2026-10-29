#include "guide_links.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/guide_links.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "organizations.h"
#include "places.h"
#include "scene_tags.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"
#include "venue_profiles.h"
#include "venues.h"

namespace TableHelpers {
namespace {

// One parent of each kind.
struct Parents {
    int64_t venueProfileId = 0;
    int64_t organizationId = 0;
    int64_t placeId = 0;
    int64_t sceneTagId = 0;
};

Parents AddParents(Transaction& transaction, DatabaseHelper databaseHelper) {
    Parents parents;
    Venues venues(databaseHelper);
    int64_t venueId = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "The Cuff" } });
    parents.venueProfileId = VenueProfiles(databaseHelper).AddProfile(transaction, {
        { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(venueId) },
        { std::string(DbSchema::kVenueProfilesSlug), "the-cuff" },
    });
    parents.organizationId = Organizations(databaseHelper).AddOrganization(transaction, {
        { std::string(DbSchema::kOrganizationsName), "League" },
        { std::string(DbSchema::kOrganizationsSlug), "league" },
        { std::string(DbSchema::kOrganizationsOrgKind),
          std::string(DbSchema::kOrganizationsOrgKindSportsLeague) },
    });
    parents.placeId = Places(databaseHelper).AddPlace(transaction, {
        { std::string(DbSchema::kPlacesName), "Beach" },
        { std::string(DbSchema::kPlacesSlug), "beach" },
        { std::string(DbSchema::kPlacesPlaceKind),
          std::string(DbSchema::kPlacesPlaceKindParkBeach) },
    });
    parents.sceneTagId = SceneTags(databaseHelper).AddSceneTag(transaction, {
        { std::string(DbSchema::kSceneTagsSlug), "bears" },
        { std::string(DbSchema::kSceneTagsDisplayName), "Bears" },
    });
    return parents;
}

KeyValueTable MakeLink(std::string_view label, std::string_view parentColumn, int64_t parentId) {
    return {
        { std::string(DbSchema::kGuideLinksLabel), std::string(label) },
        { std::string(DbSchema::kGuideLinksUrl), "https://example.com/" + std::string(label) },
        { std::string(DbSchema::kGuideLinksPlatform),
          std::string(DbSchema::kGuideLinksPlatformDiscord) },
        { std::string(parentColumn), StringFromInt(parentId) },
    };
}

std::vector<std::string> Labels(const KeyValueTableArray& rows) {
    std::vector<std::string> labels;
    for (const KeyValueTable& row : rows) {
        labels.push_back(row.at(std::string(DbSchema::kGuideLinksLabel)));
    }
    return labels;
}

TEST(GuideLinksTest, EachParentGetsOnlyItsOwnLinks) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachParentGetsOnlyItsOwnLinks", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideLinks links(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);

        EXPECT_GT(links.AddLink(transaction, MakeLink(
            "profile-link", DbSchema::kGuideLinksVenueProfileId, parents.venueProfileId)), 0);
        links.AddLink(transaction, MakeLink(
            "org-link", DbSchema::kGuideLinksOrganizationId, parents.organizationId));
        links.AddLink(transaction, MakeLink(
            "place-link", DbSchema::kGuideLinksPlaceId, parents.placeId));
        links.AddLink(transaction, MakeLink(
            "scene-link", DbSchema::kGuideLinksSceneTagId, parents.sceneTagId));

        EXPECT_EQ(Labels(links.GetLinksForVenueProfile(transaction, parents.venueProfileId)),
                  (std::vector<std::string>{ "profile-link" }));
        EXPECT_EQ(Labels(links.GetLinksForOrganization(transaction, parents.organizationId)),
                  (std::vector<std::string>{ "org-link" }));
        EXPECT_EQ(Labels(links.GetLinksForPlace(transaction, parents.placeId)),
                  (std::vector<std::string>{ "place-link" }));
        EXPECT_EQ(Labels(links.GetLinksForSceneTag(transaction, parents.sceneTagId)),
                  (std::vector<std::string>{ "scene-link" }));
    });
}

TEST(GuideLinksTest, LinksListBySortOrderThenId) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("LinksListBySortOrderThenId", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideLinks links(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        auto addWithOrder = [&](std::string_view label, std::string_view sortOrder) {
            KeyValueTable values =
                MakeLink(label, DbSchema::kGuideLinksSceneTagId, parents.sceneTagId);
            if (!sortOrder.empty()) {
                values[std::string(DbSchema::kGuideLinksSortOrder)] = std::string(sortOrder);
            }
            links.AddLink(transaction, values);
        };
        addWithOrder("third", "20");
        addWithOrder("first", "");  // sort_order defaults to 0
        addWithOrder("second", "10");
        addWithOrder("fourth", "20");

        EXPECT_EQ(Labels(links.GetLinksForSceneTag(transaction, parents.sceneTagId)),
                  (std::vector<std::string>{ "first", "second", "third", "fourth" }));
    });
}

TEST(GuideLinksTest, DeleteRemovesOnlyThatLink) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DeleteRemovesOnlyThatLink", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideLinks links(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        int64_t doomed = links.AddLink(transaction, MakeLink(
            "doomed", DbSchema::kGuideLinksOrganizationId, parents.organizationId));
        links.AddLink(transaction, MakeLink(
            "kept", DbSchema::kGuideLinksOrganizationId, parents.organizationId));

        links.DeleteLink(transaction, doomed);
        // Deleting a link that isn't there is a no-op.
        links.DeleteLink(transaction, doomed);

        EXPECT_EQ(Labels(links.GetLinksForOrganization(transaction, parents.organizationId)),
                  (std::vector<std::string>{ "kept" }));
    });
}

// guide_links_exactly_one_parent: a link with no parent would show nowhere.
TEST(GuideLinksTest, NoParentThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NoParentThrows", [&](Transaction& transaction) {
        GuideLinks links(testDb.GetDatabaseHelper());
        KeyValueTable values = {
            { std::string(DbSchema::kGuideLinksLabel), "orphan" },
            { std::string(DbSchema::kGuideLinksUrl), "https://example.com/orphan" },
            { std::string(DbSchema::kGuideLinksPlatform),
              std::string(DbSchema::kGuideLinksPlatformWebsite) },
        };
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(links.AddLink(transaction, values), std::exception);
    });
}

// guide_links_exactly_one_parent: a link belongs to one page, never two.
TEST(GuideLinksTest, TwoParentsThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TwoParentsThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideLinks links(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakeLink("both", DbSchema::kGuideLinksPlaceId, parents.placeId);
        values[std::string(DbSchema::kGuideLinksSceneTagId)] = StringFromInt(parents.sceneTagId);
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(links.AddLink(transaction, values), std::exception);
    });
}

TEST(GuideLinksTest, PlatformOutsideGd1Throws) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PlatformOutsideGd1Throws", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideLinks links(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakeLink("tiktok", DbSchema::kGuideLinksSceneTagId, parents.sceneTagId);
        values[std::string(DbSchema::kGuideLinksPlatform)] = "tiktok";
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(links.AddLink(transaction, values), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
