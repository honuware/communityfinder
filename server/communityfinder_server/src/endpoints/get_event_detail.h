#pragma once

#include <cstdint>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/events/<int> (EV1 §5.2) — one event in the public shape. Anonymous
// callers see approved events only; anything else is 404, the same answer as a
// missing id, so the URL says nothing about pending or rejected events. A caller
// holding `manage_events` sees any status — the reviewer's preview — and gets
// `status` and `review_notes` too.
Json::Value GetEventDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t eventId);

}  // namespace Endpoints
