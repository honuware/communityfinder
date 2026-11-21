#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// CM3 Phase 2 §9 — the curator's event finder behind the admin Edit Event tab,
// gated on `manage_events`. It lists every status, so a curator can find
// pending and rejected events too.
//
// GET /api/admin/events/search?q=<text>&limit=<n>
//   - q: the title text to look for (trimmed). Without it, the next events
//     that aren't over yet: the "Coming up" list.
//   - limit: 10 by default, at most 50.
// Responds {"items": [{"id", "title", "starts_at_us", "ends_at_us",
// "venue_name", "location_text", "series_title", "status"}]}, in the finder's
// order (EventHelper::FindForCurators). `ends_at_us` is a real null when the
// event has no end; the texts are "" when unset and always strings.
Json::Value GetAdminEventSearch(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
