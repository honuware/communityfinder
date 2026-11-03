#include "review_request_body.h"

#include "util/error_codes.h"
#include "util/error_response.h"
#include "util/json_value.h"

namespace Endpoints {
namespace {

constexpr std::string_view kActionApprove = "approve";
constexpr std::string_view kActionReject = "reject";

}  // namespace

std::optional<ReviewRequestBody> ReadReviewRequestBody(const crow::request& req,
                                                       crow::response& resp) {
    Json::Value body;
    try {
        body = Json::Value::FromText(req.body);
    }
    catch (const std::exception&) {
        resp = ErrorResponse::BadRequest("Request body must be JSON");
        return std::nullopt;
    }

    ReviewRequestBody review;
    const Json::Value* actionValue = nullptr;
    const std::string* action = nullptr;
    if (!body.HasChild("action", &actionValue)
        || (action = actionValue->TryGet<std::string>()) == nullptr
        || (*action != kActionApprove && *action != kActionReject)) {
        resp = ErrorResponse::ValidationError(
            "action must be \"approve\" or \"reject\"", "action");
        return std::nullopt;
    }
    review.action = *action == kActionApprove ? Events::ReviewAction::Approve
                                              : Events::ReviewAction::Reject;

    // Optional strings: absent or null = not given.
    auto readString = [&](std::string_view name, std::optional<std::string>& out) {
        const Json::Value* value = nullptr;
        if (!body.HasChild(name, &value) || value->IsNull()) {
            return true;
        }
        const std::string* text = value->TryGet<std::string>();
        if (text == nullptr) {
            resp = ErrorResponse::ValidationError(std::string(name) + " must be a string", name);
            return false;
        }
        out = *text;
        return true;
    };
    std::optional<std::string> notes;
    std::optional<std::string> reason;
    if (!readString("notes", notes) || !readString("reason", reason)
        || !readString("message_to_submitter", review.decision.messageToSubmitter)) {
        return std::nullopt;
    }
    review.notes = notes.value_or("");
    review.decision.reason = reason.value_or("");
    return review;
}

bool IsDecisionError(std::string_view errorCode) {
    return errorCode == Events::kSubmissionErrorBadReason
        || errorCode == Events::kSubmissionErrorFieldTooLong;
}

crow::response DecisionErrorResponse(std::string_view errorCode) {
    const bool badReason = errorCode == Events::kSubmissionErrorBadReason;
    return ErrorResponse::Create(
        ErrorCodes::kValidationError,
        ErrorCodes::GetTitleForType(ErrorCodes::kValidationError),
        ErrorCodes::GetStatusForType(ErrorCodes::kValidationError),
        badReason ? "reason must be one of the curator reasons, and only with reject"
                  : "message_to_submitter is too long",
        {
            { "error_code", Json::Value(std::string(errorCode)) },
            { "field", Json::Value(badReason ? "reason" : "message_to_submitter") },
        });
}

}  // namespace Endpoints
