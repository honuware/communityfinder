#include "business_logic/events/events_key_value_table.h"

#include <optional>
#include <string>

#include "util/types.h"

namespace Events {

namespace {

std::string OptionalIdString(const std::optional<int64_t>& value) {
    return value ? StringFromInt(*value) : "";
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

}  // namespace

KeyValueTable PublicEventToKeyValueTable(const PublicEvent& event) {
    KeyValueTable table;
    table["id"] = StringFromInt(event.id);
    table["title"] = event.title;
    table["description"] = event.description;
    table["starts_at_us"] = StringFromInt(event.startsAtUs);
    table["ends_at_us"] = OptionalIdString(event.endsAtUs);
    table["venue_id"] = OptionalIdString(event.venueId);
    table["venue_name"] = event.venueName;
    table["venue_address"] = event.venueAddress;
    table["neighborhood_slug"] = event.neighborhoodSlug;
    table["location_text"] = event.locationText;
    table["url"] = event.url;
    table["cost_text"] = event.costText;
    table["category_slugs"] = JoinSlugs(event.categorySlugs);
    table["scene_tag_slugs"] = JoinSlugs(event.sceneTagSlugs);
    table["series_id"] = OptionalIdString(event.seriesId);
    table["series_slug"] = event.seriesSlug;
    table["series_title"] = event.seriesTitle;
    table["origin"] = event.origin;
    return table;
}

KeyValueTableArray PublicEventsToKeyValueTableArray(const std::vector<PublicEvent>& events) {
    KeyValueTableArray array;
    array.reserve(events.size());
    for (const auto& event : events) {
        array.push_back(PublicEventToKeyValueTable(event));
    }
    return array;
}

KeyValueTable PublicSeriesToKeyValueTable(const PublicSeries& series) {
    KeyValueTable table;
    table["id"] = StringFromInt(series.id);
    table["title"] = series.title;
    table["slug"] = series.slug;
    table["description"] = series.description;
    table["cadence_text"] = series.cadenceText;
    table["url"] = series.url;
    table["venue_id"] = OptionalIdString(series.venueId);
    table["venue_name"] = series.venueName;
    table["neighborhood_slug"] = series.neighborhoodSlug;
    table["status"] = series.status;
    table["last_verified_at_us"] = OptionalIdString(series.lastVerifiedAtUs);
    table["category_slugs"] = JoinSlugs(series.categorySlugs);
    table["scene_tag_slugs"] = JoinSlugs(series.sceneTagSlugs);
    return table;
}

KeyValueTableArray PublicSeriesListToKeyValueTableArray(
    const std::vector<PublicSeries>& seriesList) {
    KeyValueTableArray array;
    array.reserve(seriesList.size());
    for (const auto& series : seriesList) {
        array.push_back(PublicSeriesToKeyValueTable(series));
    }
    return array;
}

}  // namespace Events
