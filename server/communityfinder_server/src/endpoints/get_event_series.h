#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The series endpoints (EV1 §5.3), both anonymous. Their own top-level path, so
// a series slug can never be confused with /api/events/<int> (EV1 OQ4).

// GET /api/event_series — the series index: every active series (all but
// closed and moved), by title, each with its own category and scene-tag slugs.
// Responds {"items": […]}.
Json::Value GetEventSeriesList(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/event_series/<string> — one series by slug, any status (a closed
// series keeps its page), plus its approved instances in the default window.
// Responds {"series": {…}, "upcoming": […]}; 404 for an unknown slug.
Json::Value GetEventSeriesDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

}  // namespace Endpoints
