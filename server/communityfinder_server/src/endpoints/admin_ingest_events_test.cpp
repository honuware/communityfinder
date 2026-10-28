#include "admin_ingest_events.h"

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "db_schema/events.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/types.h"

// EV1 §5.4 — POST /api/admin/ingest_events: the scanner's single door.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

Json::Value Item(const World& world, std::string_view externalKey, int64_t startsAtUs) {
    return Json::Value(Json::JsonObject{
        { "source_id", Json::Value(world.feed) },
        { "external_key", Json::Value(std::string(externalKey)) },
        { "title", Json::Value("Event " + std::string(externalKey)) },
        { "starts_at_us", Json::Value(startsAtUs) },
    });
}

std::string Body(const Json::JsonArray& items) {
    return Json::Value(Json::JsonObject{ { "items", Json::Value(items) } }).ToString();
}

crow::response Ingest(EndpointTestHelper& helper, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Post, "/api/admin/ingest_events", body);
}

int64_t EventCount(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        "SELECT count(*) FROM " + std::string(DbSchema::kEventsTable)));
}

std::vector<std::string> Strings(const Json::Value& array) {
    std::vector<std::string> strings;
    for (const Json::Value& value : array.GetArray()) {
        strings.push_back(value.Get<std::string>());
    }
    return strings;
}

TEST(AdminIngestEventsTest, ASecondIdenticalPassIsAllUnchanged) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASecondIdenticalPassIsAllUnchanged", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        SignIn(transaction, testDb, helper, "scanner@example.com", true);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        std::string body = Body({ Item(world, "a", tomorrow), Item(world, "b", tomorrow) });

        crow::response first = Ingest(helper, body);
        ASSERT_EQ(first.code, 200) << first.body;
        Json::Value firstBody = Json::Value::FromText(first.body);
        const Json::Value& firstCounts = firstBody["counts"];
        EXPECT_EQ(firstCounts["created"].Get<int64_t>(), 2);
        // Every outcome is present, zeros included.
        for (const char* outcome : { "updated", "unchanged", "reverted_to_pending",
                                     "rejected_kept", "archived_kept" }) {
            EXPECT_EQ(firstCounts[outcome].Get<int64_t>(), 0) << outcome;
        }
        // Results are parallel to the request's items.
        const Json::JsonArray& firstItems = firstBody["items"].GetArray();
        ASSERT_EQ(firstItems.size(), 2u);
        EXPECT_EQ(firstItems[0]["outcome"].Get<std::string>(), "created");

        crow::response second = Ingest(helper, body);
        ASSERT_EQ(second.code, 200) << second.body;
        Json::Value secondBody = Json::Value::FromText(second.body);
        EXPECT_EQ(secondBody["counts"]["unchanged"].Get<int64_t>(), 2);
        EXPECT_EQ(secondBody["counts"]["created"].Get<int64_t>(), 0);
        const Json::JsonArray& secondItems = secondBody["items"].GetArray();
        ASSERT_EQ(secondItems.size(), 2u);
        EXPECT_EQ(secondItems[0]["event_id"].Get<int64_t>(), firstItems[0]["event_id"].Get<int64_t>());
        EXPECT_EQ(secondItems[1]["event_id"].Get<int64_t>(), firstItems[1]["event_id"].Get<int64_t>());

        // Ingested events wait for review.
        crow::response detail = Request(
            helper, crow::HTTPMethod::Get,
            "/api/events/" + StringFromInt(firstItems[0]["event_id"].Get<int64_t>()));
        ASSERT_EQ(detail.code, 200) << detail.body;
        EXPECT_EQ(Json::Value::FromText(detail.body)["status"].Get<std::string>(), "pending");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminIngestEventsTest, UnknownSlugsAreReportedNotFatal) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownSlugsAreReportedNotFatal", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        SignIn(transaction, testDb, helper, "scanner@example.com", true);
        Json::Value item = Item(world, "slugs", NowUs(transaction) + Events::kUsPerDay);
        item["category_slugs"] = Json::Value(Json::JsonArray{ Json::Value("drag"), Json::Value("nope") });
        item["scene_tag_slugs"] = Json::Value(Json::JsonArray{ Json::Value("ghost") });
        item["series_slug"] = Json::Value("no-such-series");

        crow::response resp = Ingest(helper, Body({ item }));
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        const Json::Value& result = body["items"].GetArray().at(0);
        EXPECT_EQ(result["outcome"].Get<std::string>(), "created");
        EXPECT_EQ(Strings(result["ignored_category_slugs"]), std::vector<std::string>{ "nope" });
        EXPECT_EQ(Strings(result["ignored_scene_tag_slugs"]), std::vector<std::string>{ "ghost" });
        EXPECT_EQ(result["ignored_series_slug"].Get<std::string>(), "no-such-series");
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminIngestEventsTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        std::string body = Body({ Item(world, "a", NowUs(transaction) + Events::kUsPerDay) });

        EXPECT_EQ(Ingest(helper, body).code, 401);
        // The permission exists (a curator holds it); this member doesn't.
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(Ingest(helper, body).code, 403);
        EXPECT_EQ(EventCount(transaction), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminIngestEventsTest, AMalformedItemFailsTheBatchWithItsIndex) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AMalformedItemFailsTheBatchWithItsIndex", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        SignIn(transaction, testDb, helper, "scanner@example.com", true);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        Json::Value badType = Item(world, "b", tomorrow);
        badType["starts_at_us"] = Json::Value("tomorrow");

        crow::response resp = Ingest(helper, Body({ Item(world, "a", tomorrow), badType }));
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["item_index"].Get<int64_t>(), 1);
        EXPECT_EQ(body["detail"].Get<std::string>(), "starts_at_us must be an integer");
        EXPECT_FALSE(body.HasChild("error_code"));
        EXPECT_EQ(EventCount(transaction), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminIngestEventsTest, ABusinessFailureCarriesItsErrorCode) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ABusinessFailureCarriesItsErrorCode", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        SignIn(transaction, testDb, helper, "scanner@example.com", true);
        int64_t tomorrow = NowUs(transaction) + Events::kUsPerDay;
        Json::Value untitled = Item(world, "b", tomorrow);
        untitled.GetChildren().erase("title");

        crow::response resp = Ingest(helper, Body({ Item(world, "a", tomorrow), untitled }));
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kEventErrorMissingTitle);
        EXPECT_EQ(body["item_index"].Get<int64_t>(), 1);
        EXPECT_EQ(EventCount(transaction), 0);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminIngestEventsTest, ABodyThatIsNotABatchIs400) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ABodyThatIsNotABatchIs400", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "scanner@example.com", true);

        EXPECT_EQ(Ingest(helper, "not json").code, 400);
        crow::response noItems = Ingest(helper, R"({"items": "a"})");
        ASSERT_EQ(noItems.code, 400) << noItems.body;
        EXPECT_EQ(Json::Value::FromText(noItems.body)["field"].Get<std::string>(), "items");
        crow::response notAnObject = Ingest(helper, R"({"items": [7]})");
        ASSERT_EQ(notAnObject.code, 400) << notAnObject.body;
        EXPECT_EQ(Json::Value::FromText(notAnObject.body)["item_index"].Get<int64_t>(), 0);
        // An empty batch is fine and does nothing.
        crow::response empty = Ingest(helper, R"({"items": []})");
        ASSERT_EQ(empty.code, 200) << empty.body;
        EXPECT_TRUE(Json::Value::FromText(empty.body)["items"].GetArray().empty());
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
