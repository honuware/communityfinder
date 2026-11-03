#include "admin_review_submission.h"

#include <optional>
#include <string>

#include "business_logic/auth/session.h"
#include "business_logic/events/submission_helper.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/review_request_body.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp,
                int64_t submissionId) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostAdminReviewSubmission(endpointAuthHelper, req, resp, submissionId);
        if (resp.code == 200) {
            resp.set_header("Content-Type", "application/json");
            resp.write(result.ToString());
        }
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/admin/review_submission/<int>")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp, int64_t submissionId) {
                    HandlePost(webApp, req, resp, submissionId);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminReviewSubmission(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t submissionId) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        if (!endpointAuthHelper.RequirePermission(
                transaction, DbSchema::kPermissionManageEvents, resp)) {
            return;
        }

        std::optional<ReviewRequestBody> body = ReadReviewRequestBody(req, resp);
        if (!body) {
            return;
        }

        Events::SubmissionHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::SubmissionReviewResult reviewed = helper.ReviewSubmission(
            transaction,
            submissionId,
            body->action,
            endpointAuthHelper.GetSession().GetPersonId(),
            body->notes,
            body->decision);
        if (!reviewed.ok) {
            if (reviewed.errorCode == Events::kSubmissionErrorNotFound) {
                resp = ErrorResponse::NotFound(reviewed.errorCode);
            } else if (IsDecisionError(reviewed.errorCode)) {
                resp = DecisionErrorResponse(reviewed.errorCode);
            } else {
                resp = ErrorResponse::Create(
                    ErrorCodes::kValidationError,
                    ErrorCodes::GetTitleForType(ErrorCodes::kValidationError),
                    ErrorCodes::GetStatusForType(ErrorCodes::kValidationError),
                    "Nothing in this submission is waiting for review",
                    { { "error_code", Json::Value(reviewed.errorCode) } });
            }
            return;
        }

        bool emailed = false;
        if (reviewed.notice) {
            emailed = helper.SendDecisionNotice(
                transaction, *endpointAuthHelper.GetSecretsHelper(),
                endpointAuthHelper.GetMailHelper(), *reviewed.notice);
        }

        Json::JsonArray decided;
        for (int64_t eventId : reviewed.decidedEventIds) {
            decided.push_back(Json::Value(eventId));
        }
        result = Json::Value(Json::JsonObject{
            { "submission_id", Json::Value(submissionId) },
            { "status", Json::Value(std::string(body->action == Events::ReviewAction::Approve
                                                    ? DbSchema::kEventsStatusApproved
                                                    : DbSchema::kEventsStatusRejected)) },
            { "decided_event_ids", Json::Value(decided) },
            { "decision_emailed", Json::Value(emailed) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
