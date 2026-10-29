#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The scene endpoints (GD2 §4.1–4.2), both anonymous.

// GET /api/guide/scenes — the scene grid: every enabled scene in display order,
// with its coverage posture and the counts of what its landing lists.
// Responds {"items": […]}.
Json::Value GetGuideScenes(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/guide/scenes/<string> — a scene landing: its active venues,
// organizations and places, its links, and its intro page (the published
// `scenes` page with the scene's slug, or null). Responds {"scene": {…},
// "venues": […], "organizations": […], "places": […], "links": […],
// "intro_page": {…} | null}; 404 for an unknown or disabled slug.
Json::Value GetGuideSceneDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

}  // namespace Endpoints
