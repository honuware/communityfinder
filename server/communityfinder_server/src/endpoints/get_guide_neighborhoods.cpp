#include "get_guide_neighborhoods.h"

#include <optional>

#include "business_logic/guide/guide_helper.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/guide_json.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

void WriteJson(crow::response& resp, const Json::Value& result) {
    if (resp.code == 200) {
        resp.set_header("Content-Type", "application/json");
        resp.write(result.ToString());
    }
}

void HandleList(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        WriteJson(resp, GetGuideNeighborhoods(endpointAuthHelper, req, resp));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

void HandleDetail(
    WebApp* webApp, const crow::request& req, crow::response& resp, const std::string& slug) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        WriteJson(resp, GetGuideNeighborhoodDetail(endpointAuthHelper, req, resp, slug));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/guide/neighborhoods")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleList(webApp, req, resp);
                });
        CROW_ROUTE(webApp->GetApp(), "/api/guide/neighborhoods/<string>")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp, const std::string& slug) {
                    HandleDetail(webApp, req, resp, slug);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetGuideNeighborhoods(
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
        Guide::GuideHelper helper(endpointAuthHelper.GetDatabaseHelper());
        result = Json::Value(Json::JsonObject{
            { "items", NeighborhoodSummariesToJson(helper.GetNeighborhoodsOverview(transaction)) },
        });
        resp.code = 200;
    });

    return result;
}

Json::Value GetGuideNeighborhoodDetail(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp,
    const std::string& slug) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Guide::GuideHelper helper(endpointAuthHelper.GetDatabaseHelper());
        std::optional<Guide::NeighborhoodDetail> detail =
            helper.GetNeighborhoodDetail(transaction, slug);
        if (!detail) {
            resp = ErrorResponse::NotFound("Neighborhood not found");
            return;
        }
        result = Json::Value(Json::JsonObject{
            { "neighborhood", NeighborhoodSummaryToJson(detail->neighborhood) },
            { "venues", GuideVenuesToJson(detail->venues) },
            { "organizations", GuideOrganizationsToJson(detail->organizations) },
            { "places", GuidePlacesToJson(detail->places) },
            { "rundown_page", OptionalGuidePageToJson(detail->rundownPage) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
