#include "photo_removal_requests.h"

#include <chrono>
#include <string>
#include <string_view>

#include "business_logic/auth/proxy_trust.h"
#include "business_logic/events/photo_removal_request_helper.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// The body's string fields; absent and null both mean "not given". "" when
// it's well formed, otherwise the problem and the field it's in.
std::string ReadRequest(const Json::Value& body, Events::PhotoRemovalRequest& request,
                        std::string& field) {
    if (!body.HasChildren()) {
        return "The request body must be a JSON object";
    }
    const std::pair<std::string_view, std::string*> fields[] = {
        { "page_path", &request.pagePath },
        { "page_title", &request.pageTitle },
        { "photo", &request.photo },
        { "message", &request.message },
        { "reply_email", &request.replyEmail },
        { "website", &request.website },
    };
    for (const auto& [name, out] : fields) {
        const Json::Value* child = nullptr;
        if (!body.HasChild(name, &child) || child->IsNull()) {
            continue;
        }
        const std::string* text = child->TryGet<std::string>();
        if (text == nullptr) {
            field = std::string(name);
            return std::string(name) + " must be a string";
        }
        *out = *text;
    }
    return {};
}

crow::response RefusalOf(const Events::PhotoRemovalResult& result) {
    std::string_view type = ErrorCodes::kValidationError;
    if (result.errorCode == Events::kPhotoRemovalErrorRateLimited) {
        type = ErrorCodes::kTooManyAttempts;
    } else if (result.errorCode == Events::kPhotoRemovalErrorNotSent) {
        type = ErrorCodes::kServiceUnavailable;
    }
    Json::JsonObject extensions{ { "error_code", Json::Value(result.errorCode) } };
    if (!result.errorField.empty()) {
        extensions["field"] = Json::Value(result.errorField);
    }
    return ErrorResponse::Create(type, ErrorCodes::GetTitleForType(type),
                                 ErrorCodes::GetStatusForType(type), result.errorMessage,
                                 extensions);
}

int64_t SteadyNowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostPhotoRemovalRequest(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/photo_removal_requests")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp) {
                    HandlePost(webApp, req, resp);
                });
        // Visitors who never signed in post this, and they have no csrft
        // cookie to echo. The route reads no session and acts for no one, so
        // a forged post can do nothing a direct one couldn't.
        webApp->GetApp().get_middleware<CsrfGuard>().AllowWithoutToken(
            "/api/photo_removal_requests");
    }
} g_setupRouting;

}  // namespace

Json::Value PostPhotoRemovalRequest(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    Json::Value body;
    try {
        body = Json::Value::FromText(req.body);
    }
    catch (const std::exception&) {
        resp = ErrorResponse::BadRequest("Request body must be JSON");
        return {};
    }
    Events::PhotoRemovalRequest request;
    std::string field;
    const std::string problem = ReadRequest(body, request, field);
    if (!problem.empty()) {
        resp = ErrorResponse::ValidationError(problem, field);
        return {};
    }

    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    // No IP (no proxy header, no peer) is still one visitor, not a free pass.
    std::string visitor = Auth::ResolveClientIp(req);
    if (visitor.empty()) {
        visitor = "unknown";
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Events::PhotoRemovalRequestHelper helper(endpointAuthHelper.GetDatabaseHelper());
        const Events::PhotoRemovalResult sent = helper.Send(
            transaction, *endpointAuthHelper.GetSecretsHelper(), endpointAuthHelper.GetMailHelper(),
            request, Events::SharedPhotoRemovalRateLimiter(), visitor, SteadyNowUs());
        if (!sent.ok) {
            resp = RefusalOf(sent);
            return;
        }
        // The honeypot's answer is the same, so a bot learns nothing.
        result = Json::Value(Json::JsonObject{ { "sent", Json::Value(true) } });
        resp.code = 200;
    });
    return result;
}

}  // namespace Endpoints
