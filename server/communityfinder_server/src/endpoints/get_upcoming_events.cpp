#include "get_upcoming_events.h"

#include <cstdlib>
#include <string>

#include "business_logic/events/event_helper.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/events_json.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// Reads an int query param; returns `fallback` when absent or unparseable.
int64_t ParseIntParam(const crow::request& req, const char* key, int64_t fallback) {
    const char* value = req.url_params.get(key);
    if (value == nullptr) return fallback;
    char* end = nullptr;
    long long parsed = std::strtoll(value, &end, 10);
    if (end == value) return fallback;
    return static_cast<int64_t>(parsed);
}

// Reads a string query param; empty when absent (= not filtered).
std::string StringParam(const crow::request& req, const char* key) {
    const char* value = req.url_params.get(key);
    return value == nullptr ? std::string() : std::string(value);
}

void HandleGet(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = GetUpcomingEvents(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/events/upcoming")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleGet(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetUpcomingEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    // 0 / empty = unspecified; the helper clamps the window.
    Events::UpcomingQuery query;
    query.fromUs = ParseIntParam(req, "from_us", 0);
    query.toUs = ParseIntParam(req, "to_us", 0);
    query.categorySlug = StringParam(req, "category");
    query.sceneTagSlug = StringParam(req, "scene");
    query.neighborhoodSlug = StringParam(req, "neighborhood");
    query.venueId = ParseIntParam(req, "venue_id", 0);
    query.seriesId = ParseIntParam(req, "series_id", 0);

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::UpcomingEvents upcoming = helper.GetUpcomingPublic(transaction, query);
        result = Json::Value(Json::JsonObject{
            { "items", PublicEventsToJson(upcoming.events) },
            { "from_us", Json::Value(upcoming.window.fromUs) },
            { "to_us", Json::Value(upcoming.window.toUs) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
