#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// EV3 §4.5 (OQ3) / §4.6 (OQ10) — the Submission limits page's endpoint, gated
// on `manage_events` (OQ9). Both routes respond with each limit's current
// value, default and allowed range, and the pause switch, so the page never
// hard-codes them:
//   {"max_pending_dates": {"value", "default", "min", "max"},
//    "max_submissions_per_day": {…},
//    "submissions_paused": {"value", "default"}}

// GET /api/admin/event_submission_settings
Json::Value GetAdminEventSubmissionSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// PUT /api/admin/event_submission_settings — body {"max_pending_dates"?,
// "max_submissions_per_day"?, "submissions_paused"?}; only the settings present
// are written (the switch alone is fine). A wrong type is a 400 naming the
// `field`; an out-of-range limit is a 400 with `error_code` LIMIT_OUT_OF_RANGE
// and the `field`. Either way nothing is written.
Json::Value PutAdminEventSubmissionSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
