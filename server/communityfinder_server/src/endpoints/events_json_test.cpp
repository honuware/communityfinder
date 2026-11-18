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

// CM3: a photo and a video are objects, and none is a real null.
TEST(EventsJsonTest, PhotoAndVideoAreObjectsOrNull) {
    Events::PublicEvent event = MakeEvent();
    Json::Value bare = PublicEventToJson(event);
    EXPECT_TRUE(bare["photo"].IsNull());
    EXPECT_TRUE(bare["video"].IsNull());

    event.photo = Events::PhotoRef{ "series", 9 };
    event.video = Events::VideoLink{ "youtube", "dQw4w9WgXcQ",
                                     "https://www.youtube.com/watch?v=dQw4w9WgXcQ" };
    Json::Value json = PublicEventToJson(event);
    EXPECT_EQ(json["photo"]["table"].Get<std::string>(), "series");
    EXPECT_EQ(json["photo"]["id"].Get<int64_t>(), 9);
    EXPECT_EQ(json["video"]["provider"].Get<std::string>(), "youtube");
    EXPECT_EQ(json["video"]["id"].Get<std::string>(), "dQw4w9WgXcQ");
    // Only the id travels; the client builds the player's address.
    EXPECT_FALSE(json["video"].HasChild("canonical_url"));

    Events::PublicSeries series;
    series.id = 3;
    EXPECT_TRUE(PublicSeriesToJson(series)["photo"].IsNull());
    series.photo = Events::PhotoRef{ "venues", 4 };
    series.video = Events::VideoLink{ "vimeo", "76979871", "https://vimeo.com/76979871" };
    Json::Value seriesJson = PublicSeriesToJson(series);
    EXPECT_EQ(seriesJson["photo"]["table"].Get<std::string>(), "venues");
    EXPECT_EQ(seriesJson["video"]["id"].Get<std::string>(), "76979871");
}

TEST(EventsJsonTest, GalleryKeepsOrderAndTextStaysText) {
    Json::Value json = GalleryToJson({
        Events::GalleryPhoto{ 5, "The stage", "2024", "" },
        Events::GalleryPhoto{ 2, "The bar", "", "Sample Photographer" },
    });
    const Json::JsonArray& photos = json.GetArray();
    ASSERT_EQ(photos.size(), 2u);
    EXPECT_EQ(photos[0]["id"].Get<int64_t>(), 5);
    EXPECT_EQ(photos[0]["alt_text"].Get<std::string>(), "The stage");
    EXPECT_EQ(photos[0]["caption"].Get<std::string>(), "2024");
    EXPECT_EQ(photos[1]["credit"].Get<std::string>(), "Sample Photographer");
    EXPECT_TRUE(GalleryToJson({}).GetArray().empty());
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
