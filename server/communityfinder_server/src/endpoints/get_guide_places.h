#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The place endpoints (GD2 §4.5), both anonymous. Every place carries `adult`;
// the server serves 18+ places to everyone and the client gates rendering.

// GET /api/guide/places — every active place, by name. Responds {"items": […]}.
Json::Value GetGuidePlaces(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/guide/places/<string> — one place by slug, any status, with its
// links. Responds {"place": {…}, "links": […]}; 404 for an unknown slug.
Json::Value GetGuidePlaceDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

}  // namespace Endpoints
