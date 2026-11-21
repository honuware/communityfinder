#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/photo_removal_requests — CM3 Phase 2 §12.3 (OQ11, OQ19): "Ask us
// to remove a photo", public (no sign-in). Body:
//   {"page_path", "page_title", "photo"?, "message"?, "reply_email"?, "website"?}
// `page_path` is the site-relative page ("/events/12"). `photo` says which
// ("photo 2 of 4 (…)"); without it the message must. The request is emailed to
// the site address and everyone holding manage_events
// (Events::PhotoRemovalRequestHelper); nothing is stored.
//   - `website` is the honeypot: filled in, the answer is the same success and
//     nothing is sent.
//   - A failed check is a 400 with `error_code` (REQUIRED, TOO_LONG, BAD_PAGE,
//     BAD_EMAIL) and `field`.
//   - More than 3 an hour from one visitor (client IP), or 20 an hour across
//     the site, is a 429 (RATE_LIMITED).
//   - When no email could be sent, a 503 (NOT_SENT): the form shows the site's
//     address instead.
// Responds {"sent": true}.
Json::Value PostPhotoRemovalRequest(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
