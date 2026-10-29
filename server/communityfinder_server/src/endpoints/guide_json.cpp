#include "guide_json.h"

#include <span>
#include <string>
#include <string_view>

#include "business_logic/guide/guide_key_value_table.h"
#include "sql_util/json/key_value_table_json.h"

namespace Endpoints {
namespace {

// The free-text fields of each shape — the ones that must stay strings.
constexpr std::string_view kVenueTextKeys[] = {
    "slug", "name", "description", "address", "url", "neighborhood_slug", "crowd_vibe",
    "price_band", "closure_note", "status",
};
constexpr std::string_view kOrganizationTextKeys[] = {
    "slug", "name", "org_kind", "description", "url", "meeting_cadence_text",
    "neighborhood_slug", "closure_note", "status",
};
constexpr std::string_view kPlaceTextKeys[] = {
    "slug", "name", "place_kind", "description", "address_text", "neighborhood_slug", "status",
};
constexpr std::string_view kServiceTextKeys[] = {
    "slug", "name", "service_kind", "description", "url", "phone", "address_text",
    "neighborhood_slug", "status",
};
constexpr std::string_view kPageTextKeys[] = {
    "slug", "title", "section", "body_markdown", "status",
};
constexpr std::string_view kPageSummaryTextKeys[] = {
    "slug", "title", "section", "status",
};
constexpr std::string_view kLinkTextKeys[] = {
    "label", "url", "platform",
};
constexpr std::string_view kSceneTextKeys[] = {
    "slug", "display_name", "description", "coverage",
};
constexpr std::string_view kNeighborhoodTextKeys[] = {
    "slug", "display_name", "region",
};

// The checkboxes of each shape.
constexpr std::string_view kAdultKey[] = { "adult" };
constexpr std::string_view kServiceBoolKeys[] = { "is_health" };
constexpr std::string_view kPageBoolKeys[] = { "adult", "draft", "published" };

// KeyValueTableToJson, then each of `textKeys` put back as the string it was
// and each of `boolKeys` made a real boolean.
Json::JsonObject ToJsonObject(const KeyValueTable& table,
                              std::span<const std::string_view> textKeys,
                              std::span<const std::string_view> boolKeys = {}) {
    Json::JsonObject object = SqlUtil::KeyValueTableToJson(table).GetChildren();
    for (std::string_view key : textKeys) {
        auto it = table.find(std::string(key));
        if (it != table.end()) {
            object[std::string(key)] = Json::Value(it->second);
        }
    }
    for (std::string_view key : boolKeys) {
        auto it = table.find(std::string(key));
        if (it != table.end()) {
            object[std::string(key)] = Json::Value(it->second == "true");
        }
    }
    return object;
}

Json::Value SlugArray(const std::vector<std::string>& slugs) {
    Json::JsonArray array;
    array.reserve(slugs.size());
    for (const std::string& slug : slugs) {
        array.push_back(Json::Value(slug));
    }
    return Json::Value(array);
}

template <class Item, class Convert>
Json::Value ToArray(const std::vector<Item>& items, Convert convert) {
    Json::JsonArray array;
    array.reserve(items.size());
    for (const Item& item : items) {
        array.push_back(convert(item));
    }
    return Json::Value(array);
}

Json::Value GuideLinkToJson(const Guide::GuideLink& link) {
    return Json::Value(ToJsonObject(Guide::GuideLinkToKeyValueTable(link), kLinkTextKeys));
}

Json::Value GuidePageSummaryToJson(const Guide::GuidePageSummary& page) {
    return Json::Value(ToJsonObject(Guide::GuidePageSummaryToKeyValueTable(page),
                                    kPageSummaryTextKeys, kAdultKey));
}

}  // namespace

Json::Value GuideVenueToJson(const Guide::GuideVenue& venue) {
    Json::JsonObject object =
        ToJsonObject(Guide::GuideVenueToKeyValueTable(venue), kVenueTextKeys, kAdultKey);
    object["scene_tag_slugs"] = SlugArray(venue.sceneTagSlugs);
    return Json::Value(object);
}

Json::Value GuideVenuesToJson(const std::vector<Guide::GuideVenue>& venues) {
    return ToArray(venues, GuideVenueToJson);
}

Json::Value GuideOrganizationToJson(const Guide::GuideOrganization& organization) {
    Json::JsonObject object = ToJsonObject(
        Guide::GuideOrganizationToKeyValueTable(organization), kOrganizationTextKeys, kAdultKey);
    object["scene_tag_slugs"] = SlugArray(organization.sceneTagSlugs);
    return Json::Value(object);
}

Json::Value GuideOrganizationsToJson(const std::vector<Guide::GuideOrganization>& organizations) {
    return ToArray(organizations, GuideOrganizationToJson);
}

Json::Value GuidePlaceToJson(const Guide::GuidePlace& place) {
    Json::JsonObject object =
        ToJsonObject(Guide::GuidePlaceToKeyValueTable(place), kPlaceTextKeys, kAdultKey);
    object["scene_tag_slugs"] = SlugArray(place.sceneTagSlugs);
    return Json::Value(object);
}

Json::Value GuidePlacesToJson(const std::vector<Guide::GuidePlace>& places) {
    return ToArray(places, GuidePlaceToJson);
}

Json::Value GuideServiceToJson(const Guide::GuideService& service) {
    return Json::Value(ToJsonObject(Guide::GuideServiceToKeyValueTable(service),
                                    kServiceTextKeys, kServiceBoolKeys));
}

Json::Value GuideServicesToJson(const std::vector<Guide::GuideService>& services) {
    return ToArray(services, GuideServiceToJson);
}

Json::Value GuidePageToJson(const Guide::GuidePage& page) {
    Json::JsonObject object =
        ToJsonObject(Guide::GuidePageToKeyValueTable(page), kPageTextKeys, kPageBoolKeys);
    object["scene_tag_slugs"] = SlugArray(page.sceneTagSlugs);
    return Json::Value(object);
}

Json::Value OptionalGuidePageToJson(const std::optional<Guide::GuidePage>& page) {
    return page ? GuidePageToJson(*page) : Json::Value(nullptr);
}

Json::Value GuidePageSummariesToJson(const std::vector<Guide::GuidePageSummary>& pages) {
    return ToArray(pages, GuidePageSummaryToJson);
}

Json::Value GuideLinksToJson(const std::vector<Guide::GuideLink>& links) {
    return ToArray(links, GuideLinkToJson);
}

Json::Value SceneSummaryToJson(const Guide::SceneSummary& scene) {
    return Json::Value(ToJsonObject(Guide::SceneSummaryToKeyValueTable(scene), kSceneTextKeys));
}

Json::Value SceneSummariesToJson(const std::vector<Guide::SceneSummary>& scenes) {
    return ToArray(scenes, SceneSummaryToJson);
}

Json::Value NeighborhoodSummaryToJson(const Guide::NeighborhoodSummary& neighborhood) {
    return Json::Value(ToJsonObject(Guide::NeighborhoodSummaryToKeyValueTable(neighborhood),
                                    kNeighborhoodTextKeys));
}

Json::Value NeighborhoodSummariesToJson(
    const std::vector<Guide::NeighborhoodSummary>& neighborhoods) {
    return ToArray(neighborhoods, NeighborhoodSummaryToJson);
}

}  // namespace Endpoints
