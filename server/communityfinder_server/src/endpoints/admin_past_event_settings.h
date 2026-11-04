#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// EV4 §10 — the past-events settings on the admin events settings page, gated
// on `manage_events` like the submission limits beside them. Both routes
// respond with the window's value, default and allowed range, and the switch:
//   {"archived_events_visible_days": {"value", "default", "min", "max"},
//    "events_show_past_by_default": {"value", "default"}}

// GET /api/admin/past_event_settings
Json::Value GetAdminPastEventSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// PUT /api/admin/past_event_settings — body {"archived_events_visible_days"?,
// "events_show_past_by_default"?}; only the settings present are written. A
// wrong type is a 400 naming the `field`; an out-of-range window is a 400 with
// `error_code` DAYS_OUT_OF_RANGE and the `field`. Either way nothing is written.
Json::Value PutAdminPastEventSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
