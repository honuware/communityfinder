#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/events/upcoming?from_us&to_us&category&scene&neighborhood&venue_id&series_id
// (EV1 §5.1) — the public event list. Anonymous. Every parameter is optional;
// an unparseable number counts as absent. Approved events starting in the
// clamped window (default now → +42 days, at most 92, `from_us` back one day at
// most), in start order. Category and scene filters match the inherited
// (effective) slugs. Responds {"items": […], "from_us", "to_us"} — the window
// actually served, so a calendar can ask for the next one.
Json::Value GetUpcomingEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
