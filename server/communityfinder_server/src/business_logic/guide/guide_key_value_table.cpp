#include "business_logic/guide/guide_key_value_table.h"

#include <optional>
#include <string>

#include "util/types.h"

namespace Guide {

namespace {

std::string OptionalIntString(const std::optional<int64_t>& value) {
    return value ? StringFromInt(*value) : "";
}

std::string BoolString(bool value) {
    return value ? "true" : "false";
}

std::string JoinSlugs(const std::vector<std::string>& slugs) {
    std::string joined;
    for (const std::string& slug : slugs) {
        if (!joined.empty()) {
            joined += ",";
        }
        joined += slug;
    }
    return joined;
}

void AddFreshness(KeyValueTable& table, const Freshness& freshness) {
    table["status"] = freshness.status;
    table["last_verified_at_us"] = OptionalIntString(freshness.lastVerifiedAtUs);
}

template <class Item, class Convert>
KeyValueTableArray ToArray(const std::vector<Item>& items, Convert convert) {
    KeyValueTableArray array;
    array.reserve(items.size());
    for (const Item& item : items) {
        array.push_back(convert(item));
    }
    return array;
}

}  // namespace

KeyValueTable GuideVenueToKeyValueTable(const GuideVenue& venue) {
    KeyValueTable table;
    table["venue_id"] = StringFromInt(venue.venueId);
    table["profile_id"] = StringFromInt(venue.profileId);
    table["slug"] = venue.slug;
    table["name"] = venue.name;
    table["description"] = venue.description;
    table["address"] = venue.address;
    table["url"] = venue.url;
    table["neighborhood_slug"] = venue.neighborhoodSlug;
    table["crowd_vibe"] = venue.crowdVibe;
    table["price_band"] = venue.priceBand;
    table["adult"] = BoolString(venue.adult);
    table["closure_note"] = venue.closureNote;
    table["scene_tag_slugs"] = JoinSlugs(venue.sceneTagSlugs);
    AddFreshness(table, venue.freshness);
    table["venue_last_verified_at_us"] = OptionalIntString(venue.venueLastVerifiedAtUs);
    table["profile_last_verified_at_us"] = OptionalIntString(venue.profileLastVerifiedAtUs);
    return table;
}

KeyValueTableArray GuideVenuesToKeyValueTableArray(const std::vector<GuideVenue>& venues) {
    return ToArray(venues, GuideVenueToKeyValueTable);
}

KeyValueTable GuideOrganizationToKeyValueTable(const GuideOrganization& organization) {
    KeyValueTable table;
    table["id"] = StringFromInt(organization.id);
    table["slug"] = organization.slug;
    table["name"] = organization.name;
    table["org_kind"] = organization.orgKind;
    table["description"] = organization.description;
    table["url"] = organization.url;
    table["meeting_cadence_text"] = organization.meetingCadenceText;
    table["neighborhood_slug"] = organization.neighborhoodSlug;
    table["closure_note"] = organization.closureNote;
    table["scene_tag_slugs"] = JoinSlugs(organization.sceneTagSlugs);
    AddFreshness(table, organization.freshness);
    return table;
}

KeyValueTableArray GuideOrganizationsToKeyValueTableArray(
    const std::vector<GuideOrganization>& organizations) {
    return ToArray(organizations, GuideOrganizationToKeyValueTable);
}

KeyValueTable GuidePlaceToKeyValueTable(const GuidePlace& place) {
    KeyValueTable table;
    table["id"] = StringFromInt(place.id);
    table["slug"] = place.slug;
    table["name"] = place.name;
    table["place_kind"] = place.placeKind;
    table["description"] = place.description;
    table["address_text"] = place.addressText;
    table["neighborhood_slug"] = place.neighborhoodSlug;
    table["adult"] = BoolString(place.adult);
    table["scene_tag_slugs"] = JoinSlugs(place.sceneTagSlugs);
    AddFreshness(table, place.freshness);
    return table;
}

KeyValueTableArray GuidePlacesToKeyValueTableArray(const std::vector<GuidePlace>& places) {
    return ToArray(places, GuidePlaceToKeyValueTable);
}

KeyValueTable GuideServiceToKeyValueTable(const GuideService& service) {
    KeyValueTable table;
    table["id"] = StringFromInt(service.id);
    table["slug"] = service.slug;
    table["name"] = service.name;
    table["service_kind"] = service.serviceKind;
    table["description"] = service.description;
    table["url"] = service.url;
    table["phone"] = service.phone;
    table["address_text"] = service.addressText;
    table["neighborhood_slug"] = service.neighborhoodSlug;
    table["is_health"] = BoolString(service.isHealth);
    AddFreshness(table, service.freshness);
    return table;
}

KeyValueTableArray GuideServicesToKeyValueTableArray(const std::vector<GuideService>& services) {
    return ToArray(services, GuideServiceToKeyValueTable);
}

KeyValueTable GuidePageToKeyValueTable(const GuidePage& page) {
    KeyValueTable table;
    table["id"] = StringFromInt(page.id);
    table["slug"] = page.slug;
    table["title"] = page.title;
    table["section"] = page.section;
    table["body_markdown"] = page.bodyMarkdown;
    table["adult"] = BoolString(page.adult);
    table["draft"] = BoolString(page.draft);
    table["publish_at_us"] = OptionalIntString(page.publishAtUs);
    table["published"] = BoolString(page.published);
    table["scene_tag_slugs"] = JoinSlugs(page.sceneTagSlugs);
    AddFreshness(table, page.freshness);
    return table;
}

KeyValueTable GuidePageSummaryToKeyValueTable(const GuidePageSummary& page) {
    KeyValueTable table;
    table["id"] = StringFromInt(page.id);
    table["slug"] = page.slug;
    table["title"] = page.title;
    table["section"] = page.section;
    table["adult"] = BoolString(page.adult);
    table["publish_at_us"] = OptionalIntString(page.publishAtUs);
    AddFreshness(table, page.freshness);
    return table;
}

KeyValueTableArray GuidePageSummariesToKeyValueTableArray(
    const std::vector<GuidePageSummary>& pages) {
    return ToArray(pages, GuidePageSummaryToKeyValueTable);
}

KeyValueTable GuideLinkToKeyValueTable(const GuideLink& link) {
    KeyValueTable table;
    table["id"] = StringFromInt(link.id);
    table["label"] = link.label;
    table["url"] = link.url;
    table["platform"] = link.platform;
    return table;
}

KeyValueTableArray GuideLinksToKeyValueTableArray(const std::vector<GuideLink>& links) {
    return ToArray(links, GuideLinkToKeyValueTable);
}

KeyValueTable SceneSummaryToKeyValueTable(const SceneSummary& scene) {
    KeyValueTable table;
    table["slug"] = scene.slug;
    table["display_name"] = scene.displayName;
    table["description"] = scene.description;
    table["coverage"] = scene.coverage;
    table["venue_count"] = StringFromInt(scene.venueCount);
    table["organization_count"] = StringFromInt(scene.organizationCount);
    table["place_count"] = StringFromInt(scene.placeCount);
    return table;
}

KeyValueTableArray SceneSummariesToKeyValueTableArray(const std::vector<SceneSummary>& scenes) {
    return ToArray(scenes, SceneSummaryToKeyValueTable);
}

KeyValueTable NeighborhoodSummaryToKeyValueTable(const NeighborhoodSummary& neighborhood) {
    KeyValueTable table;
    table["slug"] = neighborhood.slug;
    table["display_name"] = neighborhood.displayName;
    table["region"] = neighborhood.region;
    table["venue_count"] = StringFromInt(neighborhood.venueCount);
    table["organization_count"] = StringFromInt(neighborhood.organizationCount);
    table["place_count"] = StringFromInt(neighborhood.placeCount);
    return table;
}

KeyValueTableArray NeighborhoodSummariesToKeyValueTableArray(
    const std::vector<NeighborhoodSummary>& neighborhoods) {
    return ToArray(neighborhoods, NeighborhoodSummaryToKeyValueTable);
}

}  // namespace Guide
