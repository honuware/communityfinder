#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/archive_past_events (EV4 §5) — the archive_past_events job:
// approved events that ended more than six hours ago become archived, at most
// Jobs::kEventSweepBatchLimit per call, oldest first. Gated on
// run_scheduled_jobs; the body is ignored. Responds {"archived": n}.
Json::Value PostAdminArchivePastEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
