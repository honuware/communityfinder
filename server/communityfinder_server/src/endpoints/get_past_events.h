#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/events/past?offset&limit&category&scene&neighborhood&venue_id&series_id
// (EV4 §10.2) — the events list's "Past events" view. Anonymous. Public events
// that started before now and ended inside the past-events window
// (archived_events_visible_days), newest first. `limit` defaults to 20 and is
// capped at 50; `offset` counts matching events to skip. The filters are the
// upcoming list's. Responds {"items": […], "has_more", "since_us"}: `since_us`
// is the window's start, the earliest end a listed event can have.
Json::Value GetPastEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
