#include "admin_reset_password.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "endpoints/endpoint_auth_helper.h"
#include "business_logic/auth/auth_helper.h"
#include "business_logic/auth/person.h"
#include "business_logic/auth/quick_account_welcome_mail.h"
#include "db_schema/people.h"
#include "sql_util/table_helpers/people.h"
#include "util/error_response.h"
#include "util/json_value.h"
#include "util/mail/mail_helper.h"
#include "util/mail/tenant_branding.h"
#include "util/types.h"

namespace Endpoints {
namespace {

// See admin_create_user.cpp — the admin-only gate (no framework string constant).
constexpr std::string_view kPermissionAdminPortal = "admin_portal";

// Random temporary password. Mirrors the framework quick-account generator
// (honuware quick_account_helper.cpp): ambiguous glyphs (I/l/1, O/0) are excluded
// because the person types this out of an email.
std::string GenerateTemporaryPassword(int length = 12) {
    static constexpr std::string_view kChars =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#";
    Auth::AuthHelper auth;
    Blob randomBytes = auth.RandomBytes(length);
    std::string password;
    password.reserve(length);
    for (int i = 0; i < length; ++i) {
        password.push_back(
            kChars[static_cast<uint8_t>(randomBytes[i]) % kChars.size()]);
    }
    return password;
}

std::string ReadColumn(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return (it == row.end()) ? std::string() : it->second;
}

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostAdminResetPassword(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/reset_password")
            .methods(crow::HTTPMethod::POST)(
                [=](const crow::request& req, crow::response& resp) {
                    HandlePost(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminResetPassword(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {

    if (req.body.empty()) {
        resp = ErrorResponse::BadRequest("Request body required");
        return {};
    }

    Json::Value body = Json::Value::FromText(req.body);

    const Json::Value* val = nullptr;
    if (!body.HasChild("email", &val)) {
        resp = ErrorResponse::ValidationError("email is required");
        return {};
    }
    std::string email = val->Get<std::string>();
    if (email.empty()) {
        resp = ErrorResponse::ValidationError("email must not be empty");
        return {};
    }

    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        // Admin-only: resetting another account's password requires `admin_portal`.
        if (!endpointAuthHelper.RequirePermission(
                transaction, kPermissionAdminPortal, resp)) {
            return;
        }

        TableHelpers::People people(endpointAuthHelper.GetDatabaseHelper());
        KeyValueTable row = people.LookupPersonByEmail(transaction, email);
        if (row.empty()) {
            resp = ErrorResponse::ValidationError("No account with that email");
            return;
        }
        const int64_t personId =
            std::stoll(ReadColumn(row, DbSchema::kPeopleId));
        const std::string firstName = ReadColumn(row, DbSchema::kPeopleFirstName);

        const std::string temporaryPassword = GenerateTemporaryPassword();

        // UpdatePassword takes the plaintext and hashes it internally; pass the
        // SecretsHelper so the Argon2id cost is production-driven rather than the
        // fast INTERACTIVE fallback. must_change_password forces the person to set
        // a new password on their next login with the emailed temporary one.
        Auth::PersonHelper personHelper(endpointAuthHelper.GetDatabaseHelper());
        personHelper.UpdatePassword(
            transaction, email, temporaryPassword,
            endpointAuthHelper.GetSecretsHelper());
        people.SetMustChangePassword(transaction, personId, true);

        // Email the temporary password. Reuses the framework quick-account welcome
        // body — it already renders a "Temporary Password" block + a "change it
        // immediately after your first login" warning, which is exactly a reset
        // notice. A send failure must never roll back the reset itself.
        auto secrets = endpointAuthHelper.GetSecretsHelper();
        auto mailHelper = endpointAuthHelper.GetMailHelper();
        if (mailHelper && secrets) {
            Auth::Mail::QuickAccountWelcomeData emailData;
            emailData.firstName = firstName;
            emailData.email = email;
            emailData.temporaryPassword = temporaryPassword;

            ::Mail::TenantBranding branding =
                ::Mail::LoadTenantBranding(*secrets, transaction);
            std::string htmlBody =
                Auth::Mail::GenerateQuickAccountWelcomeBody(emailData, branding);
            ::Mail::MailAddress from{branding.senderName, branding.senderAddress};
            ::Mail::MailAddress to{firstName, email};
            ::Mail::MailMessage message(from, to);
            message.SetSubject(
                branding.studioName + " - Your password has been reset");
            message.SetBodyHtml(htmlBody);
            try { mailHelper->SendMail(message); } catch (...) {}
        }

        result = Json::Value(Json::JsonObject{
            {"status", Json::Value(std::string("ok"))},
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
