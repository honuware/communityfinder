#include "guide_json.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

// GD2 §4 — the guide's JSON. Every text field below is given an all-digit
// value, the case KeyValueTableToJson would turn into a number, so each check
// that it's still a string proves the fix covers that field.
namespace Endpoints {
namespace {

void ExpectStrings(const Json::Value& object, std::initializer_list<std::string_view> keys) {
    for (std::string_view key : keys) {
        EXPECT_TRUE(object[key].Is<std::string>()) << key << " should be a string";
    }
}

std::vector<std::string> Strings(const Json::Value& array) {
    std::vector<std::string> strings;
    for (const Json::Value& value : array.GetArray()) {
        strings.push_back(value.Get<std::string>());
    }
    return strings;
}

TEST(GuideJsonTest, VenueTypesEveryField) {
    Guide::GuideVenue venue;
    venue.venueId = 7;
    venue.profileId = 8;
    venue.slug = "1";
    venue.name = "1999";
    venue.description = "2";
    venue.address = "3";
    venue.url = "4";
    venue.neighborhoodSlug = "5";
    venue.crowdVibe = "6";
    venue.priceBand = "7";
    venue.adult = true;
    venue.closureNote = "2021";
    venue.sceneTagSlugs = { "bears", "leather" };
    venue.freshness = { "open", 1700000000000000LL };
    venue.venueLastVerifiedAtUs = 1700000000000000LL;

    Json::Value json = GuideVenueToJson(venue);
    EXPECT_EQ(json.GetChildren().size(), 17u);
    ExpectStrings(json, { "slug", "name", "description", "address", "url", "neighborhood_slug",
                          "crowd_vibe", "price_band", "closure_note", "status" });
    EXPECT_EQ(json["name"].Get<std::string>(), "1999");
    EXPECT_EQ(json["venue_id"].Get<int64_t>(), 7);
    EXPECT_EQ(json["profile_id"].Get<int64_t>(), 8);
    EXPECT_EQ(json["adult"].Get<bool>(), true);
    EXPECT_EQ(Strings(json["scene_tag_slugs"]), (std::vector<std::string>{ "bears", "leather" }));
    EXPECT_EQ(json["last_verified_at_us"].Get<int64_t>(), 1700000000000000LL);
    EXPECT_EQ(json["venue_last_verified_at_us"].Get<int64_t>(), 1700000000000000LL);
    // Never verified is the empty string, which the client maps to null.
    EXPECT_EQ(json["profile_last_verified_at_us"].Get<std::string>(), "");
}

TEST(GuideJsonTest, OrganizationTypesEveryField) {
    Guide::GuideOrganization organization;
    organization.id = 3;
    organization.slug = "1";
    organization.name = "1999";
    organization.orgKind = "2";
    organization.description = "3";
    organization.url = "4";
    organization.meetingCadenceText = "5";
    organization.neighborhoodSlug = "6";
    organization.adult = true;
    organization.closureNote = "2021";
    organization.freshness = { "closed", std::nullopt };

    Json::Value json = GuideOrganizationToJson(organization);
    EXPECT_EQ(json.GetChildren().size(), 13u);
    ExpectStrings(json, { "slug", "name", "org_kind", "description", "url",
                          "meeting_cadence_text", "neighborhood_slug", "closure_note", "status" });
    EXPECT_EQ(json["id"].Get<int64_t>(), 3);
    EXPECT_EQ(json["adult"].Get<bool>(), true);
    EXPECT_TRUE(json["scene_tag_slugs"].GetArray().empty());
    EXPECT_EQ(json["status"].Get<std::string>(), "closed");
    EXPECT_EQ(json["last_verified_at_us"].Get<std::string>(), "");
}

TEST(GuideJsonTest, PlaceTypesEveryField) {
    Guide::GuidePlace place;
    place.id = 4;
    place.slug = "1";
    place.name = "1999";
    place.placeKind = "2";
    place.description = "3";
    place.addressText = "4";
    place.neighborhoodSlug = "5";
    place.adult = true;
    place.sceneTagSlugs = { "leather" };
    place.freshness = { "seasonal", std::nullopt };

    Json::Value json = GuidePlaceToJson(place);
    EXPECT_EQ(json.GetChildren().size(), 11u);
    ExpectStrings(json, { "slug", "name", "place_kind", "description", "address_text",
                          "neighborhood_slug", "status" });
    EXPECT_EQ(json["id"].Get<int64_t>(), 4);
    EXPECT_EQ(json["adult"].Get<bool>(), true);
    EXPECT_EQ(Strings(json["scene_tag_slugs"]), std::vector<std::string>{ "leather" });
}

TEST(GuideJsonTest, ServiceTypesEveryField) {
    Guide::GuideService service;
    service.id = 5;
    service.slug = "1";
    service.name = "1999";
    service.serviceKind = "2";
    service.description = "3";
    service.url = "4";
    service.phone = "2065550100";
    service.addressText = "5";
    service.neighborhoodSlug = "6";
    service.isHealth = false;
    service.freshness = { "open", std::nullopt };

    Json::Value json = GuideServiceToJson(service);
    EXPECT_EQ(json.GetChildren().size(), 12u);
    ExpectStrings(json, { "slug", "name", "service_kind", "description", "url", "phone",
                          "address_text", "neighborhood_slug", "status" });
    EXPECT_EQ(json["phone"].Get<std::string>(), "2065550100");
    EXPECT_EQ(json["id"].Get<int64_t>(), 5);
    EXPECT_EQ(json["is_health"].Get<bool>(), false);
}

TEST(GuideJsonTest, PageTypesEveryField) {
    Guide::GuidePage page;
    page.id = 6;
    page.slug = "1";
    page.title = "1999";
    page.section = "2";
    page.bodyMarkdown = "3";
    page.adult = false;
    page.draft = true;
    page.published = false;
    page.sceneTagSlugs = { "leather" };
    page.freshness = { "unverified", std::nullopt };

    Json::Value json = GuidePageToJson(page);
    EXPECT_EQ(json.GetChildren().size(), 12u);
    ExpectStrings(json, { "slug", "title", "section", "body_markdown", "status" });
    EXPECT_EQ(json["id"].Get<int64_t>(), 6);
    EXPECT_EQ(json["adult"].Get<bool>(), false);
    EXPECT_EQ(json["draft"].Get<bool>(), true);
    EXPECT_EQ(json["published"].Get<bool>(), false);
    EXPECT_EQ(json["publish_at_us"].Get<std::string>(), "");  // unscheduled
    EXPECT_EQ(Strings(json["scene_tag_slugs"]), std::vector<std::string>{ "leather" });

    page.publishAtUs = 1700000000000000LL;
    EXPECT_EQ(GuidePageToJson(page)["publish_at_us"].Get<int64_t>(), 1700000000000000LL);
}

TEST(GuideJsonTest, AnAbsentPageIsNull) {
    EXPECT_TRUE(OptionalGuidePageToJson(std::nullopt).IsNull());
    Guide::GuidePage page;
    page.slug = "leather";
    EXPECT_EQ(OptionalGuidePageToJson(page)["slug"].Get<std::string>(), "leather");
}

TEST(GuideJsonTest, PageSummaryAndLinkTypeEveryField) {
    Guide::GuidePageSummary summary;
    summary.id = 9;
    summary.slug = "1";
    summary.title = "1999";
    summary.section = "2";
    summary.adult = true;
    summary.publishAtUs = 1700000000000000LL;
    summary.freshness = { "open", std::nullopt };
    const Json::Value pages = GuidePageSummariesToJson({ summary });
    const Json::Value& page = pages.GetArray().at(0);
    EXPECT_EQ(page.GetChildren().size(), 8u);
    ExpectStrings(page, { "slug", "title", "section", "status" });
    EXPECT_FALSE(page.HasChild("body_markdown", nullptr));
    EXPECT_EQ(page["adult"].Get<bool>(), true);
    EXPECT_EQ(page["publish_at_us"].Get<int64_t>(), 1700000000000000LL);

    const Json::Value links = GuideLinksToJson({ Guide::GuideLink{ 2, "1999", "3", "4" } });
    const Json::Value& link = links.GetArray().at(0);
    EXPECT_EQ(link.GetChildren().size(), 4u);
    ExpectStrings(link, { "label", "url", "platform" });
    EXPECT_EQ(link["id"].Get<int64_t>(), 2);
}

TEST(GuideJsonTest, SummariesTypeEveryField) {
    Guide::SceneSummary scene{ "1", "1999", "2", "3", 4, 5, 6 };
    Json::Value sceneJson = SceneSummaryToJson(scene);
    EXPECT_EQ(sceneJson.GetChildren().size(), 7u);
    ExpectStrings(sceneJson, { "slug", "display_name", "description", "coverage" });
    EXPECT_EQ(sceneJson["venue_count"].Get<int64_t>(), 4);
    EXPECT_EQ(sceneJson["organization_count"].Get<int64_t>(), 5);
    EXPECT_EQ(sceneJson["place_count"].Get<int64_t>(), 6);

    Guide::NeighborhoodSummary neighborhood{ "1", "1999", "2", 3, 4, 5 };
    Json::Value neighborhoodJson = NeighborhoodSummaryToJson(neighborhood);
    EXPECT_EQ(neighborhoodJson.GetChildren().size(), 6u);
    ExpectStrings(neighborhoodJson, { "slug", "display_name", "region" });
    EXPECT_EQ(neighborhoodJson["venue_count"].Get<int64_t>(), 3);
    EXPECT_EQ(neighborhoodJson["organization_count"].Get<int64_t>(), 4);
    EXPECT_EQ(neighborhoodJson["place_count"].Get<int64_t>(), 5);
}

TEST(GuideJsonTest, ArraysKeepOrder) {
    Guide::GuideVenue first;
    first.slug = "b-venue";
    Guide::GuideVenue second;
    second.slug = "a-venue";
    const Json::Value venues = GuideVenuesToJson({ first, second });
    ASSERT_EQ(venues.GetArray().size(), 2u);
    EXPECT_EQ(venues.GetArray()[0]["slug"].Get<std::string>(), "b-venue");
    EXPECT_EQ(venues.GetArray()[1]["slug"].Get<std::string>(), "a-venue");
}

}  // namespace
}  // namespace Endpoints
