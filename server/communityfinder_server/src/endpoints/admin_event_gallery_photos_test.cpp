#include "admin_event_gallery_photos.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_gallery_helper.h"
#include "business_logic/events/gallery_settings.h"
#include "db_schema/event_gallery_photos.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

// CM3 §3.3 — POST /api/admin/event_gallery_photos: a curator adds a gallery
// row, which records their consent confirmation; the photo is uploaded next
// through honuware's /api/upload_photo (its own tests).
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr const char* kGalleryUrl = "/api/admin/event_gallery_photos";

crow::response AddPhoto(EndpointTestHelper& helper, const Json::JsonObject& photo) {
    return Request(helper, crow::HTTPMethod::Post, kGalleryUrl, Json::Value(photo).ToString());
}

Json::JsonObject SeriesPhoto(int64_t seriesId) {
    return Json::JsonObject{
        { "series_id", Json::Value(seriesId) },
        { "alt_text", Json::Value("The dance floor") },
        { "consent_confirmed", Json::Value(true) },
    };
}

TEST(AdminEventGalleryPhotosTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        EXPECT_EQ(AddPhoto(helper, SeriesPhoto(world.t4t)).code, 401);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(AddPhoto(helper, SeriesPhoto(world.t4t)).code, 403);
        EXPECT_TRUE(TableHelpers::EventGalleryPhotos(testDb.GetDatabaseHelper())
                        .GetGalleryForSeries(transaction, world.t4t).empty());
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminEventGalleryPhotosTest, ACuratorAddsARowWithTheirConsentRecord) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ACuratorAddsARowWithTheirConsentRecord", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        const int64_t curatorId = SignIn(transaction, testDb, helper, "curator@example.com", true);
        const int64_t beforeUs = NowUs(transaction);

        Json::JsonObject photo = SeriesPhoto(world.t4t);
        photo["caption"] = Json::Value("  Opening night  ");
        photo["credit"] = Json::Value(nullptr);
        crow::response resp = AddPhoto(helper, photo);
        ASSERT_EQ(resp.code, 200) << resp.body;
        const int64_t id = Json::Value::FromText(resp.body)["id"].Get<int64_t>();

        KeyValueTable row = TableHelpers::EventGalleryPhotos(databaseHelper).GetGalleryPhoto(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosSeriesId)), StringFromInt(world.t4t));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosEventId)), "");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosCaption)), "Opening night");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosCredit)), "");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId)),
                  StringFromInt(curatorId));
        EXPECT_GE(std::stoll(row.at(std::string(DbSchema::kEventGalleryPhotosConsentConfirmedUs))), beforeUs);
    });
    Auth::ServerConfig::Shutdown();
}

// Each refusal is a 400 naming its code and field, and writes nothing.
TEST(AdminEventGalleryPhotosTest, RefusalsAre400WithTheirCodeAndField) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RefusalsAre400WithTheirCodeAndField", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        struct Case {
            std::string name;
            Json::JsonObject photo;
            std::string_view errorCode;  // empty for a wrong type, which has none
            std::string_view field;
        };
        std::vector<Case> cases;
        auto add = [&](std::string name, std::string key, Json::Value value,
                       std::string_view errorCode, std::string_view field) {
            Json::JsonObject photo = SeriesPhoto(world.t4t);
            photo[key] = std::move(value);
            cases.push_back({ std::move(name), std::move(photo), errorCode, field });
        };
        add("no consent", "consent_confirmed", Json::Value(false),
            Events::kGalleryErrorConsentRequired, Events::kGalleryFieldConsentConfirmed);
        add("blank alt text", "alt_text", Json::Value("   "),
            Events::kGalleryErrorAltTextRequired, Events::kGalleryFieldAltText);
        add("long credit", "credit", Json::Value(std::string(121, 'a')),
            Events::kGalleryErrorTooLong, Events::kGalleryFieldCredit);
        add("two parents", "event_id", Json::Value(int64_t{ 1 }),
            Events::kGalleryErrorOneParent, Events::kGalleryFieldEventId);
        add("no such series", "series_id", Json::Value(int64_t{ 999999 }),
            Events::kGalleryErrorParentNotFound, Events::kGalleryFieldSeriesId);
        add("id as text", "series_id", Json::Value("t4t"), "", Events::kGalleryFieldSeriesId);
        add("consent as text", "consent_confirmed", Json::Value("yes"), "",
            Events::kGalleryFieldConsentConfirmed);

        for (const Case& testCase : cases) {
            crow::response resp = AddPhoto(helper, testCase.photo);
            ASSERT_EQ(resp.code, 400) << testCase.name << ": " << resp.body;
            Json::Value body = Json::Value::FromText(resp.body);
            if (!testCase.errorCode.empty()) {
                EXPECT_EQ(body["error_code"].Get<std::string>(), testCase.errorCode) << testCase.name;
            }
            EXPECT_EQ(body["field"].Get<std::string>(), testCase.field) << testCase.name;
        }
        EXPECT_EQ(Request(helper, crow::HTTPMethod::Post, kGalleryUrl, "not json").code, 400);
        EXPECT_EQ(Request(helper, crow::HTTPMethod::Post, kGalleryUrl, "[]").code, 400);
        EXPECT_TRUE(TableHelpers::EventGalleryPhotos(databaseHelper)
                        .GetGalleryForSeries(transaction, world.t4t).empty());
    });
    Auth::ServerConfig::Shutdown();
}

// The cap is the admin setting (OQ5), and a full gallery says what it is.
TEST(AdminEventGalleryPhotosTest, AFullGalleryIs400WithTheCap) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AFullGalleryIs400WithTheCap", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        Events::GallerySettingsUpdate update;
        update.maxPhotos = 2;
        ASSERT_TRUE(Events::SaveGallerySettings(*helper.GetSecretsHelper(), transaction, update).ok);

        ASSERT_EQ(AddPhoto(helper, SeriesPhoto(world.t4t)).code, 200);
        ASSERT_EQ(AddPhoto(helper, SeriesPhoto(world.t4t)).code, 200);
        crow::response full = AddPhoto(helper, SeriesPhoto(world.t4t));
        ASSERT_EQ(full.code, 400) << full.body;
        Json::Value body = Json::Value::FromText(full.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kGalleryErrorFull);
        EXPECT_EQ(body["max_photos"].Get<int64_t>(), 2);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
