#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The venue endpoints (GD2 §4.3), both anonymous. A guide venue is a venue
// with a profile: Stream A's venues row joined with its guide profile.

// GET /api/guide/venues?scene=&neighborhood=&include_closed= — the venue
// directory, by name. Closed and moved venues are left out unless
// include_closed is "true" or "1". An unknown scene or neighborhood slug
// matches nothing. Responds {"items": […]}.
Json::Value GetGuideVenues(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/guide/venues/<string> — one venue page by its profile's slug, any
// status (a closed venue's page is its graveyard entry), with its links.
// Responds {"venue": {…}, "links": […]}; 404 for an unknown slug.
Json::Value GetGuideVenueDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

}  // namespace Endpoints
