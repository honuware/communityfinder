#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The organization endpoints (GD2 §4.4), both anonymous. The URL says `orgs`,
// matching GD3's /guide/orgs/<slug> pages.

// GET /api/guide/orgs?kind=&scene=&neighborhood= — the organization directory,
// by name: active organizations only (closed and moved ones are in the
// graveyard). An unknown kind, scene or neighborhood matches nothing.
// Responds {"items": […]}.
Json::Value GetGuideOrganizations(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/guide/orgs/<string> — one organization by slug, any status, with
// its links. Responds {"organization": {…}, "links": […]}; 404 for an unknown
// slug.
Json::Value GetGuideOrganizationDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

}  // namespace Endpoints
