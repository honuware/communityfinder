#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <crow.h>

#include "business_logic/events/submission_helper.h"

// The body the two review endpoints share (admin_review_event's one date,
// admin_review_submission's whole submission): {"action": "approve" |
// "reject", "notes"?, "reason"?, "message_to_submitter"?}. Not an endpoint —
// no route, no anchor; it links in with the endpoints that use it.
namespace Endpoints {

struct ReviewRequestBody {
    Events::ReviewAction action = Events::ReviewAction::Approve;
    std::string notes;  // the curator-only review note
    Events::SubmissionDecision decision;
};

// Checks the body's shape and types. On a problem, writes the 400 to `resp`
// and returns nullopt. Whether a reason fits the action is SubmissionHelper's
// call (BAD_REASON); see DecisionErrorResponse.
std::optional<ReviewRequestBody> ReadReviewRequestBody(const crow::request& req,
                                                       crow::response& resp);

// The 400 for a decision SubmissionHelper refused: BAD_REASON names `reason`,
// FIELD_TOO_LONG names `message_to_submitter`.
crow::response DecisionErrorResponse(std::string_view errorCode);

// Whether SubmissionHelper refused the decision itself (rather than the review).
bool IsDecisionError(std::string_view errorCode);

}  // namespace Endpoints
