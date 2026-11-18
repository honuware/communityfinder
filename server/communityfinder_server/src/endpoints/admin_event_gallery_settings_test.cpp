#include "admin_event_gallery_settings.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/gallery_settings.h"
#include "endpoints/events_endpoint_test_support.h"

// CM3 OQ5 — GET / PUT /api/admin/event_gallery_settings: the gallery cap.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr const char* kSettingsUrl = "/api/admin/event_gallery_settings";

crow::response GetSettings(EndpointTestHelper& helper) {
    return Request(helper, crow::HTTPMethod::Get, kSettingsUrl);
}

crow::response PutSettings(EndpointTestHelper& helper, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Put, kSettingsUrl, body);
}

void ExpectCap(const crow::response& resp, int64_t maxPhotos) {
    ASSERT_EQ(resp.code, 200) << resp.body;
    const Json::Value body = Json::Value::FromText(resp.body);
    const Json::Value& cap = body[Events::kGallerySettingMaxPhotos];
    EXPECT_EQ(cap["value"].Get<int64_t>(), maxPhotos);
    EXPECT_EQ(cap["default"].Get<int64_t>(), 12);
    EXPECT_EQ(cap["min"].Get<int64_t>(), Events::kEventGalleryMaxPhotosRange.min);
    EXPECT_EQ(cap["max"].Get<int64_t>(), Events::kEventGalleryMaxPhotosRange.max);
}

TEST(AdminEventGallerySettingsTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        const std::string cap = R"({"event_gallery_max_photos": 20})";

        EXPECT_EQ(GetSettings(helper).code, 401);
        EXPECT_EQ(PutSettings(helper, cap).code, 401);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(GetSettings(helper).code, 403);
        EXPECT_EQ(PutSettings(helper, cap).code, 403);

        SwitchTo(transaction, testDb, helper, "curator@example.com");
        ExpectCap(GetSettings(helper), 12);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventGallerySettingsTest, SaveThenRead) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SaveThenRead", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        ExpectCap(PutSettings(helper, R"({"event_gallery_max_photos": 20})"), 20);
        ExpectCap(GetSettings(helper), 20);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventGallerySettingsTest, BadValuesAre400AndWriteNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BadValuesAre400AndWriteNothing", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        for (const std::string outOfRange : { "0", "61" }) {
            crow::response resp =
                PutSettings(helper, R"({"event_gallery_max_photos": )" + outOfRange + "}");
            ASSERT_EQ(resp.code, 400) << outOfRange << ": " << resp.body;
            Json::Value body = Json::Value::FromText(resp.body);
            EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kGalleryErrorMaxPhotosOutOfRange);
            EXPECT_EQ(body["field"].Get<std::string>(), Events::kGallerySettingMaxPhotos);
        }
        crow::response notANumber = PutSettings(helper, R"({"event_gallery_max_photos": "twelve"})");
        ASSERT_EQ(notANumber.code, 400) << notANumber.body;
        EXPECT_EQ(Json::Value::FromText(notANumber.body)["field"].Get<std::string>(),
                  Events::kGallerySettingMaxPhotos);
        EXPECT_EQ(PutSettings(helper, "{}").code, 400);
        EXPECT_EQ(PutSettings(helper, R"({"event_gallery_max_photos": null})").code, 400);
        EXPECT_EQ(PutSettings(helper, "not json").code, 400);

        ExpectCap(GetSettings(helper), 12);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
