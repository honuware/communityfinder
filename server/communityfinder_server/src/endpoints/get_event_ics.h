#pragma once

#include <cstdint>

#include <crow.h>

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/events/<int>/ical (EV2 §1.3) — one approved event as a calendar
// file to download ("Add to calendar"). Anything that isn't approved is 404,
// the same answer as a missing id, for everyone: unlike the detail page there
// is no curator preview, because a calendar file is only for public events.
void GetEventICal(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t eventId);

}  // namespace Endpoints
