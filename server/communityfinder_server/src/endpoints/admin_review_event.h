#pragma once

#include <cstdint>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/review_event/<int> (EV1 §5.5) — approve or reject an event.
// Gated on `manage_events`; the signed-in caller is recorded as the reviewer.
// Body: {"action": "approve" | "reject", "notes"?: "…"}. Responds
// {"event_id", "status"}. An unknown event is 404; a transition the review
// rules refuse (re-applying the current status, anything from archived) is a
// 400 validation error with `error_code` INVALID_TRANSITION and the event's
// current `status`.
Json::Value PostAdminReviewEvent(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t eventId);

}  // namespace Endpoints
