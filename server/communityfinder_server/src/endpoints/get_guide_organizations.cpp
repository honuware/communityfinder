#include "get_guide_organizations.h"

#include <optional>

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
        WriteJson(resp, GetGuideOrganizations(endpointAuthHelper, req, resp));
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
        WriteJson(resp, GetGuideOrganizationDetail(endpointAuthHelper, req, resp, slug));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/guide/orgs")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleList(webApp, req, resp);
                });
        CROW_ROUTE(webApp->GetApp(), "/api/guide/orgs/<string>")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp, const std::string& slug) {
                    HandleDetail(webApp, req, resp, slug);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetGuideOrganizations(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Guide::OrganizationFilters filters;
    filters.orgKind = StringParam(req, "kind");
    filters.sceneSlug = StringParam(req, "scene");
    filters.neighborhoodSlug = StringParam(req, "neighborhood");

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Guide::GuideHelper helper(endpointAuthHelper.GetDatabaseHelper());
        result = Json::Value(Json::JsonObject{
            { "items", GuideOrganizationsToJson(
                           helper.GetOrganizationsDirectory(transaction, filters)) },
        });
        resp.code = 200;
    });

    return result;
}

Json::Value GetGuideOrganizationDetail(
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
        std::optional<Guide::OrganizationDetail> detail =
            helper.GetOrganizationBySlug(transaction, slug);
        if (!detail) {
            resp = ErrorResponse::NotFound("Organization not found");
            return;
        }
        result = Json::Value(Json::JsonObject{
            { "organization", GuideOrganizationToJson(detail->organization) },
            { "links", GuideLinksToJson(detail->links) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
