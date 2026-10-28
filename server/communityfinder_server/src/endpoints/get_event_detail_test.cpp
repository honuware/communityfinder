#include "get_event_detail.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/types.h"

// EV1 §5.2 — GET /api/events/<int>: the public sees approved events only; a
// manage_events holder previews any status.
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
