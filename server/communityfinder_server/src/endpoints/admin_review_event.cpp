#include "admin_review_event.h"

#include <string>

#include "business_logic/auth/session.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

constexpr std::string_view kActionApprove = "approve";
constexpr std::string_view kActionReject = "reject";

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

        Json::Value body;
        try {
            body = Json::Value::FromText(req.body);
        }
        catch (const std::exception&) {
            resp = ErrorResponse::BadRequest("Request body must be JSON");
            return;
        }
        const Json::Value* actionValue = nullptr;
        const std::string* action = nullptr;
        if (!body.HasChild("action", &actionValue)
            || (action = actionValue->TryGet<std::string>()) == nullptr
            || (*action != kActionApprove && *action != kActionReject)) {
            resp = ErrorResponse::ValidationError(
                "action must be \"approve\" or \"reject\"", "action");
            return;
        }
        std::string notes;
        const Json::Value* notesValue = nullptr;
        if (body.HasChild("notes", &notesValue) && !notesValue->IsNull()) {
            const std::string* text = notesValue->TryGet<std::string>();
            if (text == nullptr) {
                resp = ErrorResponse::ValidationError("notes must be a string", "notes");
                return;
            }
            notes = *text;
        }

        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::ReviewResult review = helper.ReviewEvent(
            transaction,
            eventId,
            *action == kActionApprove ? Events::ReviewAction::Approve
                                      : Events::ReviewAction::Reject,
            endpointAuthHelper.GetSession().GetPersonId(),
            notes);
        if (!review.ok) {
            if (review.errorCode == Events::kEventErrorNotFound) {
                resp = ErrorResponse::NotFound(review.errorCode);
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

        result = Json::Value(Json::JsonObject{
            { "event_id", Json::Value(eventId) },
            { "status", Json::Value(review.status) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
