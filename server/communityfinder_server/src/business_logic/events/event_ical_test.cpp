#include "business_logic/events/event_ical.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_ical_config.h"

namespace Events {
namespace {

constexpr std::string_view kUidDomain = "communityfinder.test";
constexpr std::string_view kSite = "https://cf.example";
constexpr int64_t kStartUs = 1793500000000000LL;
constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

PublicEvent MakeEvent() {
    PublicEvent event;
    event.id = 42;
    event.title = "T4T Dance Party";
    event.description = "Second Saturdays.";
    event.startsAtUs = kStartUs;
    event.endsAtUs = kStartUs + 3 * kUsPerHour;
    event.venueId = 7;
    event.venueName = "The Cuff";
    event.venueAddress = "1533 13th Ave, Seattle, WA 98122";
    event.locationText = "Back patio";
    event.url = "https://example.org/t4t";
    return event;
}

TEST(EventICalTest, MapsEveryField) {
    ICalGenerator::ICalEvent entry = BuildICalEvent(MakeEvent(), kUidDomain, kSite);
    EXPECT_EQ(entry.uid, "event-42@communityfinder.test");
    EXPECT_EQ(entry.title, "T4T Dance Party");
    EXPECT_EQ(entry.startTimeUs, kStartUs);
    EXPECT_EQ(entry.endTimeUs, kStartUs + 3 * kUsPerHour);
    EXPECT_EQ(entry.timezone, "America/Los_Angeles");
    EXPECT_FALSE(entry.floatingLocal);
    // A venue wins over location_text.
    EXPECT_EQ(entry.location, "The Cuff, 1533 13th Ave, Seattle, WA 98122");
    EXPECT_EQ(entry.description,
              "Second Saturdays.\n\nhttps://example.org/t4t\nhttps://cf.example/events/42");
    // Nothing a curator didn't ask for: no organizer, attendee or recurrence.
    EXPECT_EQ(entry.organizerEmail, "");
    EXPECT_EQ(entry.attendeeEmail, "");
    EXPECT_EQ(entry.rrule, "");
}

TEST(EventICalTest, NoEndMeansTwoHours) {
    PublicEvent event = MakeEvent();
    event.endsAtUs.reset();
    ICalGenerator::ICalEvent entry = BuildICalEvent(event, kUidDomain, kSite);
    EXPECT_EQ(entry.endTimeUs, kStartUs + 2 * kUsPerHour);
    EXPECT_EQ(entry.endTimeUs - entry.startTimeUs, kICalDefaultDurationUs);
}

TEST(EventICalTest, LocationFallsBackFromVenueToLocationText) {
    PublicEvent noAddress = MakeEvent();
    noAddress.venueAddress = "";
    EXPECT_EQ(BuildICalEvent(noAddress, kUidDomain, kSite).location, "The Cuff");

    PublicEvent noVenue = MakeEvent();
    noVenue.venueId.reset();
    noVenue.venueName = "";
    noVenue.venueAddress = "";
    EXPECT_EQ(BuildICalEvent(noVenue, kUidDomain, kSite).location, "Back patio");

    noVenue.locationText = "";
    EXPECT_EQ(BuildICalEvent(noVenue, kUidDomain, kSite).location, "");
}

TEST(EventICalTest, DescriptionCarriesWhateverLinksExist) {
    PublicEvent event = MakeEvent();
    event.description = "";
    EXPECT_EQ(BuildICalEvent(event, kUidDomain, kSite).description,
              "https://example.org/t4t\nhttps://cf.example/events/42");

    // No site address configured: no page link.
    EXPECT_EQ(BuildICalEvent(event, kUidDomain, "").description, "https://example.org/t4t");

    event.url = "";
    EXPECT_EQ(BuildICalEvent(event, kUidDomain, "").description, "");

    event.description = "Second Saturdays.";
    EXPECT_EQ(BuildICalEvent(event, kUidDomain, "").description, "Second Saturdays.");
    EXPECT_EQ(BuildICalEvent(event, kUidDomain, kSite).description,
              "Second Saturdays.\n\nhttps://cf.example/events/42");
}

// The UID is what lets a calendar update an entry rather than duplicate it, so
// it depends on the event's id and nothing else.
TEST(EventICalTest, UidIsStablePerEvent) {
    EXPECT_EQ(BuildEventUid(42, kUidDomain), "event-42@communityfinder.test");

    PublicEvent edited = MakeEvent();
    edited.title = "T4T (moved)";
    edited.startsAtUs += 24 * kUsPerHour;
    edited.venueName = "Kremwerk";
    EXPECT_EQ(BuildICalEvent(edited, kUidDomain, kSite).uid,
              BuildICalEvent(MakeEvent(), kUidDomain, kSite).uid);

    PublicEvent other = MakeEvent();
    other.id = 43;
    EXPECT_NE(BuildICalEvent(other, kUidDomain, kSite).uid,
              BuildICalEvent(MakeEvent(), kUidDomain, kSite).uid);
}

TEST(EventICalTest, PageUrlToleratesATrailingSlash) {
    EXPECT_EQ(BuildEventPageUrl("http://localhost:4201/", 9), "http://localhost:4201/events/9");
    EXPECT_EQ(BuildEventPageUrl("http://localhost:4201", 9), "http://localhost:4201/events/9");
    EXPECT_EQ(BuildEventPageUrl("", 9), "");
    EXPECT_EQ(BuildEventPageUrl("/", 9), "");
}

TEST(EventICalTest, ListKeepsOrder) {
    PublicEvent second = MakeEvent();
    second.id = 43;
    std::vector<ICalGenerator::ICalEvent> entries =
        BuildICalEventsFromPublicEvents({ MakeEvent(), second }, kUidDomain, kSite);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].uid, "event-42@communityfinder.test");
    EXPECT_EQ(entries[1].uid, "event-43@communityfinder.test");
    EXPECT_TRUE(BuildICalEventsFromPublicEvents({}, kUidDomain, kSite).empty());
}

// Through honuware's generator with the app's identity: the lines a calendar
// app reads, escaped as RFC 5545 wants.
TEST(EventICalTest, GeneratesACalendarWithTheAppIdentity) {
    std::string ics = ICalGenerator::GenerateICalendar(
        BuildICalEventsFromPublicEvents({ MakeEvent() }, App::kICalUidDomain, ""),
        App::AppICalConfig());
    EXPECT_NE(ics.find("BEGIN:VCALENDAR\r\n"), std::string::npos);
    EXPECT_NE(ics.find("PRODID:-//CommunityFinder//Events//EN\r\n"), std::string::npos);
    EXPECT_NE(ics.find("UID:event-42@communityfinder.local\r\n"), std::string::npos);
    EXPECT_NE(ics.find("SUMMARY:T4T Dance Party\r\n"), std::string::npos);
    EXPECT_NE(ics.find("LOCATION:The Cuff\\, 1533 13th Ave\\, Seattle\\, WA 98122\r\n"),
              std::string::npos);
    EXPECT_NE(ics.find("DESCRIPTION:Second Saturdays.\\n\\nhttps://example.org/t4t\r\n"),
              std::string::npos);
    // 1793500000 s = 2026-11-01 02:26:40 UTC (Oct 31, 7:26 PM in Seattle).
    EXPECT_NE(ics.find("DTSTART:20261101T022640Z\r\n"), std::string::npos);
    EXPECT_NE(ics.find("DTEND:20261101T052640Z\r\n"), std::string::npos);
}

TEST(EventICalTest, FeedCalendarNameListsTheFilters) {
    EXPECT_EQ(BuildFeedCalendarName("CommunityFinder", {}), "CommunityFinder events");
    EXPECT_EQ(BuildFeedCalendarName("CommunityFinder", { "Drag" }), "CommunityFinder: Drag events");
    EXPECT_EQ(BuildFeedCalendarName("CommunityFinder", { "Drag", "Bears", "Capitol Hill" }),
              "CommunityFinder: Drag, Bears, Capitol Hill events");
}

// The name goes out through honuware's generator as X-WR-CALNAME, escaped.
TEST(EventICalTest, NamedFeedCarriesTheCalendarName) {
    ICalGenerator::ICalConfig config = App::AppICalConfig();
    config.calendarName = BuildFeedCalendarName(App::kICalCalendarBrand, { "Drag", "Capitol Hill" });
    std::string ics = ICalGenerator::GenerateICalendar(
        BuildICalEventsFromPublicEvents({ MakeEvent() }, App::kICalUidDomain, ""), config);
    EXPECT_NE(ics.find("X-WR-CALNAME:CommunityFinder: Drag\\, Capitol Hill events\r\n"),
              std::string::npos)
        << ics;
}

}  // namespace
}  // namespace Events
