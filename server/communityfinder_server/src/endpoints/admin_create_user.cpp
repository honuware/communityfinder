#include "admin_create_user.h"

#include <string>
#include <string_view>

#include "endpoints/endpoint_auth_helper.h"
#include "business_logic/auth/quick_account_helper.h"
#include "util/error_response.h"
#include "util/json_value.h"

namespace Endpoints {
namespace {

// The framework permission seeded + granted to the Administrator role
// (honuware create_framework_tables.cpp). It has no framework string constant, so
// the CF admin endpoints name it here. This is the same gate the admin CRUD editor
// relies on (admins hold it via the admin role).
constexpr std::string_view kPermissionAdminPortal = "admin_portal";

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostAdminCreateUser(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/create_user")
            .methods(crow::HTTPMethod::POST)(
                [=](const crow::request& req, crow::response& resp) {
                    HandlePost(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminCreateUser(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {

    if (req.body.empty()) {
        resp = ErrorResponse::BadRequest("Request body required");
        return {};
    }

    Json::Value body = Json::Value::FromText(req.body);

    const Json::Value* val = nullptr;
    if (!body.HasChild("first_name", &val)) {
        resp = ErrorResponse::ValidationError("first_name is required");
        return {};
    }
    std::string firstName = val->Get<std::string>();

    if (!body.HasChild("last_name", &val)) {
        resp = ErrorResponse::ValidationError("last_name is required");
        return {};
    }
    std::string lastName = val->Get<std::string>();

    if (!body.HasChild("email", &val)) {
        resp = ErrorResponse::ValidationError("email is required");
        return {};
    }
    std::string email = val->Get<std::string>();

    if (firstName.empty() || lastName.empty() || email.empty()) {
        resp = ErrorResponse::ValidationError(
            "first_name, last_name, and email must not be empty");
        return {};
    }

    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        // Admin-only: managing accounts requires the `admin_portal` permission.
        if (!endpointAuthHelper.RequirePermission(
                transaction, kPermissionAdminPortal, resp)) {
            return;
        }

        // Account creation (temporary password, must_change_password, welcome
        // email with the temporary password) lives in the framework business
        // layer, shared with knottyyoga's staff quick-account flow.
        // CommunityFinder has no post-creation side effects (no gift
        // invitations), so no post-create hook.
        Auth::QuickAccountHelper quickAccounts(
            endpointAuthHelper.GetDatabaseHelper(),
            endpointAuthHelper.GetSecretsHelper(),
            endpointAuthHelper.GetMailHelper());
        Auth::QuickAccountResult account = quickAccounts.EnsureAccountWithWelcome(
            transaction, firstName, lastName, email);

        result = Json::Value(Json::JsonObject{
            {"person_id", Json::Value(account.personId)},
            {"already_exists", Json::Value(account.alreadyExists)},
            {"first_name", Json::Value(account.firstName)},
            {"last_name", Json::Value(account.lastName)},
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
