#include "admin_event_search.h"

#include <cstdlib>
#include <string>

#include "business_logic/events/event_helper.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// The `limit` query parameter; 0 (the helper's default) when it's absent or
// isn't a number.
int64_t LimitParam(const crow::request& req) {
    const char* value = req.url_params.get("limit");
    if (value == nullptr) return 0;
    char* end = nullptr;
    long long parsed = std::strtoll(value, &end, 10);
    return end == value ? 0 : static_cast<int64_t>(parsed);
}

Json::Value MatchToJson(const Events::CuratorEventMatch& match) {
    const Events::PublicEvent& event = match.event;
    return Json::Value(Json::JsonObject{
        { "id", Json::Value(event.id) },
        { "title", Json::Value(event.title) },
        { "starts_at_us", Json::Value(event.startsAtUs) },
        { "ends_at_us", event.endsAtUs ? Json::Value(*event.endsAtUs) : Json::Value(nullptr) },
        { "venue_name", Json::Value(event.venueName) },
        { "location_text", Json::Value(event.locationText) },
        { "series_title", Json::Value(event.seriesTitle) },
        { "status", Json::Value(match.status) },
    });
}

void HandleGet(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = GetAdminEventSearch(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/events/search")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleGet(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetAdminEventSearch(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    const char* query = req.url_params.get("q");
    const std::string searchText = query == nullptr ? std::string() : std::string(query);
    const int64_t limit = LimitParam(req);

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        if (!endpointAuthHelper.RequirePermission(
                transaction, DbSchema::kPermissionManageEvents, resp)) {
            return;
        }
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Json::JsonArray items;
        for (const Events::CuratorEventMatch& match :
             helper.FindForCurators(transaction, searchText, limit)) {
            items.push_back(MatchToJson(match));
        }
        result = Json::Value(Json::JsonObject{ { "items", Json::Value(std::move(items)) } });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
