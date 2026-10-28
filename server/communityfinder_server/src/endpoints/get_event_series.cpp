#include "get_event_series.h"

#include <optional>

#include "business_logic/events/event_helper.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/events_json.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

void WriteJson(crow::response& resp, const Json::Value& result) {
    if (resp.code == 200) {
        resp.set_header("Content-Type", "application/json");
        resp.write(result.ToString());
    }
}

void HandleList(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        WriteJson(resp, GetEventSeriesList(endpointAuthHelper, req, resp));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

void HandleDetail(
    WebApp* webApp, const crow::request& req, crow::response& resp, const std::string& slug) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        WriteJson(resp, GetEventSeriesDetail(endpointAuthHelper, req, resp, slug));
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/event_series")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleList(webApp, req, resp);
                });
        CROW_ROUTE(webApp->GetApp(), "/api/event_series/<string>")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp, const std::string& slug) {
                    HandleDetail(webApp, req, resp, slug);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value GetEventSeriesList(
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
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        result = Json::Value(Json::JsonObject{
            { "items", PublicSeriesListToJson(helper.GetActiveSeries(transaction)) },
        });
        resp.code = 200;
    });

    return result;
}

Json::Value GetEventSeriesDetail(
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
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        std::optional<Events::SeriesDetail> detail = helper.GetSeriesDetail(transaction, slug);
        if (!detail) {
            resp = ErrorResponse::NotFound("Series not found");
            return;
        }
        result = Json::Value(Json::JsonObject{
            { "series", PublicSeriesToJson(detail->series) },
            { "upcoming", PublicEventsToJson(detail->upcoming.events) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
