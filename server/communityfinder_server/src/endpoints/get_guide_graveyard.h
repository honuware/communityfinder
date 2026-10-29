#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// GET /api/guide/graveyard (GD2 §4.8), anonymous — the closure graveyard:
// closed venues and closed organizations, each with its closure note. Moved
// listings aren't here; they point at their new home. Responds
// {"venues": […], "organizations": […]}.
Json::Value GetGuideGraveyard(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
