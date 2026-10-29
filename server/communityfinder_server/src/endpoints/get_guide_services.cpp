#include "get_guide_services.h"

#include <string>

#include "business_logic/guide/guide_helper.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/guide_json.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// Reads a string query param; empty when absent (= not filtered).
std::string StringParam(const crow::request& req, const char* key) {
    const char* value = req.url_params.get(key);
    return value == nullptr ? std::string() : std::string(value);
}

void HandleGet(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = GetGuideServices(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/guide/services")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleGet(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetGuideServices(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    const std::string serviceKind = StringParam(req, "kind");

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Guide::GuideHelper helper(endpointAuthHelper.GetDatabaseHelper());
        result = Json::Value(Json::JsonObject{
            { "items", GuideServicesToJson(helper.GetServices(transaction, serviceKind)) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
