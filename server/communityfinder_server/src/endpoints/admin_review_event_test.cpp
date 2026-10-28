#include "admin_review_event.h"

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/events.h"
#include "endpoints/events_endpoint_test_support.h"
#include "sql_util/table_helpers/events.h"
#include "util/types.h"

// EV1 §5.5 — POST /api/admin/review_event/<int>: approve / reject.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

crow::response Review(EndpointTestHelper& helper, int64_t eventId, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Post,
                   "/api/admin/review_event/" + StringFromInt(eventId), body);
}

std::set<int64_t> UpcomingIds(EndpointTestHelper& helper) {
    crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/events/upcoming");
    EXPECT_EQ(resp.code, 200) << resp.body;
    std::vector<int64_t> ids = IdsOf(Json::Value::FromText(resp.body)["items"].GetArray());
    return std::set<int64_t>(ids.begin(), ids.end());
}

TEST(AdminReviewEventTest, ApprovingPublishesAndStampsTheReviewer) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ApprovingPublishesAndStampsTheReviewer", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        int64_t curatorId = SignIn(transaction, testDb, helper, "curator@example.com", true);
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 0u);

        crow::response resp = Review(helper, eventId, R"({"action": "approve", "notes": "checked"})");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["event_id"].Get<int64_t>(), eventId);
        EXPECT_EQ(body["status"].Get<std::string>(), "approved");

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedByPersonId)), StringFromInt(curatorId));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewNotes)), "checked");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventsReviewedUs))), 0);
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 1u);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, RejectingHidesAnApprovedEvent) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RejectingHidesAnApprovedEvent", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay });
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 1u);

        crow::response resp = Review(helper, eventId, R"({"action": "reject"})");
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(Json::Value::FromText(resp.body)["status"].Get<std::string>(), "rejected");
        EXPECT_EQ(UpcomingIds(helper).count(eventId), 0u);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, ARepeatIsAnInvalidTransition) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ARepeatIsAnInvalidTransition", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay });
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        crow::response resp = Review(helper, eventId, R"({"action": "approve"})");
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kEventErrorInvalidTransition);
        EXPECT_EQ(body["status"].Get<std::string>(), "approved");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, UnknownEventsAndBadBodies) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownEventsAndBadBodies", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        EXPECT_EQ(Review(helper, 999999, R"({"action": "approve"})").code, 404);
        crow::response badAction = Review(helper, eventId, R"({"action": "publish"})");
        ASSERT_EQ(badAction.code, 400) << badAction.body;
        EXPECT_EQ(Json::Value::FromText(badAction.body)["field"].Get<std::string>(), "action");
        crow::response badNotes = Review(helper, eventId, R"({"action": "approve", "notes": 3})");
        ASSERT_EQ(badNotes.code, 400) << badNotes.body;
        EXPECT_EQ(Json::Value::FromText(badNotes.body)["field"].Get<std::string>(), "notes");
        EXPECT_EQ(Review(helper, eventId, "not json").code, 400);

        // None of those touched the event.
        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminReviewEventTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, world,
                                   { "a", NowUs(transaction) + Events::kUsPerDay, "pending" });
        const std::string approve = R"({"action": "approve"})";

        EXPECT_EQ(Review(helper, eventId, approve).code, 401);
        // The permission exists (a curator holds it); this member doesn't.
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(Review(helper, eventId, approve).code, 403);

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
