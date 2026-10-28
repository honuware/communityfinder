#include "business_logic/events/event_ical.h"

#include "util/types.h"

namespace Events {

namespace {

std::string Location(const PublicEvent& event) {
    if (event.venueName.empty()) {
        return event.locationText;
    }
    if (event.venueAddress.empty()) {
        return event.venueName;
    }
    return event.venueName + ", " + event.venueAddress;
}

std::string Description(const PublicEvent& event, std::string_view siteAddress) {
    std::string links;
    for (const std::string& link : { event.url, BuildEventPageUrl(siteAddress, event.id) }) {
        if (link.empty()) {
            continue;
        }
        if (!links.empty()) {
            links += "\n";
        }
        links += link;
    }

    std::string description = event.description;
    if (!description.empty() && !links.empty()) {
        description += "\n\n";
    }
    return description + links;
}

}  // namespace

std::string BuildEventUid(int64_t eventId, std::string_view uidDomain) {
    return "event-" + StringFromInt(eventId) + "@" + std::string(uidDomain);
}

std::string BuildEventPageUrl(std::string_view siteAddress, int64_t eventId) {
    while (!siteAddress.empty() && siteAddress.back() == '/') {
        siteAddress.remove_suffix(1);
    }
    if (siteAddress.empty()) {
        return "";
    }
    return std::string(siteAddress) + "/events/" + StringFromInt(eventId);
}

ICalGenerator::ICalEvent BuildICalEvent(
    const PublicEvent& event, std::string_view uidDomain, std::string_view siteAddress) {
    ICalGenerator::ICalEvent entry;
    entry.uid = BuildEventUid(event.id, uidDomain);
    entry.title = event.title;
    entry.startTimeUs = event.startsAtUs;
    entry.endTimeUs = event.endsAtUs.value_or(event.startsAtUs + kICalDefaultDurationUs);
    entry.timezone = std::string(kEventsTimeZone);
    entry.floatingLocal = false;
    entry.location = Location(event);
    entry.description = Description(event, siteAddress);
    return entry;
}

std::vector<ICalGenerator::ICalEvent> BuildICalEventsFromPublicEvents(
    const std::vector<PublicEvent>& events, std::string_view uidDomain,
    std::string_view siteAddress) {
    std::vector<ICalGenerator::ICalEvent> entries;
    entries.reserve(events.size());
    for (const PublicEvent& event : events) {
        entries.push_back(BuildICalEvent(event, uidDomain, siteAddress));
    }
    return entries;
}

}  // namespace Events
