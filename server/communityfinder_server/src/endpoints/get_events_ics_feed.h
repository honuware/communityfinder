#pragma once

#include <crow.h>

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/events/feed.ics (EV2 §1.2) — the public subscribe feed: approved
// events from a day ago to 60 days out as one calendar, for anyone, signed in or
// not. Optional filters `category`, `scene`, `neighborhood` and `series_id` work
// as on /api/events/upcoming, so "subscribe to this view" is the list's own
// query string. Served with the knottyyoga feed headers (refresh hourly).
void GetEventsICalFeed(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
