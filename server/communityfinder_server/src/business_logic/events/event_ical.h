#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/event_helper.h"
#include "util/ical_generator.h"

namespace Events {

// EV2 §1.1 — approved events as calendar entries, for the subscribe feed
// (GET /api/events/feed.ics) and the one-event download (GET
// /api/events/<int>/ical). Pure mapping, no database: the endpoints fetch the
// events, these shape them, and ICalGenerator::GenerateICalendar writes the file.

// A calendar entry needs an end; an event without one gets this long.
inline constexpr int64_t kICalDefaultDurationUs = 2LL * 3'600LL * 1'000'000LL;

// Every event is a Seattle-area event (EV2's time pin). DTSTART/DTEND are still
// written as real UTC instants ("Z"), which calendars show in the viewer's zone.
inline constexpr std::string_view kEventsTimeZone = "America/Los_Angeles";

// "event-{id}@{uidDomain}". The same event keeps the same UID in every feed and
// download, so a calendar updates its entry instead of adding a second one.
std::string BuildEventUid(int64_t eventId, std::string_view uidDomain);

// The event's page on the site, "{siteAddress}/events/{id}", or "" when no site
// address is configured.
std::string BuildEventPageUrl(std::string_view siteAddress, int64_t eventId);

// One event as a calendar entry:
//   SUMMARY      the title
//   DTSTART/END  the start, and the end — or start + kICalDefaultDurationUs
//   LOCATION     "venue, address" when the event has a venue, else location_text
//   DESCRIPTION  the description, then the event's own link and its page on the
//                site, each on a line of its own
// An empty `siteAddress` leaves the page link out.
ICalGenerator::ICalEvent BuildICalEvent(
    const PublicEvent& event, std::string_view uidDomain, std::string_view siteAddress);

// BuildICalEvent for each, in order.
std::vector<ICalGenerator::ICalEvent> BuildICalEventsFromPublicEvents(
    const std::vector<PublicEvent>& events, std::string_view uidDomain,
    std::string_view siteAddress);

// The subscribe feed's calendar name (EV2 §1.5), from the names of its filters
// (EventHelper::GetFeedFilterNames): "{brand} events" with none,
// "{brand}: Drag, Capitol Hill events" with some.
std::string BuildFeedCalendarName(
    std::string_view brand, const std::vector<std::string>& filterNames);

}  // namespace Events
