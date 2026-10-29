#include "business_logic/guide/guide_key_value_table.h"

#include <gtest/gtest.h>

#include "util/types.h"

namespace Guide {
namespace {

constexpr int64_t kVenueVerifiedUs = 1790000000000000LL;
constexpr int64_t kProfileVerifiedUs = 1793000000000000LL;

GuideVenue MakeVenue() {
    GuideVenue venue;
    venue.venueId = 7;
    venue.profileId = 70;
    venue.slug = "the-cuff";
    venue.name = "The Cuff";
    venue.description = "The venue's description.";
    venue.address = "1533 13th Ave, Seattle, WA 98122";
    venue.url = "https://example.org/cuff";
    venue.neighborhoodSlug = "capitol-hill";
    venue.crowdVibe = "The profile's crowd and vibe.";
    venue.priceBand = "$$";
    venue.adult = true;
    venue.closureNote = "";
    venue.sceneTagSlugs = { "leather", "bears" };
    venue.freshness.status = "open";
    venue.freshness.lastVerifiedAtUs = kVenueVerifiedUs;
    venue.venueLastVerifiedAtUs = kVenueVerifiedUs;
    venue.profileLastVerifiedAtUs = kProfileVerifiedUs;
    return venue;
}

TEST(GuideKeyValueTableTest, VenueConvertsEveryField) {
    KeyValueTable table = GuideVenueToKeyValueTable(MakeVenue());
    EXPECT_EQ(table["venue_id"], "7");
    EXPECT_EQ(table["profile_id"], "70");
    EXPECT_EQ(table["slug"], "the-cuff");
    EXPECT_EQ(table["name"], "The Cuff");
    EXPECT_EQ(table["description"], "The venue's description.");
    EXPECT_EQ(table["address"], "1533 13th Ave, Seattle, WA 98122");
    EXPECT_EQ(table["url"], "https://example.org/cuff");
    EXPECT_EQ(table["neighborhood_slug"], "capitol-hill");
    EXPECT_EQ(table["crowd_vibe"], "The profile's crowd and vibe.");
    EXPECT_EQ(table["price_band"], "$$");
    EXPECT_EQ(table["adult"], "true");
    EXPECT_EQ(table["closure_note"], "");
    EXPECT_EQ(table["scene_tag_slugs"], "leather,bears");
    EXPECT_EQ(table["status"], "open");
    EXPECT_EQ(table["last_verified_at_us"], StringFromInt(kVenueVerifiedUs));
    EXPECT_EQ(table["venue_last_verified_at_us"], StringFromInt(kVenueVerifiedUs));
    EXPECT_EQ(table["profile_last_verified_at_us"], StringFromInt(kProfileVerifiedUs));
    EXPECT_EQ(table.size(), 17u);
    // The verifier's bookkeeping stays in.
    EXPECT_EQ(table.count("volatility_tier"), 0u);
}

// A "0" would reach the client as the epoch; empty maps to null.
TEST(GuideKeyValueTableTest, NeverVerifiedIsAnEmptyString) {
    GuideVenue venue = MakeVenue();
    venue.freshness.lastVerifiedAtUs.reset();
    venue.profileLastVerifiedAtUs.reset();
    venue.adult = false;
    venue.sceneTagSlugs.clear();
    KeyValueTable table = GuideVenueToKeyValueTable(venue);
    EXPECT_EQ(table["last_verified_at_us"], "");
    EXPECT_EQ(table["profile_last_verified_at_us"], "");
    EXPECT_EQ(table["venue_last_verified_at_us"], StringFromInt(kVenueVerifiedUs));
    EXPECT_EQ(table["adult"], "false");
    EXPECT_EQ(table["scene_tag_slugs"], "");
}

TEST(GuideKeyValueTableTest, OrganizationConvertsEveryField) {
    GuideOrganization organization;
    organization.id = 3;
    organization.slug = "example-softball";
    organization.name = "Example Softball League";
    organization.orgKind = "sports-league";
    organization.description = "The league's description.";
    organization.url = "https://example.org/softball";
    organization.meetingCadenceText = "Sundays, April to September";
    organization.neighborhoodSlug = "";
    organization.adult = true;
    organization.closureNote = "Folded in 2024.";
    organization.sceneTagSlugs = { "sports" };
    organization.freshness.status = "closed";
    organization.freshness.lastVerifiedAtUs = kVenueVerifiedUs;

    KeyValueTable table = GuideOrganizationToKeyValueTable(organization);
    EXPECT_EQ(table["id"], "3");
    EXPECT_EQ(table["slug"], "example-softball");
    EXPECT_EQ(table["name"], "Example Softball League");
    EXPECT_EQ(table["org_kind"], "sports-league");
    EXPECT_EQ(table["description"], "The league's description.");
    EXPECT_EQ(table["url"], "https://example.org/softball");
    EXPECT_EQ(table["meeting_cadence_text"], "Sundays, April to September");
    EXPECT_EQ(table["neighborhood_slug"], "");
    EXPECT_EQ(table["adult"], "true");
    EXPECT_EQ(table["closure_note"], "Folded in 2024.");
    EXPECT_EQ(table["scene_tag_slugs"], "sports");
    EXPECT_EQ(table["status"], "closed");
    EXPECT_EQ(table["last_verified_at_us"], StringFromInt(kVenueVerifiedUs));
    EXPECT_EQ(table.size(), 13u);
}

TEST(GuideKeyValueTableTest, PlaceConvertsEveryField) {
    GuidePlace place;
    place.id = 4;
    place.slug = "example-beach";
    place.name = "Example Beach";
    place.placeKind = "park-beach";
    place.description = "The place's description.";
    place.addressText = "At the end of the road";
    place.neighborhoodSlug = "madison-park";
    place.adult = true;
    place.sceneTagSlugs = { "outdoors" };
    place.freshness.status = "seasonal";

    KeyValueTable table = GuidePlaceToKeyValueTable(place);
    EXPECT_EQ(table["id"], "4");
    EXPECT_EQ(table["slug"], "example-beach");
    EXPECT_EQ(table["name"], "Example Beach");
    EXPECT_EQ(table["place_kind"], "park-beach");
    EXPECT_EQ(table["description"], "The place's description.");
    EXPECT_EQ(table["address_text"], "At the end of the road");
    EXPECT_EQ(table["neighborhood_slug"], "madison-park");
    EXPECT_EQ(table["adult"], "true");
    EXPECT_EQ(table["scene_tag_slugs"], "outdoors");
    EXPECT_EQ(table["status"], "seasonal");
    EXPECT_EQ(table["last_verified_at_us"], "");
    EXPECT_EQ(table.size(), 11u);
}

TEST(GuideKeyValueTableTest, ServiceConvertsEveryField) {
    GuideService service;
    service.id = 5;
    service.slug = "clinic";
    service.name = "A Clinic";
    service.serviceKind = "sti-testing";
    service.description = "Walk-in testing.";
    service.url = "https://example.org/clinic";
    service.phone = "206-555-0100";
    service.addressText = "100 Broadway E";
    service.neighborhoodSlug = "capitol-hill";
    service.isHealth = true;
    service.freshness.status = "open";
    service.freshness.lastVerifiedAtUs = kProfileVerifiedUs;

    KeyValueTable table = GuideServiceToKeyValueTable(service);
    EXPECT_EQ(table["id"], "5");
    EXPECT_EQ(table["slug"], "clinic");
    EXPECT_EQ(table["name"], "A Clinic");
    EXPECT_EQ(table["service_kind"], "sti-testing");
    EXPECT_EQ(table["description"], "Walk-in testing.");
    EXPECT_EQ(table["url"], "https://example.org/clinic");
    EXPECT_EQ(table["phone"], "206-555-0100");
    EXPECT_EQ(table["address_text"], "100 Broadway E");
    EXPECT_EQ(table["neighborhood_slug"], "capitol-hill");
    EXPECT_EQ(table["is_health"], "true");
    EXPECT_EQ(table["status"], "open");
    EXPECT_EQ(table["last_verified_at_us"], StringFromInt(kProfileVerifiedUs));
    EXPECT_EQ(table.size(), 12u);

    service.isHealth = false;
    EXPECT_EQ(GuideServiceToKeyValueTable(service)["is_health"], "false");
}

TEST(GuideKeyValueTableTest, PageConvertsEveryField) {
    GuidePage page;
    page.id = 6;
    page.slug = "bears";
    page.title = "Bears in Seattle";
    page.section = "scenes";
    page.bodyMarkdown = "# Bears\n\nWhere to start.";
    page.adult = false;
    page.draft = false;
    page.publishAtUs = kVenueVerifiedUs;
    page.published = true;
    page.sceneTagSlugs = { "bears" };
    page.freshness.status = "open";
    page.freshness.lastVerifiedAtUs = kProfileVerifiedUs;

    KeyValueTable table = GuidePageToKeyValueTable(page);
    EXPECT_EQ(table["id"], "6");
    EXPECT_EQ(table["slug"], "bears");
    EXPECT_EQ(table["title"], "Bears in Seattle");
    EXPECT_EQ(table["section"], "scenes");
    EXPECT_EQ(table["body_markdown"], "# Bears\n\nWhere to start.");
    EXPECT_EQ(table["adult"], "false");
    EXPECT_EQ(table["draft"], "false");
    EXPECT_EQ(table["publish_at_us"], StringFromInt(kVenueVerifiedUs));
    EXPECT_EQ(table["published"], "true");
    EXPECT_EQ(table["scene_tag_slugs"], "bears");
    EXPECT_EQ(table["status"], "open");
    EXPECT_EQ(table["last_verified_at_us"], StringFromInt(kProfileVerifiedUs));
    EXPECT_EQ(table.size(), 12u);

    // An unscheduled draft, as the preview path shows it.
    page.draft = true;
    page.publishAtUs.reset();
    page.published = false;
    KeyValueTable draft = GuidePageToKeyValueTable(page);
    EXPECT_EQ(draft["draft"], "true");
    EXPECT_EQ(draft["publish_at_us"], "");
    EXPECT_EQ(draft["published"], "false");
}

TEST(GuideKeyValueTableTest, PageSummaryHasNoBody) {
    GuidePageSummary page;
    page.id = 6;
    page.slug = "the-freeze";
    page.title = "The Freeze";
    page.section = "freeze";
    page.adult = false;
    page.publishAtUs = kVenueVerifiedUs;
    page.freshness.status = "unverified";

    KeyValueTable table = GuidePageSummaryToKeyValueTable(page);
    EXPECT_EQ(table["id"], "6");
    EXPECT_EQ(table["slug"], "the-freeze");
    EXPECT_EQ(table["title"], "The Freeze");
    EXPECT_EQ(table["section"], "freeze");
    EXPECT_EQ(table["adult"], "false");
    EXPECT_EQ(table["publish_at_us"], StringFromInt(kVenueVerifiedUs));
    EXPECT_EQ(table["status"], "unverified");
    EXPECT_EQ(table["last_verified_at_us"], "");
    EXPECT_EQ(table.size(), 8u);
    EXPECT_EQ(table.count("body_markdown"), 0u);
}

TEST(GuideKeyValueTableTest, LinkConvertsEveryField) {
    GuideLink link;
    link.id = 9;
    link.label = "Seattle Bears Discord";
    link.url = "https://example.org/discord";
    link.platform = "discord";

    KeyValueTable table = GuideLinkToKeyValueTable(link);
    EXPECT_EQ(table["id"], "9");
    EXPECT_EQ(table["label"], "Seattle Bears Discord");
    EXPECT_EQ(table["url"], "https://example.org/discord");
    EXPECT_EQ(table["platform"], "discord");
    EXPECT_EQ(table.size(), 4u);
}

TEST(GuideKeyValueTableTest, SummariesConvertEveryField) {
    SceneSummary scene;
    scene.slug = "bears";
    scene.displayName = "Bears";
    scene.description = "The scene's description.";
    scene.coverage = "full";
    scene.venueCount = 2;
    scene.organizationCount = 1;
    scene.placeCount = 0;
    KeyValueTable sceneTable = SceneSummaryToKeyValueTable(scene);
    EXPECT_EQ(sceneTable["slug"], "bears");
    EXPECT_EQ(sceneTable["display_name"], "Bears");
    EXPECT_EQ(sceneTable["description"], "The scene's description.");
    EXPECT_EQ(sceneTable["coverage"], "full");
    EXPECT_EQ(sceneTable["venue_count"], "2");
    EXPECT_EQ(sceneTable["organization_count"], "1");
    EXPECT_EQ(sceneTable["place_count"], "0");
    EXPECT_EQ(sceneTable.size(), 7u);

    NeighborhoodSummary neighborhood;
    neighborhood.slug = "capitol-hill";
    neighborhood.displayName = "Capitol Hill";
    neighborhood.region = "seattle-core";
    neighborhood.venueCount = 3;
    neighborhood.organizationCount = 0;
    neighborhood.placeCount = 1;
    KeyValueTable neighborhoodTable = NeighborhoodSummaryToKeyValueTable(neighborhood);
    EXPECT_EQ(neighborhoodTable["slug"], "capitol-hill");
    EXPECT_EQ(neighborhoodTable["display_name"], "Capitol Hill");
    EXPECT_EQ(neighborhoodTable["region"], "seattle-core");
    EXPECT_EQ(neighborhoodTable["venue_count"], "3");
    EXPECT_EQ(neighborhoodTable["organization_count"], "0");
    EXPECT_EQ(neighborhoodTable["place_count"], "1");
    EXPECT_EQ(neighborhoodTable.size(), 6u);
}

TEST(GuideKeyValueTableTest, ArraysKeepOrder) {
    GuideVenue first = MakeVenue();
    GuideVenue second = MakeVenue();
    second.venueId = 8;
    KeyValueTableArray venues = GuideVenuesToKeyValueTableArray({ first, second });
    ASSERT_EQ(venues.size(), 2u);
    EXPECT_EQ(venues[0]["venue_id"], "7");
    EXPECT_EQ(venues[1]["venue_id"], "8");
    EXPECT_TRUE(GuideVenuesToKeyValueTableArray({}).empty());

    GuideOrganization organization;
    organization.id = 3;
    GuidePlace place;
    place.id = 4;
    GuideService service;
    service.id = 5;
    GuidePageSummary page;
    page.id = 6;
    GuideLink link;
    link.id = 9;
    SceneSummary scene;
    scene.slug = "bears";
    NeighborhoodSummary neighborhood;
    neighborhood.slug = "ballard";
    EXPECT_EQ(GuideOrganizationsToKeyValueTableArray({ organization })[0]["id"], "3");
    EXPECT_EQ(GuidePlacesToKeyValueTableArray({ place })[0]["id"], "4");
    EXPECT_EQ(GuideServicesToKeyValueTableArray({ service })[0]["id"], "5");
    EXPECT_EQ(GuidePageSummariesToKeyValueTableArray({ page })[0]["id"], "6");
    EXPECT_EQ(GuideLinksToKeyValueTableArray({ link })[0]["id"], "9");
    EXPECT_EQ(SceneSummariesToKeyValueTableArray({ scene })[0]["slug"], "bears");
    EXPECT_EQ(NeighborhoodSummariesToKeyValueTableArray({ neighborhood })[0]["slug"], "ballard");
}

}  // namespace
}  // namespace Guide
