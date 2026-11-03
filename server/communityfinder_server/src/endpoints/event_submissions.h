#pragma once

#include <cstdint>

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// EV3 §4.1 / §4.6 — the submitter's side of the public submission pipeline.
// All but the status route need a signed-in caller (401 otherwise). Validation
// failures are 400s with `error_code` and `field` (and `date_index` for a
// date's error).

// POST /api/event_submissions. Body:
//   {"title", "dates": [{"starts_at_us", "ends_at_us"?}], "venue_id"?,
//    "location_text"?, "description"?, "url"?, "cost_text"?,
//    "category_slugs"?: [], "scene_tag_slugs"?: [], "regular_thing"?,
//    "relationship"?, "submitter_note"?, "agreed": true, "website"?}
// `website` is the honeypot: when it is filled in, the answer looks like
// success (submission_id 0) and nothing is written. An unconfirmed email is a
// 403 (EMAIL_NOT_VERIFIED); paused submissions are a 503 (SUBMISSIONS_PAUSED)
// whose `detail` is the notice to show; a rate limit is a 429 (RATE_LIMITED)
// whose `detail` states the limit. Responds {"submission_id", "event_ids",
// "ignored_category_slugs", "ignored_scene_tag_slugs", "possible_duplicates":
// [{"date_index", "event_id", "title", "starts_at_us"}]}.
Json::Value PostEventSubmission(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/event_submissions/mine — the caller's submissions, newest first:
// {"items": [{"id", "created_us", "relationship", "regular_thing",
// "submitter_note", "reject_reason", "message_to_submitter", "dates":
// [{"event_id", "title", "starts_at_us", "ends_at_us", "state"}]}]}. `state` is
// waiting / listed / not_listed / withdrawn / past. No curator notes.
Json::Value GetMyEventSubmissions(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// GET /api/event_submissions/status — public (no sign-in), so the form can
// show the paused notice before anyone fills it in: {"accepting_submissions"}.
Json::Value GetEventSubmissionStatus(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

// POST /api/event_submissions/<int>/withdraw — rejects the submission's pending
// dates as withdrawn. Someone else's submission (or none) is a 404, so ids
// reveal nothing; nothing left pending is a 400 (NOTHING_PENDING). Responds
// {"submission_id", "withdrawn_event_ids"}.
Json::Value PostWithdrawEventSubmission(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t submissionId);

}  // namespace Endpoints
