#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/expire_unreviewed_events (EV4 §5) — the
// expire_unreviewed_events job: pending events whose start has passed become
// rejected ("Date passed before review"), at most Jobs::kEventSweepBatchLimit
// per call, oldest first. When that closes a submission's last pending date,
// its decision email goes out here (a failed send records an admin alert and
// never fails the job). Gated on run_scheduled_jobs; the body is ignored.
// Responds {"expired": n, "decision_emails_sent": n}.
Json::Value PostAdminExpireUnreviewedEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
