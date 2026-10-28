#include "get_event_ics.h"

#include <optional>
#include <string>

#include "business_logic/app_ical_config.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/event_ical.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_response.h"
#include "util/ical_generator.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper.h"
#include "util/types.h"

namespace Endpoints {
namespace {

void HandleGet(WebApp* webApp, const crow::request& req, crow::response& resp, int64_t eventId) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        GetEventICal(endpointAuthHelper, req, resp, eventId);
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/events/<int>/ical")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp, int64_t eventId) {
                    HandleGet(webApp, req, resp, eventId);
                });
    }
} g_setupRouting;

}  // namespace

void GetEventICal(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp,
    int64_t eventId) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return;
    }

    tp->RunInTransaction([&](Transaction& transaction) {
        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        std::optional<Events::EventDetail> detail = helper.GetEventDetail(transaction, eventId);
        if (!detail || detail->status != DbSchema::kEventsStatusApproved) {
            resp = ErrorResponse::NotFound("Event not found");
            return;
        }
        std::string siteAddress = endpointAuthHelper.GetSecretsHelper()->LookupSecret(
            transaction, Secrets::kWebsiteAddressLogin);

        resp.code = 200;
        resp.set_header("Content-Type", "text/calendar; charset=utf-8");
        resp.set_header("Content-Disposition",
                        "attachment; filename=\"event-" + StringFromInt(eventId) + ".ics\"");
        resp.body = ICalGenerator::GenerateICalendar(
            Events::BuildICalEvent(detail->event, App::kICalUidDomain, siteAddress),
            App::AppICalConfig());
    });
}

}  // namespace Endpoints
