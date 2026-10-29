#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/guide/services?kind= (GD2 §4.6), anonymous — the active service
// listings of one kind (absent = every kind), by name. Each carries
// `is_health`, the MHMDA rendering flag. Services have no detail page at MVP
// (GD3 §3.6). Responds {"items": […]}.
Json::Value GetGuideServices(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
