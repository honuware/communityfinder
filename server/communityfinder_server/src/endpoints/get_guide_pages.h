#pragma once

#include <string>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// The guide-page endpoints (GD2 §4.9).

// GET /api/guide/pages/<string> — one guide page, body included. A published
// page serves to everyone. An unpublished one (a draft, or scheduled for later)
// is 404 — the same answer as a missing slug — unless the caller holds
// `manage_guide`, who gets it as a preview: `published` false says so.
Json::Value GetGuidePage(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    const std::string& slug);

// GET /api/guide/pages?section= — a section's published pages, by title,
// without bodies: GD3's section indexes. Published only, for curators too.
// Responds {"items": […]}; 400 without a section.
Json::Value GetGuidePagesBySection(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
