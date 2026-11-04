#include "get_event_settings.h"

#include <string>

#include "business_logic/events/past_event_settings.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

void HandleGet(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = GetEventSettings(endpointAuthHelper, req, resp);
        if (resp.code == 200) {
            resp.set_header("Content-Type", "application/json");
            // Revalidated rather than held, as /api/site_info does, so an
            // admin's change shows on the next page load.
            resp.set_header("Cache-Control", "no-cache");
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
        CROW_ROUTE(webApp->GetApp(), "/api/events/settings")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleGet(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetEventSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Events::PastEventSettings settings = Events::LoadPastEventSettings(
            *endpointAuthHelper.GetSecretsHelper(), transaction);
        result = Json::Value(Json::JsonObject{
            { std::string(Events::kPastEventSettingVisibleDays),
              Json::Value(settings.archivedEventsVisibleDays) },
            { std::string(Events::kPastEventSettingShowByDefault),
              Json::Value(settings.showPastByDefault) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
