#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/events/settings (EV4 §10.3) — what the events pages need to know
// before their first read. Anonymous. Responds
//   {"archived_events_visible_days": 365, "events_show_past_by_default": false}
// The calendar starts its "Show past events" switch from the second, for a
// visitor who hasn't chosen, and goes back no further than the first allows.
//
// Not on /api/site_info, as §10.3 planned: an app's site-info block is handed
// only a DatabaseHelper, and these settings are encrypted at rest, so a block
// can't read them. This is one small public read on the events pages only.
Json::Value GetEventSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
