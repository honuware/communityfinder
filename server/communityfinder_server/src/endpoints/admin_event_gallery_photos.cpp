#include "admin_event_gallery_photos.h"

#include <string>
#include <string_view>
#include <utility>

#include "business_logic/auth/session.h"
#include "business_logic/events/event_gallery_helper.h"
#include "business_logic/events/gallery_settings.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostAdminEventGalleryPhoto(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/event_gallery_photos")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp) {
                    HandlePost(webApp, req, resp);
                });
    }
} g_setupRouting;

// Types only; whether a value is acceptable is EventGalleryHelper's call.
// Absent and null both mean "not given". "" when the body is well formed;
// otherwise the field at fault.
std::string ReadGalleryPhoto(const Json::Value& body, Events::NewGalleryPhoto& photo) {
    auto child = [&](std::string_view name) -> const Json::Value* {
        const Json::Value* value = nullptr;
        if (!body.HasChild(name, &value) || value->IsNull()) {
            return nullptr;
        }
        return value;
    };
    auto readId = [&](std::string_view name, int64_t& out) {
        if (const Json::Value* value = child(name)) {
            const int64_t* id = value->TryGet<int64_t>();
            if (id == nullptr) return false;
            out = *id;
        }
        return true;
    };
    auto readText = [&](std::string_view name, std::string& out) {
        if (const Json::Value* value = child(name)) {
            const std::string* text = value->TryGet<std::string>();
            if (text == nullptr) return false;
            out = *text;
        }
        return true;
    };
    if (!readId(Events::kGalleryFieldEventId, photo.eventId)) {
        return std::string(Events::kGalleryFieldEventId);
    }
    if (!readId(Events::kGalleryFieldSeriesId, photo.seriesId)) {
        return std::string(Events::kGalleryFieldSeriesId);
    }
    for (const auto& [name, out] : { std::pair<std::string_view, std::string*>{ Events::kGalleryFieldAltText, &photo.altText },
                                     std::pair<std::string_view, std::string*>{ Events::kGalleryFieldCaption, &photo.caption },
                                     std::pair<std::string_view, std::string*>{ Events::kGalleryFieldCredit, &photo.credit } }) {
        if (!readText(name, *out)) {
            return std::string(name);
        }
    }
    if (const Json::Value* consent = child(Events::kGalleryFieldConsentConfirmed)) {
        const bool* confirmed = consent->TryGet<bool>();
        if (confirmed == nullptr) {
            return std::string(Events::kGalleryFieldConsentConfirmed);
        }
        photo.consentConfirmed = *confirmed;
    }
    return {};
}

}  // namespace

Json::Value PostAdminEventGalleryPhoto(
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
        if (!body.HasChildren()) {
            resp = ErrorResponse::BadRequest("The request body must be a JSON object");
            return;
        }
        Events::NewGalleryPhoto photo;
        const std::string badField = ReadGalleryPhoto(body, photo);
        if (!badField.empty()) {
            resp = ErrorResponse::ValidationError(badField + " has the wrong type", badField);
            return;
        }

        const Events::GallerySettings settings =
            Events::LoadGallerySettings(*endpointAuthHelper.GetSecretsHelper(), transaction);
        Events::EventGalleryHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::AddGalleryPhotoResult added = helper.AddGalleryPhoto(
            transaction, photo, endpointAuthHelper.GetSession().GetPersonId(), settings.maxPhotos);
        if (!added.ok) {
            Json::JsonObject extensions{
                { "error_code", Json::Value(added.errorCode) },
                { "field", Json::Value(added.field) },
            };
            if (added.errorCode == Events::kGalleryErrorFull) {
                extensions["max_photos"] = Json::Value(added.maxPhotos);
            }
            resp = ErrorResponse::Create(
                ErrorCodes::kValidationError,
                ErrorCodes::GetTitleForType(ErrorCodes::kValidationError),
                ErrorCodes::GetStatusForType(ErrorCodes::kValidationError),
                added.errorCode, extensions);
            return;
        }

        result = Json::Value(Json::JsonObject{ { "id", Json::Value(added.id) } });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
