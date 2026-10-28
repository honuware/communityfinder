#include "get_event_detail.h"

#include <optional>

#include "business_logic/auth/session.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/events_json.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// Whether the caller holds manage_events. A quiet check, not RequirePermission:
// lacking it here just means the public view, never a 401/403.
bool HoldsManageEvents(EndpointAuthHelper& endpointAuthHelper, Transaction& transaction) {
    Auth::Session& session = endpointAuthHelper.GetSession();
    if (!session.IsLoggedIn()) {
        return false;
    }
    try {
        return session.ActiveUserHasPermission(transaction, DbSchema::kPermissionManageEvents);
    }
    catch (const std::exception&) {
        // Thrown when the permission isn't seeded. Fail closed, as
        // RequirePermission does.
        return false;
    }
}

void HandleGet(WebApp* webApp, const crow::request& req, crow::response& resp, int64_t eventId) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = GetEventDetail(endpointAuthHelper, req, resp, eventId);
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
        CROW_ROUTE(webApp->GetApp(), "/api/events/<int>")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp, int64_t eventId) {
                    HandleGet(webApp, req, resp, eventId);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetEventDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp,
    int64_t eventId) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        std::optional<Events::EventDetail> detail = helper.GetEventDetail(transaction, eventId);
        const bool reviewer = HoldsManageEvents(endpointAuthHelper, transaction);
        if (!detail || (detail->status != DbSchema::kEventsStatusApproved && !reviewer)) {
            resp = ErrorResponse::NotFound("Event not found");
            return;
        }
        result = PublicEventToJson(detail->event);
        if (reviewer) {
            result["status"] = detail->status;
            result["review_notes"] = detail->reviewNotes;
        }
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
