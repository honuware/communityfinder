#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// CM3 OQ5 — the gallery cap on the admin events settings page, gated on
// `manage_events` like the settings beside it. Both routes respond with the
// cap's value, default and allowed range:
//   {"event_gallery_max_photos": {"value", "default", "min", "max"}}

// GET /api/admin/event_gallery_settings
Json::Value GetAdminEventGallerySettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// PUT /api/admin/event_gallery_settings — body {"event_gallery_max_photos"}. A
// wrong type is a 400 naming the `field`; an out-of-range cap is a 400 with
// `error_code` MAX_PHOTOS_OUT_OF_RANGE and the `field`. Either way nothing is
// written.
Json::Value PutAdminEventGallerySettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
