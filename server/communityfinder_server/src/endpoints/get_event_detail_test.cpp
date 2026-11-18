#include "get_event_detail.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_gallery_helper.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/photo_test_support.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

// EV1 §5.2 — GET /api/events/<int>: the public sees approved and (EV4) archived
// events only; a manage_events holder previews any status.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

crow::response GetDetail(EndpointTestHelper& helper, int64_t eventId) {
    return Request(helper, crow::HTTPMethod::Get, "/api/events/" + StringFromInt(eventId));
}

bool HasField(const Json::Value& body, const char* name) {
    return body.HasChild(name, static_cast<const Json::Value**>(nullptr));
}

TEST(GetEventDetailTest, SignedOutSeesApprovedEventsOnly) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutSeesApprovedEventsOnly", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        EventSpec approvedSpec{ "approved", tomorrow };
        approvedSpec.venueId = world.sunsetTavern;
        int64_t approved = AddEvent(transaction, databaseHelper, world, approvedSpec);
        int64_t pending = AddEvent(transaction, databaseHelper, world, { "pending", tomorrow, "pending" });
        int64_t rejected = AddEvent(transaction, databaseHelper, world, { "rejected", tomorrow, "rejected" });

        crow::response resp = GetDetail(helper, approved);
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["id"].Get<int64_t>(), approved);
        EXPECT_EQ(body["venue_name"].Get<std::string>(), "Sunset Tavern");
        EXPECT_TRUE(body["category_slugs"].IsArray());
        EXPECT_FALSE(HasField(body, "status"));
        EXPECT_FALSE(HasField(body, "review_notes"));

        // Pending, rejected and missing all look the same from outside.
        EXPECT_EQ(GetDetail(helper, pending).code, 404);
        EXPECT_EQ(GetDetail(helper, rejected).code, 404);
        EXPECT_EQ(GetDetail(helper, 999999).code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetEventDetailTest, ManageEventsPreviewsAnyStatus) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ManageEventsPreviewsAnyStatus", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        int64_t pending = AddEvent(transaction, databaseHelper, world, { "pending", tomorrow, "pending" });
        int64_t rejected = AddEvent(transaction, databaseHelper, world, { "rejected", tomorrow, "rejected" });
        int64_t approved = AddEvent(transaction, databaseHelper, world, { "approved", tomorrow });
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        crow::response pendingResp = GetDetail(helper, pending);
        ASSERT_EQ(pendingResp.code, 200) << pendingResp.body;
        Json::Value pendingBody = Json::Value::FromText(pendingResp.body);
        EXPECT_EQ(pendingBody["status"].Get<std::string>(), "pending");
        EXPECT_EQ(pendingBody["review_notes"].Get<std::string>(), "");

        crow::response rejectedResp = GetDetail(helper, rejected);
        ASSERT_EQ(rejectedResp.code, 200) << rejectedResp.body;
        EXPECT_EQ(Json::Value::FromText(rejectedResp.body)["status"].Get<std::string>(), "rejected");

        crow::response approvedResp = GetDetail(helper, approved);
        ASSERT_EQ(approvedResp.code, 200) << approvedResp.body;
        EXPECT_EQ(Json::Value::FromText(approvedResp.body)["status"].Get<std::string>(), "approved");

        EXPECT_EQ(GetDetail(helper, 999999).code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

// EV4 OQ3: an archived event happened and was listed, so its page still loads
// for anyone (shared links and search results keep working).
TEST(GetEventDetailTest, SignedOutStillSeesAnArchivedEvent) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutStillSeesAnArchivedEvent", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t lastWeek = NowUs(transaction) - 7 * Events::kUsPerDay;
        int64_t archived = AddEvent(transaction, databaseHelper, world, { "archived", lastWeek, "archived" });

        crow::response resp = GetDetail(helper, archived);
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["id"].Get<int64_t>(), archived);
        EXPECT_FALSE(HasField(body, "status"));
    });
    Auth::ServerConfig::Shutdown();
}

// EV4 §10 (OQ8): once the past-events window has passed, the public gets a 404,
// as for an event that never was. The row stays, and a curator still sees it.
TEST(GetEventDetailTest, AnEventPastTheWindowIsNotFoundButACuratorStillSeesIt) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnEventPastTheWindowIsNotFound", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kArchivedEventsVisibleDays, "30");
        const int64_t nowUs = NowUs(transaction);
        int64_t inside = AddEvent(transaction, databaseHelper, world,
                                  { "inside", nowUs - 29 * Events::kUsPerDay, "archived" });
        int64_t outside = AddEvent(transaction, databaseHelper, world,
                                   { "outside", nowUs - 31 * Events::kUsPerDay, "archived" });

        EXPECT_EQ(GetDetail(helper, inside).code, 200);
        EXPECT_EQ(GetDetail(helper, outside).code, 404);

        SignIn(transaction, testDb, helper, "curator@example.com", true);
        crow::response resp = GetDetail(helper, outside);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(Json::Value::FromText(resp.body)["status"].Get<std::string>(), "archived");
    });
    Auth::ServerConfig::Shutdown();
}

// CM3 — the page's photo (resolved by the server), its video ({provider, id}
// only) and its gallery in order.
TEST(GetEventDetailTest, CarriesThePhotoVideoAndGallery) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CarriesThePhotoVideoAndGallery", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        EventSpec inSeries{ "in-series", tomorrow };
        inSeries.seriesSlug = "t4t";
        int64_t eventId = AddEvent(transaction, databaseHelper, world, inSeries);
        int64_t bare = AddEvent(transaction, databaseHelper, world, { "bare", tomorrow });
        TableHelpers::PhotoTest::AttachTestPhoto(transaction, databaseHelper, DbSchema::kSeriesTable, world.t4t);
        TableHelpers::Events(databaseHelper).UpdateEvent(transaction, eventId, {
            { std::string(DbSchema::kEventsVideoUrl), "https://www.youtube.com/watch?v=dQw4w9WgXcQ" } });
        Events::EventGalleryHelper gallery(databaseHelper);
        std::vector<int64_t> galleryIds;
        for (std::string_view altText : { "The stage", "The bar" }) {
            Events::NewGalleryPhoto photo;
            photo.eventId = eventId;
            photo.altText = std::string(altText);
            photo.consentConfirmed = true;
            galleryIds.push_back(gallery.AddGalleryPhoto(transaction, photo, 1, 12).id);
            TableHelpers::PhotoTest::AttachTestPhoto(
                transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable, galleryIds.back());
        }

        crow::response resp = GetDetail(helper, eventId);
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["photo"]["table"].Get<std::string>(), DbSchema::kSeriesTable);
        EXPECT_EQ(body["photo"]["id"].Get<int64_t>(), world.t4t);
        EXPECT_EQ(body["video"]["provider"].Get<std::string>(), "youtube");
        EXPECT_EQ(body["video"]["id"].Get<std::string>(), "dQw4w9WgXcQ");
        const Json::JsonArray& photos = body["gallery"].GetArray();
        EXPECT_EQ(IdsOf(photos), galleryIds);
        ASSERT_EQ(photos.size(), 2u);
        EXPECT_EQ(photos[0]["alt_text"].Get<std::string>(), "The stage");

        crow::response bareResp = GetDetail(helper, bare);
        ASSERT_EQ(bareResp.code, 200) << bareResp.body;
        Json::Value bareBody = Json::Value::FromText(bareResp.body);
        EXPECT_TRUE(bareBody["photo"].IsNull());
        EXPECT_TRUE(bareBody["video"].IsNull());
        EXPECT_TRUE(bareBody["gallery"].GetArray().empty());
    });
    Auth::ServerConfig::Shutdown();
}

// CM3 — a pending event's gallery is no more public than its page; a curator's
// preview shows it.
TEST(GetEventDetailTest, APendingEventsGalleryShowsOnlyToACurator) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("APendingEventsGalleryShowsOnlyToACurator", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t pending = AddEvent(transaction, databaseHelper, world,
                                   { "pending", NowUs(transaction) + Events::kUsPerDay, "pending" });
        Events::NewGalleryPhoto photo;
        photo.eventId = pending;
        photo.altText = "The stage";
        photo.consentConfirmed = true;
        int64_t galleryId = Events::EventGalleryHelper(databaseHelper).AddGalleryPhoto(transaction, photo, 1, 12).id;
        TableHelpers::PhotoTest::AttachTestPhoto(
            transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable, galleryId);

        EXPECT_EQ(GetDetail(helper, pending).code, 404);

        SignIn(transaction, testDb, helper, "curator@example.com", true);
        crow::response resp = GetDetail(helper, pending);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(IdsOf(Json::Value::FromText(resp.body)["gallery"].GetArray()),
                  std::vector<int64_t>{ galleryId });
    });
    Auth::ServerConfig::Shutdown();
}

// Signed in but without manage_events is the public view — a 404, not a 403.
TEST(GetEventDetailTest, SignedInWithoutManageEventsGetsThePublicView) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedInWithoutManageEventsGetsThePublicView", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        int64_t pending = AddEvent(transaction, databaseHelper, world, { "pending", tomorrow, "pending" });
        int64_t approved = AddEvent(transaction, databaseHelper, world, { "approved", tomorrow });
        SignIn(transaction, testDb, helper, "member@example.com", false);

        EXPECT_EQ(GetDetail(helper, pending).code, 404);
        crow::response resp = GetDetail(helper, approved);
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_FALSE(HasField(Json::Value::FromText(resp.body), "status"));
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
