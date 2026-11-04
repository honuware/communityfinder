#include "get_event_settings.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "business_logic/auth/server_config.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/secrets/secrets_helper_test_util.h"

// EV4 §10.3 — GET /api/events/settings, signed out.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

Json::Value GetSettings(EndpointTestHelper& helper) {
    crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/events/settings");
    EXPECT_EQ(resp.code, 200) << resp.body;
    EXPECT_EQ(resp.get_header_value("Cache-Control"), "no-cache");
    return Json::Value::FromText(resp.body);
}

TEST(GetEventSettingsTest, NothingStoredServesTheDefaults) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NothingStoredServesTheDefaults", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        Json::Value body = GetSettings(helper);
        EXPECT_EQ(body["archived_events_visible_days"].Get<int64_t>(), 365);
        EXPECT_FALSE(body["events_show_past_by_default"].Get<bool>());
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetEventSettingsTest, ServesWhatIsStored) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ServesWhatIsStored", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kArchivedEventsVisibleDays, "90");
        helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kEventsShowPastByDefault, "true");
        Json::Value body = GetSettings(helper);
        EXPECT_EQ(body["archived_events_visible_days"].Get<int64_t>(), 90);
        EXPECT_TRUE(body["events_show_past_by_default"].Get<bool>());

        // A bad window reads as the default, as everywhere else.
        helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kArchivedEventsVisibleDays, "forever");
        EXPECT_EQ(GetSettings(helper)["archived_events_visible_days"].Get<int64_t>(), 365);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
