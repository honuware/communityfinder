#include "events_json.h"

#include <string>

#include <gtest/gtest.h>

namespace Endpoints {
namespace {

Events::PublicEvent MakeEvent() {
    Events::PublicEvent event;
    event.id = 42;
    event.title = "T4T Dance Party";
    event.startsAtUs = 1793500000000000LL;
    event.venueId = 7;
    event.costText = "$10";
    event.categorySlugs = { "dance", "drag" };
    event.sceneTagSlugs = { "trans" };
    event.origin = "scanned";
    return event;
}

TEST(EventsJsonTest, EventSlugListsAreArraysAndIdsAreNumbers) {
    Json::Value json = PublicEventToJson(MakeEvent());
    EXPECT_EQ(json["id"].Get<int64_t>(), 42);
    EXPECT_EQ(json["starts_at_us"].Get<int64_t>(), 1793500000000000LL);
    EXPECT_EQ(json["venue_id"].Get<int64_t>(), 7);
    const Json::JsonArray& categories = json["category_slugs"].GetArray();
    ASSERT_EQ(categories.size(), 2u);
    EXPECT_EQ(categories[0].Get<std::string>(), "dance");
    EXPECT_EQ(categories[1].Get<std::string>(), "drag");
    EXPECT_EQ(json["scene_tag_slugs"].GetArray().size(), 1u);
    EXPECT_EQ(json["title"].Get<std::string>(), "T4T Dance Party");
}

// KeyValueTableToJson alone would send these as numbers.
TEST(EventsJsonTest, AllDigitTextStaysAString) {
    Events::PublicEvent event = MakeEvent();
    event.title = "1999";
    event.costText = "10";
    event.locationText = "21";
    event.venueName = "2020";
    event.venueAddress = "98122";
    event.seriesTitle = "7";
    Json::Value json = PublicEventToJson(event);
    EXPECT_EQ(json["title"].Get<std::string>(), "1999");
    EXPECT_EQ(json["cost_text"].Get<std::string>(), "10");
    EXPECT_EQ(json["location_text"].Get<std::string>(), "21");
    EXPECT_EQ(json["venue_name"].Get<std::string>(), "2020");
    EXPECT_EQ(json["venue_address"].Get<std::string>(), "98122");
    EXPECT_EQ(json["series_title"].Get<std::string>(), "7");

    Events::PublicSeries series;
    series.id = 3;
    series.title = "54";
    series.slug = "54";
    series.cadenceText = "1";
    Json::Value seriesJson = PublicSeriesToJson(series);
    EXPECT_EQ(seriesJson["id"].Get<int64_t>(), 3);
    EXPECT_EQ(seriesJson["title"].Get<std::string>(), "54");
    EXPECT_EQ(seriesJson["slug"].Get<std::string>(), "54");
    EXPECT_EQ(seriesJson["cadence_text"].Get<std::string>(), "1");
}

TEST(EventsJsonTest, AbsentOptionalsAndEmptyListsSurvive) {
    Events::PublicEvent event = MakeEvent();
    event.venueId.reset();
    event.categorySlugs.clear();
    Json::Value json = PublicEventToJson(event);
    EXPECT_EQ(json["venue_id"].Get<std::string>(), "");
    EXPECT_EQ(json["ends_at_us"].Get<std::string>(), "");
    EXPECT_TRUE(json["category_slugs"].IsArray());
    EXPECT_TRUE(json["category_slugs"].GetArray().empty());
}

TEST(EventsJsonTest, ListsKeepOrder) {
    Events::PublicEvent second = MakeEvent();
    second.id = 43;
    Json::Value events = PublicEventsToJson({ MakeEvent(), second });
    ASSERT_EQ(events.GetArray().size(), 2u);
    EXPECT_EQ(events.GetArray()[1]["id"].Get<int64_t>(), 43);

    Events::PublicSeries first;
    first.id = 1;
    first.sceneTagSlugs = { "bears" };
    Events::PublicSeries other;
    other.id = 2;
    Json::Value seriesList = PublicSeriesListToJson({ first, other });
    ASSERT_EQ(seriesList.GetArray().size(), 2u);
    EXPECT_EQ(seriesList.GetArray()[0]["scene_tag_slugs"].GetArray()[0].Get<std::string>(),
              "bears");
    EXPECT_EQ(seriesList.GetArray()[1]["id"].Get<int64_t>(), 2);
}

}  // namespace
}  // namespace Endpoints
