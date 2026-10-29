#include "get_guide_pages.h"

#include <optional>

#include "business_logic/auth/session.h"
#include "business_logic/guide/guide_helper.h"
#include "db_schema/guide_pages.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/guide_json.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// Whether the caller holds manage_guide. A quiet check, not RequirePermission:
// lacking it here just means the public view, never a 401/403.
bool HoldsManageGuide(EndpointAuthHelper& endpointAuthHelper, Transaction& transaction) {
    Auth::Session& session = endpointAuthHelper.GetSession();
    if (!session.IsLoggedIn()) {
        return false;
    }
    try {
        return session.ActiveUserHasPermission(transaction, DbSchema::kPermissionManageGuide);
    }
    catch (const std::exception&) {
        // Thrown when the permission isn't seeded. Fail closed, as
        // RequirePermission does.
        return false;
    }
}

void WriteJson(crow::response& resp, const Json::Value& result) {
    if (resp.code == 200) {
        resp.set_header("Content-Type", "application/json");
        resp.write(result.ToString());
    }
}

void HandlePage(
    WebApp* webApp, const crow::request& req, crow::response& resp, const std::string& slug) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        WriteJson(resp, GetGuidePage(endpointAuthHelper, req, resp, slug));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

void HandleSection(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        WriteJson(resp, GetGuidePagesBySection(endpointAuthHelper, req, resp));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/guide/pages")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleSection(webApp, req, resp);
                });
        CROW_ROUTE(webApp->GetApp(), "/api/guide/pages/<string>")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp, const std::string& slug) {
                    HandlePage(webApp, req, resp, slug);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetGuidePage(
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
        // The public read goes through the SQL publish rule (§2.4). Only when
        // that finds nothing does the caller's permission matter.
        std::optional<Guide::GuidePage> page = helper.GetPublishedGuidePage(transaction, slug);
        if (!page && HoldsManageGuide(endpointAuthHelper, transaction)) {
            page = helper.GetGuidePageAnyState(transaction, slug);
        }
        if (!page) {
            resp = ErrorResponse::NotFound("Page not found");
            return;
        }
        result = GuidePageToJson(*page);
        resp.code = 200;
    });

    return result;
}

Json::Value GetGuidePagesBySection(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    const char* section = req.url_params.get("section");
    if (section == nullptr || *section == '\0') {
        resp = ErrorResponse::BadRequest("A section is required");
        return {};
    }

    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Guide::GuideHelper helper(endpointAuthHelper.GetDatabaseHelper());
        result = Json::Value(Json::JsonObject{
            { "items", GuidePageSummariesToJson(
                           helper.GetPublishedPagesBySection(transaction, section)) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
