#pragma once

#include <cstdint>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/review_event/<int> (EV1 §5.5) — approve or reject an event.
// Gated on `manage_events`; the signed-in caller is recorded as the reviewer.
// Body: {"action": "approve" | "reject", "notes"?: "…", "reason"?: "…",
// "message_to_submitter"?: "…"}. Responds {"event_id", "status",
// "decision_emailed"}. An unknown event is 404; a transition the review rules
// refuse (re-applying the current status, anything from archived) is a 400
// validation error with `error_code` INVALID_TRANSITION and the event's
// current `status`.
//
// EV3 §4.2 — for a submitted date, `reason` (one of the six curator reasons,
// reject only; else 400 BAD_REASON) and `message_to_submitter` are stored on
// its submission, and they are ignored for an event from anywhere else. When
// this decides the submission's last pending date, the submitter's one
// decision email goes out (`decision_emailed`); a failed send still returns
// 200 and leaves an admin alert.
Json::Value PostAdminReviewEvent(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t eventId);

}  // namespace Endpoints
