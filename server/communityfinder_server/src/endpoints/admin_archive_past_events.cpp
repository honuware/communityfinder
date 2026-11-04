#include "admin_archive_past_events.h"

#include "business_logic/jobs/scheduled_jobs.h"
#include "endpoints/endpoint_auth_helper.h"
#include "endpoints/scheduled_job_handler.h"

namespace Endpoints {
namespace {

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/admin/archive_past_events")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp) {
                    HandleScheduledJobPost(webApp, req, resp, &PostAdminArchivePastEvents);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminArchivePastEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request&,
    crow::response& resp) {
    return PostScheduledJob(endpointAuthHelper, resp, Jobs::kJobArchivePastEvents, &Jobs::RunScheduledJob);
}

}  // namespace Endpoints
