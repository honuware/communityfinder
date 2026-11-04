#include "admin_past_event_settings.h"

#include <optional>
#include <string>
#include <string_view>

#include "business_logic/events/past_event_settings.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

Json::Value SettingsJson(const Events::PastEventSettings& settings) {
    const Events::PastEventDaysRange& range = Events::kArchivedEventsVisibleDaysRange;
    return Json::Value(Json::JsonObject{
        { std::string(Events::kPastEventSettingVisibleDays),
          Json::Value(Json::JsonObject{
              { "value", Json::Value(settings.archivedEventsVisibleDays) },
              { "default", Json::Value(range.defaultValue) },
              { "min", Json::Value(range.min) },
              { "max", Json::Value(range.max) },
          }) },
        { std::string(Events::kPastEventSettingShowByDefault),
          Json::Value(Json::JsonObject{
              { "value", Json::Value(settings.showPastByDefault) },
              { "default", Json::Value(Events::kEventsShowPastByDefaultDefault) },
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/past_event_settings")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return GetAdminPastEventSettings(helper, req, resp);
                    });
                });
        CROW_ROUTE(webApp->GetApp(), "/api/admin/past_event_settings")
            .methods(crow::HTTPMethod::Put)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return PutAdminPastEventSettings(helper, req, resp);
                    });
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetAdminPastEventSettings(
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
        result = SettingsJson(Events::LoadPastEventSettings(
            *endpointAuthHelper.GetSecretsHelper(), transaction));
        resp.code = 200;
    });

    return result;
}

Json::Value PutAdminPastEventSettings(
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

        // Types here; the range is SavePastEventSettings' call.
        Events::PastEventSettingsUpdate update;
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
        if (!read(Events::kPastEventSettingVisibleDays, update.archivedEventsVisibleDays,
                  "a whole number")
            || !read(Events::kPastEventSettingShowByDefault, update.showPastByDefault,
                     "true or false")) {
            return;
        }
        if (!update.archivedEventsVisibleDays && !update.showPastByDefault) {
            resp = ErrorResponse::BadRequest("No past-event settings in the request");
            return;
        }

        Events::SavePastEventSettingsResult saved = Events::SavePastEventSettings(
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
