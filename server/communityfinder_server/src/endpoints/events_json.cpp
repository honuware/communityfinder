#include "events_json.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "business_logic/events/events_key_value_table.h"
#include "sql_util/json/key_value_table_json.h"

namespace Endpoints {
namespace {

// The free-text fields of each shape — the ones that must stay strings.
constexpr std::string_view kEventTextKeys[] = {
    "title", "description", "venue_name", "venue_address", "neighborhood_slug",
    "location_text", "url", "cost_text", "series_slug", "series_title", "origin",
};
constexpr std::string_view kSeriesTextKeys[] = {
    "title", "slug", "description", "cadence_text", "url", "venue_name",
    "neighborhood_slug", "status",
};

// KeyValueTableToJson, then each of `textKeys` put back as the string it was.
template <size_t KeyCount>
Json::JsonObject ToJsonObject(const KeyValueTable& table,
                              const std::string_view (&textKeys)[KeyCount]) {
    Json::JsonObject object = SqlUtil::KeyValueTableToJson(table).GetChildren();
    for (std::string_view key : textKeys) {
        auto it = table.find(std::string(key));
        if (it != table.end()) {
            object[std::string(key)] = Json::Value(it->second);
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

// CM3: {"table": …, "id": N}, or null.
Json::Value PhotoJson(const std::optional<Events::PhotoRef>& photo) {
    if (!photo) {
        return Json::Value(nullptr);
    }
    return Json::Value(Json::JsonObject{
        { "table", Json::Value(photo->table) },
        { "id", Json::Value(photo->id) },
    });
}

// CM3: {"provider": …, "id": "…"}, or null. Only the id: the client builds the
// player's address from it, never from a stored link.
Json::Value VideoJson(const std::optional<Events::VideoLink>& video) {
    if (!video) {
        return Json::Value(nullptr);
    }
    return Json::Value(Json::JsonObject{
        { "provider", Json::Value(video->provider) },
        { "id", Json::Value(video->id) },
    });
}

}  // namespace

Json::Value PublicEventToJson(const Events::PublicEvent& event) {
    Json::JsonObject object =
        ToJsonObject(Events::PublicEventToKeyValueTable(event), kEventTextKeys);
    object["category_slugs"] = SlugArray(event.categorySlugs);
    object["scene_tag_slugs"] = SlugArray(event.sceneTagSlugs);
    object["photo"] = PhotoJson(event.photo);
    object["video"] = VideoJson(event.video);
    return Json::Value(object);
}

Json::Value PublicEventsToJson(const std::vector<Events::PublicEvent>& events) {
    Json::JsonArray array;
    array.reserve(events.size());
    for (const Events::PublicEvent& event : events) {
        array.push_back(PublicEventToJson(event));
    }
    return Json::Value(array);
}

Json::Value PublicSeriesToJson(const Events::PublicSeries& series) {
    Json::JsonObject object =
        ToJsonObject(Events::PublicSeriesToKeyValueTable(series), kSeriesTextKeys);
    object["category_slugs"] = SlugArray(series.categorySlugs);
    object["scene_tag_slugs"] = SlugArray(series.sceneTagSlugs);
    object["photo"] = PhotoJson(series.photo);
    object["video"] = VideoJson(series.video);
    return Json::Value(object);
}

Json::Value PublicSeriesListToJson(const std::vector<Events::PublicSeries>& seriesList) {
    Json::JsonArray array;
    array.reserve(seriesList.size());
    for (const Events::PublicSeries& series : seriesList) {
        array.push_back(PublicSeriesToJson(series));
    }
    return Json::Value(array);
}

Json::Value GalleryToJson(const std::vector<Events::GalleryPhoto>& gallery) {
    Json::JsonArray array;
    array.reserve(gallery.size());
    for (const Events::GalleryPhoto& photo : gallery) {
        array.push_back(Json::Value(Json::JsonObject{
            { "id", Json::Value(photo.id) },
            { "alt_text", Json::Value(photo.altText) },
            { "caption", Json::Value(photo.caption) },
            { "credit", Json::Value(photo.credit) },
        }));
    }
    return Json::Value(array);
}

}  // namespace Endpoints
