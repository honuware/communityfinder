#include "guide_endpoint_test_support.h"

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
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/guide_links.h"
#include "sql_util/table_helpers/guide_page_scene_tag_assignments.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/organization_scene_tag_assignments.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/place_scene_tag_assignments.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/service_listings.h"
#include "sql_util/table_helpers/venue_profile_scene_tag_assignments.h"
#include "sql_util/table_helpers/venue_profiles.h"
#include "sql_util/table_helpers/venues.h"
#include "util/thread_pool.h"
#include "util/types.h"

namespace Endpoints::GuideEndpointTest {
namespace {

constexpr int64_t kUsPerDay = 86'400LL * 1'000'000LL;

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

}  // namespace

World MakeWorld(Transaction& transaction, DatabaseHelper databaseHelper) {
    World world;

    TableHelpers::Neighborhoods neighborhoods(databaseHelper);
    auto addNeighborhood = [&](std::string_view slug) {
        return neighborhoods.AddNeighborhood(transaction, {
            { std::string(DbSchema::kNeighborhoodsSlug), std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsDisplayName), "Name of " + std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsRegion),
              std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
        });
    };
    world.capitolHill = addNeighborhood("capitol-hill");
    world.ballard = addNeighborhood("ballard");

    TableHelpers::SceneTags sceneTags(databaseHelper);
    auto addSceneTag = [&](std::string_view slug, int64_t sortOrder, std::string_view coverage) {
        return sceneTags.AddSceneTag(transaction, {
            { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
            { std::string(DbSchema::kSceneTagsDisplayName), "Name of " + std::string(slug) },
            { std::string(DbSchema::kSceneTagsDescription), "About " + std::string(slug) },
            { std::string(DbSchema::kSceneTagsCoverage), std::string(coverage) },
            { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
        });
    };
    world.leather = addSceneTag("leather", 10, DbSchema::kSceneTagsCoverageFull);
    world.bears = addSceneTag("bears", 20, DbSchema::kSceneTagsCoverageLinkOut);

    TableHelpers::GuideLinks links(databaseHelper);
    auto addLink = [&](std::string_view label, std::string_view parentColumn, int64_t parentId) {
        links.AddLink(transaction, {
            { std::string(DbSchema::kGuideLinksLabel), std::string(label) },
            { std::string(DbSchema::kGuideLinksUrl), "https://example.org/" + std::string(label) },
            { std::string(DbSchema::kGuideLinksPlatform),
              std::string(DbSchema::kGuideLinksPlatformDiscord) },
            { std::string(parentColumn), StringFromInt(parentId) },
        });
    };
    addLink("scene-chat", DbSchema::kGuideLinksSceneTagId, world.leather);

    // Venues and their profiles.
    TableHelpers::Venues venues(databaseHelper);
    TableHelpers::VenueProfiles profiles(databaseHelper);
    TableHelpers::VenueProfileSceneTagAssignments profileTags(databaseHelper);
    world.exampleBar = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "Example Bar" },
        { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(world.capitolHill) },
    });
    int64_t exampleBarProfile = profiles.AddProfile(transaction, {
        { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(world.exampleBar) },
        { std::string(DbSchema::kVenueProfilesSlug), "example-bar" },
        { std::string(DbSchema::kVenueProfilesCrowdVibe), "The profile's crowd and vibe." },
        { std::string(DbSchema::kVenueProfilesPriceBand), "$$" },
    });
    profileTags.Assign(transaction, exampleBarProfile, world.leather);
    addLink("bar-site", DbSchema::kGuideLinksVenueProfileId, exampleBarProfile);

    world.closedBar = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "Closed Bar" },
        { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(world.ballard) },
        { std::string(DbSchema::kVenuesStatus), std::string(DbSchema::kFreshnessStatusClosed) },
    });
    int64_t closedBarProfile = profiles.AddProfile(transaction, {
        { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(world.closedBar) },
        { std::string(DbSchema::kVenueProfilesSlug), "closed-bar" },
        { std::string(DbSchema::kVenueProfilesClosureNote), "The venue's closure note." },
    });
    profileTags.Assign(transaction, closedBarProfile, world.leather);

    // Organizations.
    TableHelpers::Organizations organizations(databaseHelper);
    TableHelpers::OrganizationSceneTagAssignments organizationTags(databaseHelper);
    world.league = organizations.AddOrganization(transaction, {
        { std::string(DbSchema::kOrganizationsName), "Example League" },
        { std::string(DbSchema::kOrganizationsSlug), "example-league" },
        { std::string(DbSchema::kOrganizationsOrgKind),
          std::string(DbSchema::kOrganizationsOrgKindSportsLeague) },
        { std::string(DbSchema::kOrganizationsNeighborhoodId), StringFromInt(world.capitolHill) },
    });
    organizationTags.Assign(transaction, world.league, world.leather);
    addLink("league-chat", DbSchema::kGuideLinksOrganizationId, world.league);

    world.chorus = organizations.AddOrganization(transaction, {
        { std::string(DbSchema::kOrganizationsName), "Example Chorus" },
        { std::string(DbSchema::kOrganizationsSlug), "example-chorus" },
        { std::string(DbSchema::kOrganizationsOrgKind),
          std::string(DbSchema::kOrganizationsOrgKindChorusPerformance) },
        { std::string(DbSchema::kOrganizationsNeighborhoodId), StringFromInt(world.ballard) },
    });
    organizationTags.Assign(transaction, world.chorus, world.bears);

    world.closedClub = organizations.AddOrganization(transaction, {
        { std::string(DbSchema::kOrganizationsName), "Closed Club" },
        { std::string(DbSchema::kOrganizationsSlug), "closed-club" },
        { std::string(DbSchema::kOrganizationsOrgKind),
          std::string(DbSchema::kOrganizationsOrgKindClubHobby) },
        { std::string(DbSchema::kOrganizationsStatus), std::string(DbSchema::kFreshnessStatusClosed) },
        { std::string(DbSchema::kOrganizationsClosureNote), "The club's closure note." },
    });

    // The place.
    world.beach = TableHelpers::Places(databaseHelper).AddPlace(transaction, {
        { std::string(DbSchema::kPlacesName), "Example Beach" },
        { std::string(DbSchema::kPlacesSlug), "example-beach" },
        { std::string(DbSchema::kPlacesPlaceKind), std::string(DbSchema::kPlacesPlaceKindParkBeach) },
        { std::string(DbSchema::kPlacesAdult), "true" },
        { std::string(DbSchema::kPlacesNeighborhoodId), StringFromInt(world.capitolHill) },
    });
    TableHelpers::PlaceSceneTagAssignments(databaseHelper).Assign(transaction, world.beach, world.leather);
    addLink("beach-group", DbSchema::kGuideLinksPlaceId, world.beach);

    // Services.
    TableHelpers::ServiceListings services(databaseHelper);
    world.clinic = services.AddServiceListing(transaction, {
        { std::string(DbSchema::kServiceListingsName), "Example Clinic" },
        { std::string(DbSchema::kServiceListingsSlug), "example-clinic" },
        { std::string(DbSchema::kServiceListingsServiceKind),
          std::string(DbSchema::kServiceListingsServiceKindStiTesting) },
        { std::string(DbSchema::kServiceListingsPhone), "2065550100" },
    });
    world.dryCleaner = services.AddServiceListing(transaction, {
        { std::string(DbSchema::kServiceListingsName), "Example Dry Cleaner" },
        { std::string(DbSchema::kServiceListingsSlug), "example-dry-cleaner" },
        { std::string(DbSchema::kServiceListingsServiceKind),
          std::string(DbSchema::kServiceListingsServiceKindEverydayService) },
        { std::string(DbSchema::kServiceListingsIsHealth), "false" },
    });

    // Pages: published yesterday unless noted.
    const int64_t nowUs = NowUs(transaction);
    TableHelpers::GuidePages pages(databaseHelper);
    auto addPage = [&](std::string_view slug, std::string_view section, bool draft,
                       int64_t publishAtUs) {
        return pages.AddPage(transaction, {
            { std::string(DbSchema::kGuidePagesTitle), "Title of " + std::string(slug) },
            { std::string(DbSchema::kGuidePagesSlug), std::string(slug) },
            { std::string(DbSchema::kGuidePagesSection), std::string(section) },
            { std::string(DbSchema::kGuidePagesBodyMarkdown), "Body of " + std::string(slug) },
            { std::string(DbSchema::kGuidePagesDraft), draft ? "true" : "false" },
            { std::string(DbSchema::kGuidePagesPublishAtUs), StringFromInt(publishAtUs) },
        });
    };
    const int64_t yesterday = nowUs - kUsPerDay;
    world.leatherIntro = addPage("leather", DbSchema::kGuidePagesSectionScenes, false, yesterday);
    world.capitolHillRundown =
        addPage("capitol-hill", DbSchema::kGuidePagesSectionNewToSeattle, false, yesterday);
    world.healthBasics = addPage("health-basics", DbSchema::kGuidePagesSectionHealth, false, yesterday);
    TableHelpers::GuidePageSceneTagAssignments(databaseHelper)
        .Assign(transaction, world.healthBasics, world.leather);
    world.draftPage = addPage("draft-page", DbSchema::kGuidePagesSectionHealth, true, yesterday);
    world.scheduledPage =
        addPage("scheduled-page", DbSchema::kGuidePagesSectionHealth, false, nowUs + kUsPerDay);
    return world;
}

crow::response Get(EndpointTestHelper& helper, const std::string& url, const std::string& query) {
    crow::request req;
    req.method = crow::HTTPMethod::Get;
    req.url = url;
    if (!query.empty()) {
        req.url_params = crow::query_string(("?" + query).c_str());
    }
    crow::response resp;
    helper.GetWebApp().GetApp().handle_full(req, resp);
    ThreadPool::GetInstance().Shutdown();
    return resp;
}

void SignIn(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper,
            std::string_view email, bool manageGuide) {
    // EV1's sign-in, without its manage_events grant.
    int64_t personId = EventsEndpointTest::SignIn(transaction, testDb, helper, email, false);
    if (manageGuide) {
        helper.GrantPermissionToPerson(transaction, personId, DbSchema::kPermissionManageGuide);
    }
}

std::vector<std::string> SlugsOf(const Json::Value& items) {
    std::vector<std::string> slugs;
    for (const Json::Value& item : items.GetArray()) {
        slugs.push_back(item["slug"].Get<std::string>());
    }
    return slugs;
}

const Json::Value* FindBySlug(const Json::Value& items, std::string_view slug) {
    for (const Json::Value& item : items.GetArray()) {
        const Json::Value* itemSlug = nullptr;
        if (item.HasChild("slug", &itemSlug) && itemSlug->Is<std::string>()
            && itemSlug->Get<std::string>() == slug) {
            return &item;
        }
    }
    return nullptr;
}

std::vector<std::string> Strings(const Json::Value& array) {
    std::vector<std::string> strings;
    for (const Json::Value& value : array.GetArray()) {
        strings.push_back(value.Get<std::string>());
    }
    return strings;
}

}  // namespace Endpoints::GuideEndpointTest
