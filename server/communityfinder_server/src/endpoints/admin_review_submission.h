#pragma once

#include <cstdint>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/review_submission/<int> (EV3 §4.2) — Approve all / Reject all:
// decides every pending date of one submission in a single transaction, and
// leaves its decided dates alone. Gated on `manage_events`. Body: the same as
// review_event's, {"action", "notes"?, "reason"?, "message_to_submitter"?}.
// Responds {"submission_id", "status", "decided_event_ids",
// "decision_emailed"}. An unknown submission is 404; one with nothing pending
// is a 400 (NOTHING_PENDING); a bad reason is a 400 (BAD_REASON). Sends the
// submitter's one decision email, which a failed send never turns into an error.
Json::Value PostAdminReviewSubmission(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t submissionId);

}  // namespace Endpoints
