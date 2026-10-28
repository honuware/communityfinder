#include "get_events_ics_feed.h"

#include <cstdlib>
#include <string>

#include "business_logic/app_ical_config.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/event_ical.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_response.h"
#include "util/ical_generator.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper.h"

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
        GetEventsICalFeed(endpointAuthHelper, req, resp);
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/events/feed.ics")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleGet(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

void GetEventsICalFeed(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return;
    }

    // The window is the feed's own; only the filters come from the caller.
    Events::UpcomingQuery filters;
    filters.categorySlug = StringParam(req, "category");
    filters.sceneTagSlug = StringParam(req, "scene");
    filters.neighborhoodSlug = StringParam(req, "neighborhood");
    filters.seriesId = ParseIntParam(req, "series_id", 0);

    tp->RunInTransaction([&](Transaction& transaction) {
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::UpcomingEvents feed = helper.GetFeedEvents(transaction, filters);
        std::string siteAddress = endpointAuthHelper.GetSecretsHelper()->LookupSecret(
            transaction, Secrets::kWebsiteAddressLogin);

        resp.code = 200;
        resp.set_header("Content-Type", "text/calendar; charset=utf-8");
        resp.set_header("X-PUBLISHED-TTL", "PT1H");
        resp.set_header("Cache-Control", "max-age=3600");
        resp.body = ICalGenerator::GenerateICalendar(
            Events::BuildICalEventsFromPublicEvents(feed.events, App::kICalUidDomain, siteAddress),
            App::AppICalConfig());
    });
}

}  // namespace Endpoints
