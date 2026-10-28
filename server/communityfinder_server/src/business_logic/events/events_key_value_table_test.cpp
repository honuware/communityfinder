#include "business_logic/events/events_key_value_table.h"

#include <gtest/gtest.h>

#include "util/types.h"

namespace Events {
namespace {

PublicEvent MakeEvent() {
    PublicEvent event;
    event.id = 42;
    event.title = "T4T Dance Party";
    event.description = "Second Saturdays.";
    event.startsAtUs = 1793500000000000LL;
    event.endsAtUs = 1793510800000000LL;
    event.venueId = 7;
    event.venueName = "The Cuff";
    event.neighborhoodSlug = "capitol-hill";
    event.locationText = "Back patio";
    event.url = "https://example.org/t4t";
    event.costText = "$10 at the door";
    event.categorySlugs = { "dance", "drag" };
    event.sceneTagSlugs = { "trans" };
    event.seriesId = 3;
    event.seriesSlug = "t4t";
    event.seriesTitle = "T4T";
    event.origin = "scanned";
    return event;
}

TEST(EventsKeyValueTableTest, PublicEventConvertsEveryField) {
    KeyValueTable table = PublicEventToKeyValueTable(MakeEvent());
    EXPECT_EQ(table["id"], "42");
    EXPECT_EQ(table["title"], "T4T Dance Party");
    EXPECT_EQ(table["description"], "Second Saturdays.");
    EXPECT_EQ(table["starts_at_us"], "1793500000000000");
    EXPECT_EQ(table["ends_at_us"], "1793510800000000");
    EXPECT_EQ(table["venue_id"], "7");
    EXPECT_EQ(table["venue_name"], "The Cuff");
    EXPECT_EQ(table["neighborhood_slug"], "capitol-hill");
    EXPECT_EQ(table["location_text"], "Back patio");
    EXPECT_EQ(table["url"], "https://example.org/t4t");
    EXPECT_EQ(table["cost_text"], "$10 at the door");
    EXPECT_EQ(table["category_slugs"], "dance,drag");
    EXPECT_EQ(table["scene_tag_slugs"], "trans");
    EXPECT_EQ(table["series_id"], "3");
    EXPECT_EQ(table["series_slug"], "t4t");
    EXPECT_EQ(table["series_title"], "T4T");
    EXPECT_EQ(table["origin"], "scanned");
    EXPECT_EQ(table.size(), 17u);
}

// The public shape has no review fields to leak — pinned so a later "just add
// status" edit has to delete this test first.
TEST(EventsKeyValueTableTest, NeverCarriesReviewOrAuthorFields) {
    KeyValueTable table = PublicEventToKeyValueTable(MakeEvent());
    for (const char* hidden : { "status", "reviewed_by_person_id", "reviewed_us",
                                "review_notes", "created_by_person_id",
                                "source_id", "external_key" }) {
        EXPECT_EQ(table.count(hidden), 0u) << hidden;
    }
}

// A "0" would reach the client as the epoch / as venue 0; empty maps to null.
TEST(EventsKeyValueTableTest, AbsentOptionalsAreEmptyStrings) {
    PublicEvent event = MakeEvent();
    event.endsAtUs.reset();
    event.venueId.reset();
    event.seriesId.reset();
    event.categorySlugs.clear();
    event.sceneTagSlugs.clear();
    KeyValueTable table = PublicEventToKeyValueTable(event);
    EXPECT_EQ(table["ends_at_us"], "");
    EXPECT_EQ(table["venue_id"], "");
    EXPECT_EQ(table["series_id"], "");
    EXPECT_EQ(table["category_slugs"], "");
    EXPECT_EQ(table["scene_tag_slugs"], "");
}

TEST(EventsKeyValueTableTest, ArrayKeepsOrder) {
    PublicEvent first = MakeEvent();
    PublicEvent second = MakeEvent();
    second.id = 43;
    KeyValueTableArray array = PublicEventsToKeyValueTableArray({ first, second });
    ASSERT_EQ(array.size(), 2u);
    EXPECT_EQ(array[0]["id"], "42");
    EXPECT_EQ(array[1]["id"], "43");
    EXPECT_TRUE(PublicEventsToKeyValueTableArray({}).empty());
}

}  // namespace
}  // namespace Events
