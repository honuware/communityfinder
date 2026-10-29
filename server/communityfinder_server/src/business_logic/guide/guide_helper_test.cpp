#include "business_logic/guide/guide_helper.h"

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/freshness.h"
#include "db_schema/guide_links.h"
#include "db_schema/guide_pages.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace Guide {
namespace {

constexpr int64_t kUsPerDay = 86'400LL * 1'000'000LL;
constexpr int64_t kEarlierUs = 1790000000000000LL;
constexpr int64_t kLaterUs = 1793000000000000LL;

constexpr std::string_view kOpen = DbSchema::kFreshnessStatusOpen;
constexpr std::string_view kClosed = DbSchema::kFreshnessStatusClosed;
constexpr std::string_view kMoved = DbSchema::kFreshnessStatusMoved;
constexpr std::string_view kSeasonal = DbSchema::kFreshnessStatusSeasonal;
constexpr std::string_view kUnverified = DbSchema::kFreshnessStatusUnverified;

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

enum class PageState {
    Published,    // not a draft, published yesterday
    Scheduled,    // not a draft, publishes tomorrow
    Draft,        // a draft, though its publish time has passed
    Unscheduled,  // not a draft, but no publish time
};

struct VenueIds {
    int64_t venueId = 0;
    int64_t profileId = 0;
};

// Adds the rows a test needs, one call per row, through the same table
// helpers GuideHelper reads with. A neighborhoodId of 0 = none.
struct GuideRows {
    Transaction& transaction;
    DatabaseHelper databaseHelper;

    int64_t Neighborhood(std::string_view slug, bool enabled = true) {
        return TableHelpers::Neighborhoods(databaseHelper).AddNeighborhood(transaction, {
            { std::string(DbSchema::kNeighborhoodsSlug), std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsDisplayName), "Name of " + std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsRegion),
              std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
            { std::string(DbSchema::kNeighborhoodsEnabled), enabled ? "true" : "false" },
        });
    }

    int64_t SceneTag(std::string_view slug, int64_t sortOrder, bool enabled = true,
                     std::string_view coverage = DbSchema::kSceneTagsCoverageFull) {
        return TableHelpers::SceneTags(databaseHelper).AddSceneTag(transaction, {
            { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
            { std::string(DbSchema::kSceneTagsDisplayName), "Name of " + std::string(slug) },
            { std::string(DbSchema::kSceneTagsDescription), "About " + std::string(slug) },
            { std::string(DbSchema::kSceneTagsCoverage), std::string(coverage) },
            { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
            { std::string(DbSchema::kSceneTagsEnabled), enabled ? "true" : "false" },
        });
    }

    // A venue with a profile: a guide venue.
    VenueIds Venue(std::string_view name, std::string_view slug, int64_t neighborhoodId,
                   std::string_view status, const std::vector<int64_t>& sceneTagIds) {
        KeyValueTable venue = {
            { std::string(DbSchema::kVenuesName), std::string(name) },
            { std::string(DbSchema::kVenuesStatus), std::string(status) },
        };
        if (neighborhoodId > 0) {
            venue[std::string(DbSchema::kVenuesNeighborhoodId)] = StringFromInt(neighborhoodId);
        }
        VenueIds ids;
        ids.venueId = TableHelpers::Venues(databaseHelper).AddVenue(transaction, venue);
        ids.profileId = TableHelpers::VenueProfiles(databaseHelper).AddProfile(transaction, {
            { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(ids.venueId) },
            { std::string(DbSchema::kVenueProfilesSlug), std::string(slug) },
        });
        TableHelpers::VenueProfileSceneTagAssignments tags(databaseHelper);
        for (int64_t sceneTagId : sceneTagIds) {
            tags.Assign(transaction, ids.profileId, sceneTagId);
        }
        return ids;
    }

    int64_t Organization(std::string_view name, std::string_view slug, std::string_view orgKind,
                         int64_t neighborhoodId, std::string_view status,
                         const std::vector<int64_t>& sceneTagIds) {
        KeyValueTable values = {
            { std::string(DbSchema::kOrganizationsName), std::string(name) },
            { std::string(DbSchema::kOrganizationsSlug), std::string(slug) },
            { std::string(DbSchema::kOrganizationsOrgKind), std::string(orgKind) },
            { std::string(DbSchema::kOrganizationsStatus), std::string(status) },
        };
        if (neighborhoodId > 0) {
            values[std::string(DbSchema::kOrganizationsNeighborhoodId)] =
                StringFromInt(neighborhoodId);
        }
        int64_t id = TableHelpers::Organizations(databaseHelper).AddOrganization(transaction, values);
        TableHelpers::OrganizationSceneTagAssignments tags(databaseHelper);
        for (int64_t sceneTagId : sceneTagIds) {
            tags.Assign(transaction, id, sceneTagId);
        }
        return id;
    }

    int64_t Place(std::string_view name, std::string_view slug, int64_t neighborhoodId,
                  std::string_view status, const std::vector<int64_t>& sceneTagIds) {
        KeyValueTable values = {
            { std::string(DbSchema::kPlacesName), std::string(name) },
            { std::string(DbSchema::kPlacesSlug), std::string(slug) },
            { std::string(DbSchema::kPlacesPlaceKind),
              std::string(DbSchema::kPlacesPlaceKindParkBeach) },
            { std::string(DbSchema::kPlacesStatus), std::string(status) },
        };
        if (neighborhoodId > 0) {
            values[std::string(DbSchema::kPlacesNeighborhoodId)] = StringFromInt(neighborhoodId);
        }
        int64_t id = TableHelpers::Places(databaseHelper).AddPlace(transaction, values);
        TableHelpers::PlaceSceneTagAssignments tags(databaseHelper);
        for (int64_t sceneTagId : sceneTagIds) {
            tags.Assign(transaction, id, sceneTagId);
        }
        return id;
    }

    int64_t Service(std::string_view name, std::string_view slug, std::string_view serviceKind,
                    std::string_view status, bool isHealth) {
        return TableHelpers::ServiceListings(databaseHelper).AddServiceListing(transaction, {
            { std::string(DbSchema::kServiceListingsName), std::string(name) },
            { std::string(DbSchema::kServiceListingsSlug), std::string(slug) },
            { std::string(DbSchema::kServiceListingsServiceKind), std::string(serviceKind) },
            { std::string(DbSchema::kServiceListingsStatus), std::string(status) },
            { std::string(DbSchema::kServiceListingsIsHealth), isHealth ? "true" : "false" },
        });
    }

    // The title defaults to the slug.
    int64_t Page(std::string_view slug, std::string_view section, PageState state,
                 std::string_view title = "") {
        const int64_t nowUs = NowUs(transaction);
        KeyValueTable values = {
            { std::string(DbSchema::kGuidePagesTitle),
              std::string(title.empty() ? slug : title) },
            { std::string(DbSchema::kGuidePagesSlug), std::string(slug) },
            { std::string(DbSchema::kGuidePagesSection), std::string(section) },
            { std::string(DbSchema::kGuidePagesDraft), state == PageState::Draft ? "true" : "false" },
        };
        if (state == PageState::Published || state == PageState::Draft) {
            values[std::string(DbSchema::kGuidePagesPublishAtUs)] = StringFromInt(nowUs - kUsPerDay);
        } else if (state == PageState::Scheduled) {
            values[std::string(DbSchema::kGuidePagesPublishAtUs)] = StringFromInt(nowUs + kUsPerDay);
        }
        return TableHelpers::GuidePages(databaseHelper).AddPage(transaction, values);
    }

    // `parentColumn` is one of guide_links' four parent FKs.
    void Link(std::string_view label, int64_t sortOrder, std::string_view parentColumn,
              int64_t parentId) {
        TableHelpers::GuideLinks(databaseHelper).AddLink(transaction, {
            { std::string(DbSchema::kGuideLinksLabel), std::string(label) },
            { std::string(DbSchema::kGuideLinksUrl), "https://example.org/" + std::string(label) },
            { std::string(DbSchema::kGuideLinksPlatform),
              std::string(DbSchema::kGuideLinksPlatformDiscord) },
            { std::string(DbSchema::kGuideLinksSortOrder), StringFromInt(sortOrder) },
            { std::string(parentColumn), StringFromInt(parentId) },
        });
    }
};

template <class Item>
std::vector<std::string> Names(const std::vector<Item>& items) {
    std::vector<std::string> names;
    for (const Item& item : items) {
        names.push_back(item.name);
    }
    return names;
}

template <class Item>
const Item* FindByName(const std::vector<Item>& items, std::string_view name) {
    auto it = std::find_if(items.begin(), items.end(),
                           [&](const Item& item) { return item.name == name; });
    return it == items.end() ? nullptr : &*it;
}

using Strings = std::vector<std::string>;

// --- The free functions ---

TEST(GuideHelperTest, IsPublishedNeedsNoDraftAndAnArrivedPublishTime) {
    const int64_t nowUs = kLaterUs;
    EXPECT_TRUE(IsPublished(false, nowUs - 1, nowUs));
    EXPECT_TRUE(IsPublished(false, nowUs, nowUs));
    EXPECT_FALSE(IsPublished(false, nowUs + 1, nowUs));
    EXPECT_FALSE(IsPublished(false, std::nullopt, nowUs));
    EXPECT_FALSE(IsPublished(true, nowUs - 1, nowUs));
    EXPECT_FALSE(IsPublished(true, std::nullopt, nowUs));
}

TEST(GuideHelperTest, OlderVerifiedAtUsIsTheOlderOrNone) {
    EXPECT_EQ(OlderVerifiedAtUs(kEarlierUs, kLaterUs), std::optional<int64_t>(kEarlierUs));
    EXPECT_EQ(OlderVerifiedAtUs(kLaterUs, kEarlierUs), std::optional<int64_t>(kEarlierUs));
    EXPECT_FALSE(OlderVerifiedAtUs(std::nullopt, kLaterUs).has_value());
    EXPECT_FALSE(OlderVerifiedAtUs(kEarlierUs, std::nullopt).has_value());
    EXPECT_FALSE(OlderVerifiedAtUs(std::nullopt, std::nullopt).has_value());
}

// --- Venues ---

TEST(GuideHelperTest, VenuePageJoinsTheVenueRowAndItsProfile) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("VenuePageJoinsTheVenueRowAndItsProfile", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        int64_t leather = rows.SceneTag("leather", 20);
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t hidden = rows.SceneTag("hidden", 30, false);

        int64_t venueId = TableHelpers::Venues(databaseHelper).AddVenue(transaction, {
            { std::string(DbSchema::kVenuesName), "The Cuff" },
            { std::string(DbSchema::kVenuesDescription), "The venue's description." },
            { std::string(DbSchema::kVenuesAddress), "1533 13th Ave" },
            { std::string(DbSchema::kVenuesCity), "Seattle" },
            { std::string(DbSchema::kVenuesState), "WA" },
            { std::string(DbSchema::kVenuesZip), "98122" },
            { std::string(DbSchema::kVenuesUrl), "https://example.org/cuff" },
            { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(capitolHill) },
            { std::string(DbSchema::kVenuesLastVerifiedAtUs), StringFromInt(kLaterUs) },
        });
        int64_t profileId = TableHelpers::VenueProfiles(databaseHelper).AddProfile(transaction, {
            { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(venueId) },
            { std::string(DbSchema::kVenueProfilesSlug), "the-cuff" },
            { std::string(DbSchema::kVenueProfilesCrowdVibe), "The profile's crowd and vibe." },
            { std::string(DbSchema::kVenueProfilesPriceBand), "$$" },
            { std::string(DbSchema::kVenueProfilesAdult), "true" },
            { std::string(DbSchema::kVenueProfilesLastVerifiedAtUs), StringFromInt(kEarlierUs) },
        });
        TableHelpers::VenueProfileSceneTagAssignments tags(databaseHelper);
        tags.Assign(transaction, profileId, leather);
        tags.Assign(transaction, profileId, hidden);
        tags.Assign(transaction, profileId, bears);
        rows.Link("second", 20, DbSchema::kGuideLinksVenueProfileId, profileId);
        rows.Link("first", 10, DbSchema::kGuideLinksVenueProfileId, profileId);

        GuideHelper guide(databaseHelper);
        std::optional<VenueDetail> detail = guide.GetVenueProfileBySlug(transaction, "the-cuff");
        ASSERT_TRUE(detail.has_value());
        const GuideVenue& venue = detail->venue;
        EXPECT_EQ(venue.venueId, venueId);
        EXPECT_EQ(venue.profileId, profileId);
        EXPECT_EQ(venue.slug, "the-cuff");
        // The venue row's facts...
        EXPECT_EQ(venue.name, "The Cuff");
        EXPECT_EQ(venue.description, "The venue's description.");
        EXPECT_EQ(venue.address, "1533 13th Ave, Seattle, WA 98122");
        EXPECT_EQ(venue.url, "https://example.org/cuff");
        EXPECT_EQ(venue.neighborhoodSlug, "capitol-hill");
        EXPECT_EQ(venue.freshness.status, kOpen);
        // ...and the profile's.
        EXPECT_EQ(venue.crowdVibe, "The profile's crowd and vibe.");
        EXPECT_EQ(venue.priceBand, "$$");
        EXPECT_TRUE(venue.adult);
        EXPECT_EQ(venue.closureNote, "");
        // In display order, and the disabled tag left out.
        EXPECT_EQ(venue.sceneTagSlugs, (Strings{ "bears", "leather" }));
        // The stamp is the older check.
        EXPECT_EQ(venue.venueLastVerifiedAtUs, std::optional<int64_t>(kLaterUs));
        EXPECT_EQ(venue.profileLastVerifiedAtUs, std::optional<int64_t>(kEarlierUs));
        EXPECT_EQ(venue.freshness.lastVerifiedAtUs, std::optional<int64_t>(kEarlierUs));

        ASSERT_EQ(detail->links.size(), 2u);
        EXPECT_EQ(detail->links[0].label, "first");
        EXPECT_EQ(detail->links[0].url, "https://example.org/first");
        EXPECT_EQ(detail->links[0].platform, DbSchema::kGuideLinksPlatformDiscord);
        EXPECT_EQ(detail->links[1].label, "second");

        EXPECT_FALSE(guide.GetVenueProfileBySlug(transaction, "no-such-venue").has_value());
    });
}

TEST(GuideHelperTest, VenueStampIsNeverVerifiedUntilBothSidesAreChecked) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "VenueStampIsNeverVerifiedUntilBothSidesAreChecked", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        TableHelpers::Venues venues(databaseHelper);
        TableHelpers::VenueProfiles profiles(databaseHelper);
        VenueIds venueOnly = rows.Venue("Venue Checked", "venue-checked", 0, kOpen, {});
        venues.UpdateVenue(transaction, venueOnly.venueId, {
            { std::string(DbSchema::kVenuesLastVerifiedAtUs), StringFromInt(kLaterUs) } });
        VenueIds profileOnly = rows.Venue("Profile Checked", "profile-checked", 0, kOpen, {});
        profiles.UpdateProfile(transaction, profileOnly.profileId, {
            { std::string(DbSchema::kVenueProfilesLastVerifiedAtUs), StringFromInt(kLaterUs) } });

        std::vector<GuideVenue> directory =
            GuideHelper(databaseHelper).GetVenuesDirectory(transaction, VenueFilters());
        const GuideVenue* first = FindByName(directory, "Venue Checked");
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->venueLastVerifiedAtUs, std::optional<int64_t>(kLaterUs));
        EXPECT_FALSE(first->profileLastVerifiedAtUs.has_value());
        EXPECT_FALSE(first->freshness.lastVerifiedAtUs.has_value());
        const GuideVenue* second = FindByName(directory, "Profile Checked");
        ASSERT_NE(second, nullptr);
        EXPECT_FALSE(second->venueLastVerifiedAtUs.has_value());
        EXPECT_EQ(second->profileLastVerifiedAtUs, std::optional<int64_t>(kLaterUs));
        EXPECT_FALSE(second->freshness.lastVerifiedAtUs.has_value());
    });
}

TEST(GuideHelperTest, VenuesDirectoryLeavesOutRetiredAndUnprofiledVenues) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "VenuesDirectoryLeavesOutRetiredAndUnprofiledVenues", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        // Slugs sort the other way, so the order below is the names'.
        rows.Venue("Bravo", "a-bravo", 0, kOpen, {});
        rows.Venue("Alpha", "b-alpha", 0, kSeasonal, {});
        rows.Venue("Charlie", "c-charlie", 0, kUnverified, {});
        rows.Venue("Closed", "closed", 0, kClosed, {});
        rows.Venue("Moved", "moved", 0, kMoved, {});
        // A venue with no profile isn't in the guide yet.
        TableHelpers::Venues(databaseHelper).AddVenue(transaction, {
            { std::string(DbSchema::kVenuesName), "Unprofiled" } });

        GuideHelper guide(databaseHelper);
        EXPECT_EQ(Names(guide.GetVenuesDirectory(transaction, VenueFilters())),
                  (Strings{ "Alpha", "Bravo", "Charlie" }));
        VenueFilters everything;
        everything.includeClosed = true;
        EXPECT_EQ(Names(guide.GetVenuesDirectory(transaction, everything)),
                  (Strings{ "Alpha", "Bravo", "Charlie", "Closed", "Moved" }));
    });
}

TEST(GuideHelperTest, VenuesDirectoryFiltersBySceneAndNeighborhood) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "VenuesDirectoryFiltersBySceneAndNeighborhood", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        int64_t ballard = rows.Neighborhood("ballard");
        int64_t oldHood = rows.Neighborhood("old-hood", false);
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t leather = rows.SceneTag("leather", 20);
        rows.Venue("Hill Bears", "hill-bears", capitolHill, kOpen, { bears });
        rows.Venue("Hill Leather", "hill-leather", capitolHill, kOpen, { leather });
        rows.Venue("Ballard Bears", "ballard-bears", ballard, kOpen, { bears });
        rows.Venue("Old Hood Bears", "old-hood-bears", oldHood, kOpen, { bears });

        GuideHelper guide(databaseHelper);
        auto names = [&](std::string_view sceneSlug, std::string_view neighborhoodSlug) {
            VenueFilters filters;
            filters.sceneSlug = std::string(sceneSlug);
            filters.neighborhoodSlug = std::string(neighborhoodSlug);
            return Names(guide.GetVenuesDirectory(transaction, filters));
        };
        EXPECT_EQ(names("bears", ""), (Strings{ "Ballard Bears", "Hill Bears", "Old Hood Bears" }));
        EXPECT_EQ(names("", "capitol-hill"), (Strings{ "Hill Bears", "Hill Leather" }));
        EXPECT_EQ(names("bears", "capitol-hill"), (Strings{ "Hill Bears" }));
        // A disabled neighborhood or an unknown scene matches nothing.
        EXPECT_TRUE(names("", "old-hood").empty());
        EXPECT_TRUE(names("no-such-scene", "").empty());

        std::optional<VenueDetail> oldHoodBears =
            guide.GetVenueProfileBySlug(transaction, "old-hood-bears");
        ASSERT_TRUE(oldHoodBears.has_value());
        EXPECT_EQ(oldHoodBears->venue.neighborhoodSlug, "");
    });
}

// --- Scenes ---

TEST(GuideHelperTest, SceneOverviewCountsEachEnabledScenesActiveListings) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "SceneOverviewCountsEachEnabledScenesActiveListings", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t leather = rows.SceneTag("leather", 20);
        int64_t bears = rows.SceneTag("bears", 10);
        rows.SceneTag("sober", 30, true, DbSchema::kSceneTagsCoverageLinkOut);
        int64_t hidden = rows.SceneTag("hidden", 40, false);
        rows.Venue("Open Bar", "open-bar", 0, kOpen, { bears, hidden });
        rows.Venue("Closed Bar", "closed-bar", 0, kClosed, { bears });
        rows.Organization("League", "league", DbSchema::kOrganizationsOrgKindSportsLeague, 0,
                          kOpen, { bears, leather });
        rows.Organization("Old Club", "old-club", DbSchema::kOrganizationsOrgKindClubHobby, 0,
                          kClosed, { leather });
        rows.Place("Beach", "beach", 0, kSeasonal, { bears });
        rows.Place("Old Pier", "old-pier", 0, kMoved, { bears });

        std::vector<SceneSummary> scenes = GuideHelper(databaseHelper).GetSceneOverview(transaction);
        ASSERT_EQ(scenes.size(), 3u);  // `hidden` is disabled
        EXPECT_EQ(scenes[0].slug, "bears");
        EXPECT_EQ(scenes[0].displayName, "Name of bears");
        EXPECT_EQ(scenes[0].description, "About bears");
        EXPECT_EQ(scenes[0].coverage, DbSchema::kSceneTagsCoverageFull);
        EXPECT_EQ(scenes[0].venueCount, 1);
        EXPECT_EQ(scenes[0].organizationCount, 1);
        EXPECT_EQ(scenes[0].placeCount, 1);
        EXPECT_EQ(scenes[1].slug, "leather");
        EXPECT_EQ(scenes[1].venueCount, 0);
        EXPECT_EQ(scenes[1].organizationCount, 1);
        EXPECT_EQ(scenes[1].placeCount, 0);
        EXPECT_EQ(scenes[2].slug, "sober");
        EXPECT_EQ(scenes[2].coverage, DbSchema::kSceneTagsCoverageLinkOut);
        EXPECT_EQ(scenes[2].venueCount, 0);
        EXPECT_EQ(scenes[2].organizationCount, 0);
        EXPECT_EQ(scenes[2].placeCount, 0);
    });
}

TEST(GuideHelperTest, SceneDetailListsTheTaggedActiveListingsLinksAndIntro) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "SceneDetailListsTheTaggedActiveListingsLinksAndIntro", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t leather = rows.SceneTag("leather", 20);
        rows.Venue("Tagged Bar", "tagged-bar", 0, kOpen, { bears });
        rows.Venue("Untagged Bar", "untagged-bar", 0, kOpen, { leather });
        rows.Venue("Closed Bar", "closed-bar", 0, kClosed, { bears });
        rows.Organization("League", "league", DbSchema::kOrganizationsOrgKindSportsLeague, 0,
                          kOpen, { bears });
        rows.Organization("Chorus", "chorus", DbSchema::kOrganizationsOrgKindChorusPerformance, 0,
                          kOpen, {});
        rows.Place("Beach", "beach", 0, kOpen, { bears });
        rows.Link("Bears Discord", 0, DbSchema::kGuideLinksSceneTagId, bears);
        rows.Page("bears", DbSchema::kGuidePagesSectionScenes, PageState::Published);
        rows.Page("leather", DbSchema::kGuidePagesSectionScenes, PageState::Draft);

        GuideHelper guide(databaseHelper);
        std::optional<SceneDetail> detail = guide.GetSceneDetail(transaction, "bears");
        ASSERT_TRUE(detail.has_value());
        EXPECT_EQ(detail->scene.slug, "bears");
        EXPECT_EQ(detail->scene.displayName, "Name of bears");
        EXPECT_EQ(Names(detail->venues), (Strings{ "Tagged Bar" }));
        EXPECT_EQ(Names(detail->organizations), (Strings{ "League" }));
        EXPECT_EQ(Names(detail->places), (Strings{ "Beach" }));
        EXPECT_EQ(detail->scene.venueCount, 1);
        EXPECT_EQ(detail->scene.organizationCount, 1);
        EXPECT_EQ(detail->scene.placeCount, 1);
        ASSERT_EQ(detail->links.size(), 1u);
        EXPECT_EQ(detail->links[0].label, "Bears Discord");
        ASSERT_TRUE(detail->introPage.has_value());
        EXPECT_EQ(detail->introPage->slug, "bears");
        EXPECT_TRUE(detail->introPage->published);

        // The closed venue left the scene, not the guide.
        EXPECT_EQ(Names(guide.GetGraveyard(transaction).venues), (Strings{ "Closed Bar" }));

        // A draft intro is no intro yet.
        std::optional<SceneDetail> leatherDetail = guide.GetSceneDetail(transaction, "leather");
        ASSERT_TRUE(leatherDetail.has_value());
        EXPECT_EQ(Names(leatherDetail->venues), (Strings{ "Untagged Bar" }));
        EXPECT_TRUE(leatherDetail->links.empty());
        EXPECT_FALSE(leatherDetail->introPage.has_value());
    });
}

TEST(GuideHelperTest, DisabledScenesHaveNoPageAndDropOffEveryItem) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "DisabledScenesHaveNoPageAndDropOffEveryItem", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t hidden = rows.SceneTag("hidden", 20, false);
        rows.Venue("Bar", "bar", 0, kOpen, { bears, hidden });
        rows.Organization("Club", "club", DbSchema::kOrganizationsOrgKindClubHobby, 0, kOpen,
                          { hidden });
        rows.Place("Beach", "beach", 0, kOpen, { hidden, bears });
        int64_t pageId =
            rows.Page("a-page", DbSchema::kGuidePagesSectionFreeze, PageState::Published);
        TableHelpers::GuidePageSceneTagAssignments(databaseHelper).Assign(transaction, pageId, hidden);

        GuideHelper guide(databaseHelper);
        EXPECT_FALSE(guide.GetSceneDetail(transaction, "hidden").has_value());
        EXPECT_FALSE(guide.GetSceneDetail(transaction, "no-such-scene").has_value());
        EXPECT_EQ(guide.GetVenueProfileBySlug(transaction, "bar")->venue.sceneTagSlugs,
                  (Strings{ "bears" }));
        EXPECT_TRUE(guide.GetOrganizationBySlug(transaction, "club")->organization
                        .sceneTagSlugs.empty());
        EXPECT_EQ(guide.GetPlaceBySlug(transaction, "beach")->place.sceneTagSlugs,
                  (Strings{ "bears" }));
        EXPECT_TRUE(guide.GetPublishedGuidePage(transaction, "a-page")->sceneTagSlugs.empty());
        OrganizationFilters filters;
        filters.sceneSlug = "hidden";
        EXPECT_TRUE(guide.GetOrganizationsDirectory(transaction, filters).empty());
    });
}

// --- Organizations, places, services ---

TEST(GuideHelperTest, OrganizationsDirectoryFiltersByKindSceneAndNeighborhood) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "OrganizationsDirectoryFiltersByKindSceneAndNeighborhood", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t leather = rows.SceneTag("leather", 20);
        constexpr std::string_view sports = DbSchema::kOrganizationsOrgKindSportsLeague;
        rows.Organization("League", "league", sports, capitolHill, kOpen, { bears });
        rows.Organization("Chorus", "chorus", DbSchema::kOrganizationsOrgKindChorusPerformance, 0,
                          kOpen, { leather });
        rows.Organization("Closed League", "closed-league", sports, capitolHill, kClosed, { bears });
        rows.Organization("Moved League", "moved-league", sports, 0, kMoved, {});

        GuideHelper guide(databaseHelper);
        auto names = [&](std::string_view orgKind, std::string_view sceneSlug,
                         std::string_view neighborhoodSlug) {
            OrganizationFilters filters;
            filters.orgKind = std::string(orgKind);
            filters.sceneSlug = std::string(sceneSlug);
            filters.neighborhoodSlug = std::string(neighborhoodSlug);
            return Names(guide.GetOrganizationsDirectory(transaction, filters));
        };
        EXPECT_EQ(names("", "", ""), (Strings{ "Chorus", "League" }));
        // The by-kind read sees every status; the retired ones still stay out.
        EXPECT_EQ(names(sports, "", ""), (Strings{ "League" }));
        EXPECT_EQ(names("", "leather", ""), (Strings{ "Chorus" }));
        EXPECT_EQ(names("", "", "capitol-hill"), (Strings{ "League" }));
        EXPECT_TRUE(names(DbSchema::kOrganizationsOrgKindChorusPerformance, "bears", "").empty());
    });
}

TEST(GuideHelperTest, OrganizationAndPlacePagesShowAnyStatusWithTheirLinks) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "OrganizationAndPlacePagesShowAnyStatusWithTheirLinks", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t clubId = rows.Organization("Old Club", "old-club",
                                           DbSchema::kOrganizationsOrgKindClubHobby, capitolHill,
                                           kClosed, { bears });
        TableHelpers::Organizations(databaseHelper).UpdateOrganization(transaction, clubId, {
            { std::string(DbSchema::kOrganizationsDescription), "The club's description." },
            { std::string(DbSchema::kOrganizationsUrl), "https://example.org/club" },
            { std::string(DbSchema::kOrganizationsMeetingCadenceText), "Thursdays, 7 PM" },
            { std::string(DbSchema::kOrganizationsClosureNote), "Closed in 2020." },
        });
        int64_t beachId = rows.Place("Beach", "beach", capitolHill, kOpen, { bears });
        TableHelpers::Places(databaseHelper).UpdatePlace(transaction, beachId, {
            { std::string(DbSchema::kPlacesDescription), "The place's description." },
            { std::string(DbSchema::kPlacesAddressText), "At the end of the road" },
            { std::string(DbSchema::kPlacesAdult), "true" },
        });
        rows.Link("club-link", 0, DbSchema::kGuideLinksOrganizationId, clubId);
        rows.Link("beach-link", 0, DbSchema::kGuideLinksPlaceId, beachId);

        GuideHelper guide(databaseHelper);
        std::optional<OrganizationDetail> club = guide.GetOrganizationBySlug(transaction, "old-club");
        ASSERT_TRUE(club.has_value());
        EXPECT_EQ(club->organization.id, clubId);
        EXPECT_EQ(club->organization.slug, "old-club");
        EXPECT_EQ(club->organization.name, "Old Club");
        EXPECT_EQ(club->organization.orgKind, DbSchema::kOrganizationsOrgKindClubHobby);
        EXPECT_EQ(club->organization.description, "The club's description.");
        EXPECT_EQ(club->organization.url, "https://example.org/club");
        EXPECT_EQ(club->organization.meetingCadenceText, "Thursdays, 7 PM");
        EXPECT_EQ(club->organization.neighborhoodSlug, "capitol-hill");
        EXPECT_EQ(club->organization.closureNote, "Closed in 2020.");
        EXPECT_EQ(club->organization.sceneTagSlugs, (Strings{ "bears" }));
        EXPECT_EQ(club->organization.freshness.status, kClosed);
        ASSERT_EQ(club->links.size(), 1u);
        EXPECT_EQ(club->links[0].label, "club-link");

        std::optional<PlaceDetail> beach = guide.GetPlaceBySlug(transaction, "beach");
        ASSERT_TRUE(beach.has_value());
        EXPECT_EQ(beach->place.id, beachId);
        EXPECT_EQ(beach->place.slug, "beach");
        EXPECT_EQ(beach->place.name, "Beach");
        EXPECT_EQ(beach->place.placeKind, DbSchema::kPlacesPlaceKindParkBeach);
        EXPECT_EQ(beach->place.description, "The place's description.");
        EXPECT_EQ(beach->place.addressText, "At the end of the road");
        EXPECT_EQ(beach->place.neighborhoodSlug, "capitol-hill");
        EXPECT_TRUE(beach->place.adult);
        EXPECT_EQ(beach->place.sceneTagSlugs, (Strings{ "bears" }));
        EXPECT_EQ(beach->place.freshness.status, kOpen);
        ASSERT_EQ(beach->links.size(), 1u);
        EXPECT_EQ(beach->links[0].label, "beach-link");

        EXPECT_FALSE(guide.GetOrganizationBySlug(transaction, "no-such-org").has_value());
        EXPECT_FALSE(guide.GetPlaceBySlug(transaction, "no-such-place").has_value());
    });
}

TEST(GuideHelperTest, PlacesListTheActiveOnesByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PlacesListTheActiveOnesByName", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        rows.Place("Bravo Park", "a-bravo", 0, kOpen, {});
        rows.Place("Alpha Park", "b-alpha", 0, kSeasonal, {});
        rows.Place("Closed Park", "closed-park", 0, kClosed, {});
        rows.Place("Moved Park", "moved-park", 0, kMoved, {});

        EXPECT_EQ(Names(GuideHelper(databaseHelper).GetPlaces(transaction)),
                  (Strings{ "Alpha Park", "Bravo Park" }));
    });
}

TEST(GuideHelperTest, ServicesListTheActiveOnesOfAKind) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ServicesListTheActiveOnesOfAKind", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        constexpr std::string_view testing = DbSchema::kServiceListingsServiceKindStiTesting;
        int64_t clinicId = rows.Service("Clinic", "clinic", testing, kOpen, true);
        TableHelpers::ServiceListings(databaseHelper).UpdateServiceListing(transaction, clinicId, {
            { std::string(DbSchema::kServiceListingsDescription), "The clinic's description." },
            { std::string(DbSchema::kServiceListingsUrl), "https://example.org/clinic" },
            { std::string(DbSchema::kServiceListingsPhone), "206-555-0100" },
            { std::string(DbSchema::kServiceListingsAddressText), "100 Example Ave" },
            { std::string(DbSchema::kServiceListingsNeighborhoodId), StringFromInt(capitolHill) },
        });
        rows.Service("Closed Clinic", "closed-clinic", testing, kClosed, true);
        rows.Service("Barber", "barber", DbSchema::kServiceListingsServiceKindEverydayService,
                     kOpen, false);

        GuideHelper guide(databaseHelper);
        std::vector<GuideService> all = guide.GetServices(transaction, "");
        EXPECT_EQ(Names(all), (Strings{ "Barber", "Clinic" }));
        const GuideService* clinic = FindByName(all, "Clinic");
        ASSERT_NE(clinic, nullptr);
        EXPECT_EQ(clinic->id, clinicId);
        EXPECT_EQ(clinic->slug, "clinic");
        EXPECT_EQ(clinic->serviceKind, testing);
        EXPECT_EQ(clinic->description, "The clinic's description.");
        EXPECT_EQ(clinic->url, "https://example.org/clinic");
        EXPECT_EQ(clinic->phone, "206-555-0100");
        EXPECT_EQ(clinic->addressText, "100 Example Ave");
        EXPECT_EQ(clinic->neighborhoodSlug, "capitol-hill");
        EXPECT_TRUE(clinic->isHealth);
        EXPECT_EQ(clinic->freshness.status, kOpen);
        EXPECT_FALSE(FindByName(all, "Barber")->isHealth);

        EXPECT_EQ(Names(guide.GetServices(transaction, testing)), (Strings{ "Clinic" }));
        EXPECT_TRUE(guide.GetServices(
            transaction, DbSchema::kServiceListingsServiceKindDental).empty());
    });
}

// --- Neighborhoods and the graveyard ---

TEST(GuideHelperTest, NeighborhoodPagesListTheirActiveListings) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NeighborhoodPagesListTheirActiveListings", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        int64_t ballard = rows.Neighborhood("ballard");
        int64_t oldHood = rows.Neighborhood("old-hood", false);
        rows.Venue("Hill Bar", "hill-bar", capitolHill, kOpen, {});
        rows.Venue("Closed Hill Bar", "closed-hill-bar", capitolHill, kClosed, {});
        rows.Venue("Ballard Bar", "ballard-bar", ballard, kOpen, {});
        rows.Venue("Old Hood Bar", "old-hood-bar", oldHood, kOpen, {});
        rows.Organization("Hill Club", "hill-club", DbSchema::kOrganizationsOrgKindClubHobby,
                          capitolHill, kOpen, {});
        rows.Organization("Countywide Group", "countywide-group",
                          DbSchema::kOrganizationsOrgKindSocialMeetup, 0, kOpen, {});
        rows.Place("Hill Park", "hill-park", capitolHill, kOpen, {});
        rows.Page("capitol-hill", DbSchema::kGuidePagesSectionNewToSeattle, PageState::Published);

        GuideHelper guide(databaseHelper);
        std::vector<NeighborhoodSummary> overview = guide.GetNeighborhoodsOverview(transaction);
        ASSERT_EQ(overview.size(), 2u);  // `old-hood` is disabled
        EXPECT_EQ(overview[0].slug, "capitol-hill");
        EXPECT_EQ(overview[0].displayName, "Name of capitol-hill");
        EXPECT_EQ(overview[0].region, DbSchema::kNeighborhoodsRegionSeattleCore);
        EXPECT_EQ(overview[0].venueCount, 1);
        EXPECT_EQ(overview[0].organizationCount, 1);
        EXPECT_EQ(overview[0].placeCount, 1);
        EXPECT_EQ(overview[1].slug, "ballard");
        EXPECT_EQ(overview[1].venueCount, 1);
        EXPECT_EQ(overview[1].organizationCount, 0);
        EXPECT_EQ(overview[1].placeCount, 0);

        std::optional<NeighborhoodDetail> detail =
            guide.GetNeighborhoodDetail(transaction, "capitol-hill");
        ASSERT_TRUE(detail.has_value());
        EXPECT_EQ(detail->neighborhood.slug, "capitol-hill");
        EXPECT_EQ(Names(detail->venues), (Strings{ "Hill Bar" }));
        EXPECT_EQ(Names(detail->organizations), (Strings{ "Hill Club" }));
        EXPECT_EQ(Names(detail->places), (Strings{ "Hill Park" }));
        EXPECT_EQ(detail->neighborhood.venueCount, 1);
        EXPECT_EQ(detail->neighborhood.organizationCount, 1);
        EXPECT_EQ(detail->neighborhood.placeCount, 1);
        ASSERT_TRUE(detail->rundownPage.has_value());
        EXPECT_EQ(detail->rundownPage->slug, "capitol-hill");

        EXPECT_FALSE(guide.GetNeighborhoodDetail(transaction, "old-hood").has_value());
        EXPECT_FALSE(guide.GetNeighborhoodDetail(transaction, "no-such-hood").has_value());
    });
}

TEST(GuideHelperTest, GraveyardIsTheClosedVenuesAndOrganizations) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GraveyardIsTheClosedVenuesAndOrganizations", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        VenueIds closedBar = rows.Venue("Closed Bar", "closed-bar", 0, kClosed, {});
        TableHelpers::VenueProfiles(databaseHelper).UpdateProfile(transaction, closedBar.profileId, {
            { std::string(DbSchema::kVenueProfilesClosureNote), "Closed in 2021." } });
        rows.Venue("Moved Bar", "moved-bar", 0, kMoved, {});
        rows.Venue("Open Bar", "open-bar", 0, kOpen, {});
        int64_t closedClub = rows.Organization("Closed Club", "closed-club",
                                               DbSchema::kOrganizationsOrgKindClubHobby, 0,
                                               kClosed, {});
        TableHelpers::Organizations(databaseHelper).UpdateOrganization(transaction, closedClub, {
            { std::string(DbSchema::kOrganizationsClosureNote), "Folded in 2019." } });
        rows.Organization("Moved Club", "moved-club", DbSchema::kOrganizationsOrgKindClubHobby, 0,
                          kMoved, {});
        rows.Place("Closed Park", "closed-park", 0, kClosed, {});

        Graveyard graveyard = GuideHelper(databaseHelper).GetGraveyard(transaction);
        ASSERT_EQ(Names(graveyard.venues), (Strings{ "Closed Bar" }));
        EXPECT_EQ(graveyard.venues[0].closureNote, "Closed in 2021.");
        ASSERT_EQ(Names(graveyard.organizations), (Strings{ "Closed Club" }));
        EXPECT_EQ(graveyard.organizations[0].closureNote, "Folded in 2019.");
    });
}

// --- Guide pages ---

TEST(GuideHelperTest, OnlyPublishedPagesArePublic) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OnlyPublishedPagesArePublic", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        constexpr std::string_view freeze = DbSchema::kGuidePagesSectionFreeze;
        rows.Page("published", freeze, PageState::Published);
        rows.Page("scheduled", freeze, PageState::Scheduled);
        rows.Page("draft", freeze, PageState::Draft);
        rows.Page("unscheduled", freeze, PageState::Unscheduled);

        struct Case {
            const char* slug;
            bool published;
        };
        const std::vector<Case> cases = {
            { "published", true }, { "scheduled", false }, { "draft", false },
            { "unscheduled", false },
        };
        GuideHelper guide(databaseHelper);
        for (const Case& expected : cases) {
            EXPECT_EQ(guide.GetPublishedGuidePage(transaction, expected.slug).has_value(),
                      expected.published) << expected.slug;
            // The preview path sees every state, and says which it is.
            std::optional<GuidePage> anyState = guide.GetGuidePageAnyState(transaction, expected.slug);
            ASSERT_TRUE(anyState.has_value()) << expected.slug;
            EXPECT_EQ(anyState->published, expected.published) << expected.slug;
            EXPECT_EQ(anyState->draft, std::string(expected.slug) == "draft") << expected.slug;
        }
        EXPECT_FALSE(guide.GetGuidePageAnyState(transaction, "no-such-page").has_value());
        EXPECT_FALSE(guide.GetGuidePageAnyState(transaction, "unscheduled")->publishAtUs.has_value());
    });
}

TEST(GuideHelperTest, PublishedPageCarriesItsBodyTagsAndFreshness) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "PublishedPageCarriesItsBodyTagsAndFreshness", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t leather = rows.SceneTag("leather", 20);
        int64_t bears = rows.SceneTag("bears", 10);
        int64_t pageId = rows.Page("bears-guide", DbSchema::kGuidePagesSectionScenes,
                                   PageState::Published, "Bears in Seattle");
        TableHelpers::GuidePages(databaseHelper).UpdatePage(transaction, pageId, {
            { std::string(DbSchema::kGuidePagesBodyMarkdown), "# Bears\n\nWhere to start." },
            { std::string(DbSchema::kGuidePagesStatus), std::string(kUnverified) },
            { std::string(DbSchema::kGuidePagesLastVerifiedAtUs), StringFromInt(kEarlierUs) },
        });
        TableHelpers::GuidePageSceneTagAssignments tags(databaseHelper);
        tags.Assign(transaction, pageId, leather);
        tags.Assign(transaction, pageId, bears);

        std::optional<GuidePage> page =
            GuideHelper(databaseHelper).GetPublishedGuidePage(transaction, "bears-guide");
        ASSERT_TRUE(page.has_value());
        EXPECT_EQ(page->id, pageId);
        EXPECT_EQ(page->slug, "bears-guide");
        EXPECT_EQ(page->title, "Bears in Seattle");
        EXPECT_EQ(page->section, DbSchema::kGuidePagesSectionScenes);
        EXPECT_EQ(page->bodyMarkdown, "# Bears\n\nWhere to start.");
        EXPECT_FALSE(page->adult);
        EXPECT_FALSE(page->draft);
        EXPECT_TRUE(page->publishAtUs.has_value());
        EXPECT_TRUE(page->published);
        EXPECT_EQ(page->sceneTagSlugs, (Strings{ "bears", "leather" }));
        EXPECT_EQ(page->freshness.status, kUnverified);
        EXPECT_EQ(page->freshness.lastVerifiedAtUs, std::optional<int64_t>(kEarlierUs));
    });
}

TEST(GuideHelperTest, SectionListsItsPublishedPagesByTitle) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SectionListsItsPublishedPagesByTitle", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        constexpr std::string_view freeze = DbSchema::kGuidePagesSectionFreeze;
        rows.Page("a-page", freeze, PageState::Published, "Bravo");
        rows.Page("b-page", freeze, PageState::Published, "Alpha");
        rows.Page("c-page", freeze, PageState::Draft, "Charlie");
        rows.Page("d-page", DbSchema::kGuidePagesSectionHealth, PageState::Published, "Delta");

        std::vector<GuidePageSummary> pages =
            GuideHelper(databaseHelper).GetPublishedPagesBySection(transaction, freeze);
        ASSERT_EQ(pages.size(), 2u);
        EXPECT_EQ(pages[0].title, "Alpha");
        EXPECT_EQ(pages[0].slug, "b-page");
        EXPECT_EQ(pages[0].section, freeze);
        EXPECT_FALSE(pages[0].adult);
        EXPECT_TRUE(pages[0].publishAtUs.has_value());
        EXPECT_EQ(pages[0].freshness.status, kOpen);
        EXPECT_FALSE(pages[0].freshness.lastVerifiedAtUs.has_value());
        EXPECT_EQ(pages[1].title, "Bravo");
    });
}

TEST(GuideHelperTest, IntrosAndRundownsArePublishedPagesOfTheirSection) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "IntrosAndRundownsArePublishedPagesOfTheirSection", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        rows.SceneTag("bears", 10);
        rows.SceneTag("leather", 20);
        rows.SceneTag("sober", 30);
        rows.Neighborhood("capitol-hill");
        rows.Neighborhood("ballard");
        rows.Neighborhood("fremont");
        constexpr std::string_view scenes = DbSchema::kGuidePagesSectionScenes;
        constexpr std::string_view newToSeattle = DbSchema::kGuidePagesSectionNewToSeattle;
        rows.Page("bears", scenes, PageState::Published);
        rows.Page("leather", DbSchema::kGuidePagesSectionHealth, PageState::Published);  // wrong section
        rows.Page("sober", scenes, PageState::Scheduled);                                // not yet
        rows.Page("capitol-hill", newToSeattle, PageState::Published);
        rows.Page("ballard", scenes, PageState::Published);                              // wrong section
        rows.Page("fremont", newToSeattle, PageState::Draft);

        GuideHelper guide(databaseHelper);
        auto intro = [&](std::string_view slug) {
            return guide.GetSceneDetail(transaction, slug)->introPage.has_value();
        };
        auto rundown = [&](std::string_view slug) {
            return guide.GetNeighborhoodDetail(transaction, slug)->rundownPage.has_value();
        };
        EXPECT_TRUE(intro("bears"));
        EXPECT_FALSE(intro("leather"));
        EXPECT_FALSE(intro("sober"));
        EXPECT_TRUE(rundown("capitol-hill"));
        EXPECT_FALSE(rundown("ballard"));
        EXPECT_FALSE(rundown("fremont"));
    });
}

// --- Contract 3 ---

TEST(GuideHelperTest, EveryListingCarriesItsStatusAndVerifiedDate) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "EveryListingCarriesItsStatusAndVerifiedDate", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        GuideRows rows{ transaction, databaseHelper };
        int64_t capitolHill = rows.Neighborhood("capitol-hill");
        int64_t bears = rows.SceneTag("bears", 10);
        const std::string verified = StringFromInt(kEarlierUs);

        VenueIds venue = rows.Venue("Bar", "bar", capitolHill, kUnverified, { bears });
        TableHelpers::Venues(databaseHelper).UpdateVenue(transaction, venue.venueId, {
            { std::string(DbSchema::kVenuesLastVerifiedAtUs), verified } });
        TableHelpers::VenueProfiles(databaseHelper).UpdateProfile(transaction, venue.profileId, {
            { std::string(DbSchema::kVenueProfilesLastVerifiedAtUs), verified } });
        int64_t organization = rows.Organization(
            "Club", "club", DbSchema::kOrganizationsOrgKindClubHobby, capitolHill, kUnverified,
            { bears });
        TableHelpers::Organizations(databaseHelper).UpdateOrganization(transaction, organization, {
            { std::string(DbSchema::kOrganizationsLastVerifiedAtUs), verified } });
        int64_t place = rows.Place("Beach", "beach", capitolHill, kUnverified, { bears });
        TableHelpers::Places(databaseHelper).UpdatePlace(transaction, place, {
            { std::string(DbSchema::kPlacesLastVerifiedAtUs), verified } });
        int64_t service = rows.Service("Clinic", "clinic",
                                       DbSchema::kServiceListingsServiceKindStiTesting,
                                       kUnverified, true);
        TableHelpers::ServiceListings(databaseHelper).UpdateServiceListing(transaction, service, {
            { std::string(DbSchema::kServiceListingsLastVerifiedAtUs), verified } });
        int64_t page = rows.Page("page", DbSchema::kGuidePagesSectionFreeze, PageState::Published);
        TableHelpers::GuidePages(databaseHelper).UpdatePage(transaction, page, {
            { std::string(DbSchema::kGuidePagesStatus), std::string(kUnverified) },
            { std::string(DbSchema::kGuidePagesLastVerifiedAtUs), verified } });

        auto expectFreshness = [](const Freshness& freshness, const char* what) {
            EXPECT_EQ(freshness.status, kUnverified) << what;
            EXPECT_EQ(freshness.lastVerifiedAtUs, std::optional<int64_t>(kEarlierUs)) << what;
        };
        GuideHelper guide(databaseHelper);
        std::vector<GuideVenue> venues = guide.GetVenuesDirectory(transaction, VenueFilters());
        ASSERT_EQ(venues.size(), 1u);
        expectFreshness(venues[0].freshness, "venue");
        std::vector<GuideOrganization> organizations =
            guide.GetOrganizationsDirectory(transaction, OrganizationFilters());
        ASSERT_EQ(organizations.size(), 1u);
        expectFreshness(organizations[0].freshness, "organization");
        std::vector<GuidePlace> places = guide.GetPlaces(transaction);
        ASSERT_EQ(places.size(), 1u);
        expectFreshness(places[0].freshness, "place");
        std::vector<GuideService> services = guide.GetServices(transaction, "");
        ASSERT_EQ(services.size(), 1u);
        expectFreshness(services[0].freshness, "service");
        expectFreshness(guide.GetPublishedGuidePage(transaction, "page")->freshness, "page");
        std::vector<GuidePageSummary> pages = guide.GetPublishedPagesBySection(
            transaction, DbSchema::kGuidePagesSectionFreeze);
        ASSERT_EQ(pages.size(), 1u);
        expectFreshness(pages[0].freshness, "page summary");

        // The same items on a scene page and a neighborhood page.
        std::optional<SceneDetail> scene = guide.GetSceneDetail(transaction, "bears");
        ASSERT_TRUE(scene.has_value());
        ASSERT_EQ(scene->venues.size(), 1u);
        ASSERT_EQ(scene->organizations.size(), 1u);
        ASSERT_EQ(scene->places.size(), 1u);
        expectFreshness(scene->venues[0].freshness, "scene venue");
        expectFreshness(scene->organizations[0].freshness, "scene organization");
        expectFreshness(scene->places[0].freshness, "scene place");
        std::optional<NeighborhoodDetail> neighborhood =
            guide.GetNeighborhoodDetail(transaction, "capitol-hill");
        ASSERT_TRUE(neighborhood.has_value());
        ASSERT_EQ(neighborhood->venues.size(), 1u);
        expectFreshness(neighborhood->venues[0].freshness, "neighborhood venue");
    });
}

}  // namespace
}  // namespace Guide
