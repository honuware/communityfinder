#include "admin_expire_unreviewed_events.h"

#include "business_logic/jobs/scheduled_jobs.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/scheduled_job_handler.h"

namespace Endpoints {
namespace {

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/admin/expire_unreviewed_events")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleScheduledJobPost(webApp, req, resp, &PostAdminExpireUnreviewedEvents);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminExpireUnreviewedEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request&,
    crow::response& resp) {
    return PostScheduledJob(endpointAuthHelper, resp, Jobs::kJobExpireUnreviewedEvents, &Jobs::RunScheduledJob);
}

}  // namespace Endpoints
