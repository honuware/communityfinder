#include "admin_event_submission_settings.h"

#include <optional>
#include <string>
#include <string_view>

#include "business_logic/events/submission_settings.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

Json::Value LimitJson(int64_t value, const Events::SubmissionLimitRange& range) {
    return Json::Value(Json::JsonObject{
        { "value", Json::Value(value) },
        { "default", Json::Value(range.defaultValue) },
        { "min", Json::Value(range.min) },
        { "max", Json::Value(range.max) },
    });
}

Json::Value SettingsJson(const Events::SubmissionSettings& settings) {
    return Json::Value(Json::JsonObject{
        { std::string(Events::kSubmissionLimitMaxPendingDates),
          LimitJson(settings.maxPendingDates, Events::kMaxPendingDatesRange) },
        { std::string(Events::kSubmissionLimitMaxSubmissionsPerDay),
          LimitJson(settings.maxSubmissionsPerDay, Events::kMaxSubmissionsPerDayRange) },
        { std::string(Events::kSubmissionSettingSubmissionsPaused),
          Json::Value(Json::JsonObject{
              { "value", Json::Value(settings.submissionsPaused) },
              { "default", Json::Value(Events::kSubmissionsPausedDefault) },
          }) },
    });
}

template <class Handler>
void Handle(WebApp* webApp, const crow::request& req, crow::response& resp, Handler handler) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = handler(endpointAuthHelper);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/event_submission_settings")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return GetAdminEventSubmissionSettings(helper, req, resp);
                    });
                });
        CROW_ROUTE(webApp->GetApp(), "/api/admin/event_submission_settings")
            .methods(crow::HTTPMethod::Put)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return PutAdminEventSubmissionSettings(helper, req, resp);
                    });
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetAdminEventSubmissionSettings(
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
        if (!endpointAuthHelper.RequirePermission(
                transaction, DbSchema::kPermissionManageEvents, resp)) {
            return;
        }
        result = SettingsJson(Events::LoadSubmissionSettings(
            *endpointAuthHelper.GetSecretsHelper(), transaction));
        resp.code = 200;
    });

    return result;
}

Json::Value PutAdminEventSubmissionSettings(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
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

        // Types here; ranges are SaveSubmissionSettings' call.
        Events::SubmissionSettingsUpdate update;
        auto read = [&]<class Type>(std::string_view name, std::optional<Type>& out,
                                    std::string_view mustBe) {
            const Json::Value* value = nullptr;
            if (!body.HasChild(name, &value) || value->IsNull()) {
                return true;
            }
            const Type* typed = value->TryGet<Type>();
            if (typed == nullptr) {
                resp = ErrorResponse::ValidationError(
                    std::string(name) + " must be " + std::string(mustBe), name);
                return false;
            }
            out = *typed;
            return true;
        };
        if (!read(Events::kSubmissionLimitMaxPendingDates, update.maxPendingDates, "a whole number")
            || !read(Events::kSubmissionLimitMaxSubmissionsPerDay, update.maxSubmissionsPerDay,
                     "a whole number")
            || !read(Events::kSubmissionSettingSubmissionsPaused, update.submissionsPaused,
                     "true or false")) {
            return;
        }
        if (!update.maxPendingDates && !update.maxSubmissionsPerDay && !update.submissionsPaused) {
            resp = ErrorResponse::BadRequest("No submission settings in the request");
            return;
        }

        Events::SaveSettingsResult saved = Events::SaveSubmissionSettings(
            *endpointAuthHelper.GetSecretsHelper(), transaction, update);
        if (!saved.ok) {
            resp = ErrorResponse::Create(
                ErrorCodes::kValidationError,
                ErrorCodes::GetTitleForType(ErrorCodes::kValidationError),
                ErrorCodes::GetStatusForType(ErrorCodes::kValidationError),
                saved.field + " is outside its allowed range",
                {
                    { "error_code", Json::Value(saved.errorCode) },
                    { "field", Json::Value(saved.field) },
                });
            return;
        }
        result = SettingsJson(saved.settings);
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
