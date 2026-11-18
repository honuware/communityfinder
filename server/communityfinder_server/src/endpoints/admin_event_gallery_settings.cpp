#include "admin_event_gallery_settings.h"

#include <optional>
#include <string>

#include "business_logic/events/gallery_settings.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

Json::Value SettingsJson(const Events::GallerySettings& settings) {
    const Events::GalleryMaxPhotosRange& range = Events::kEventGalleryMaxPhotosRange;
    return Json::Value(Json::JsonObject{
        { std::string(Events::kGallerySettingMaxPhotos),
          Json::Value(Json::JsonObject{
              { "value", Json::Value(settings.maxPhotos) },
              { "default", Json::Value(range.defaultValue) },
              { "min", Json::Value(range.min) },
              { "max", Json::Value(range.max) },
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/event_gallery_settings")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return GetAdminEventGallerySettings(helper, req, resp);
                    });
                });
        CROW_ROUTE(webApp->GetApp(), "/api/admin/event_gallery_settings")
            .methods(crow::HTTPMethod::Put)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return PutAdminEventGallerySettings(helper, req, resp);
                    });
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetAdminEventGallerySettings(
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
        result = SettingsJson(Events::LoadGallerySettings(
            *endpointAuthHelper.GetSecretsHelper(), transaction));
        resp.code = 200;
    });

    return result;
}

Json::Value PutAdminEventGallerySettings(
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

        // The type here; the range is SaveGallerySettings' call.
        const std::string_view name = Events::kGallerySettingMaxPhotos;
        const Json::Value* value = nullptr;
        if (!body.HasChild(name, &value) || value->IsNull()) {
            resp = ErrorResponse::BadRequest("No gallery settings in the request");
            return;
        }
        const int64_t* maxPhotos = value->TryGet<int64_t>();
        if (maxPhotos == nullptr) {
            resp = ErrorResponse::ValidationError(std::string(name) + " must be a whole number", name);
            return;
        }
        Events::GallerySettingsUpdate update;
        update.maxPhotos = *maxPhotos;

        Events::SaveGallerySettingsResult saved = Events::SaveGallerySettings(
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
