#include "admin_review_event.h"

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

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp, int64_t eventId) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostAdminReviewEvent(endpointAuthHelper, req, resp, eventId);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/review_event/<int>")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp, int64_t eventId) {
                    HandlePost(webApp, req, resp, eventId);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminReviewEvent(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp,
    int64_t eventId) {
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
        Events::DateReviewResult reviewed = helper.ReviewDate(
            transaction,
            eventId,
            body->action,
            endpointAuthHelper.GetSession().GetPersonId(),
            body->notes,
            body->decision);
        const Events::ReviewResult& review = reviewed.review;
        if (!review.ok) {
            if (review.errorCode == Events::kEventErrorNotFound) {
                resp = ErrorResponse::NotFound(review.errorCode);
            } else if (IsDecisionError(review.errorCode)) {
                resp = DecisionErrorResponse(review.errorCode);
            } else {
                resp = ErrorResponse::Create(
                    ErrorCodes::kValidationError,
                    ErrorCodes::GetTitleForType(ErrorCodes::kValidationError),
                    ErrorCodes::GetStatusForType(ErrorCodes::kValidationError),
                    review.errorCode,
                    {
                        { "error_code", Json::Value(review.errorCode) },
                        { "status", Json::Value(review.status) },
                    });
            }
            return;
        }

        // The last pending date of a submission: its one email. A failed send
        // never fails the review (SendDecisionNotice alerts and retries later).
        bool emailed = false;
        if (reviewed.notice) {
            emailed = helper.SendDecisionNotice(
                transaction, *endpointAuthHelper.GetSecretsHelper(),
                endpointAuthHelper.GetMailHelper(), *reviewed.notice);
        }

        result = Json::Value(Json::JsonObject{
            { "event_id", Json::Value(eventId) },
            { "status", Json::Value(review.status) },
            { "decision_emailed", Json::Value(emailed) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
