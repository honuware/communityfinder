#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The neighborhood endpoints (GD2 §4.7), both anonymous.

// GET /api/guide/neighborhoods — every enabled neighborhood in display order,
// with the counts of what its page lists. Responds {"items": […]}.
Json::Value GetGuideNeighborhoods(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/guide/neighborhoods/<string> — a neighborhood page: its active
// venues, organizations and places, and its rundown (the published
// `new-to-seattle` page with the neighborhood's slug, or null). Responds
// {"neighborhood": {…}, "venues": […], "organizations": […], "places": […],
// "rundown_page": {…} | null}; 404 for an unknown or disabled slug.
Json::Value GetGuideNeighborhoodDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

}  // namespace Endpoints
