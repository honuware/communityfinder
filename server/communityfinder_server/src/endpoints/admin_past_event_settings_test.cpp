#include "admin_past_event_settings.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/past_event_settings.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/types.h"

// EV4 §10 — GET / PUT /api/admin/past_event_settings: the past-events window and
// the calendar's default.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

constexpr const char* kSettingsUrl = "/api/admin/past_event_settings";

crow::response GetSettings(EndpointTestHelper& helper) {
    return Request(helper, crow::HTTPMethod::Get, kSettingsUrl);
}

crow::response PutSettings(EndpointTestHelper& helper, const std::string& body) {
    return Request(helper, crow::HTTPMethod::Put, kSettingsUrl, body);
}

void ExpectSettings(const crow::response& resp, int64_t visibleDays, bool showPast) {
    ASSERT_EQ(resp.code, 200) << resp.body;
    const Json::Value body = Json::Value::FromText(resp.body);
    const Json::Value& days = body[Events::kPastEventSettingVisibleDays];
    EXPECT_EQ(days["value"].Get<int64_t>(), visibleDays);
    EXPECT_EQ(days["default"].Get<int64_t>(), 365);
    EXPECT_EQ(days["min"].Get<int64_t>(), Events::kArchivedEventsVisibleDaysRange.min);
    EXPECT_EQ(days["max"].Get<int64_t>(), Events::kArchivedEventsVisibleDaysRange.max);
    const Json::Value& show = body[Events::kPastEventSettingShowByDefault];
    EXPECT_EQ(show["value"].Get<bool>(), showPast);
    EXPECT_FALSE(show["default"].Get<bool>());
}

TEST(AdminPastEventSettingsTest, SignedOutIs401AndWithoutManageEventsIs403) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SignedOutIs401AndWithoutManageEventsIs403", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        const std::string days = R"({"archived_events_visible_days": 30})";

        EXPECT_EQ(GetSettings(helper).code, 401);
        EXPECT_EQ(PutSettings(helper, days).code, 401);
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        SignIn(transaction, testDb, helper, "member@example.com", false);
        EXPECT_EQ(GetSettings(helper).code, 403);
        EXPECT_EQ(PutSettings(helper, days).code, 403);

        SwitchTo(transaction, testDb, helper, "curator@example.com");
        ExpectSettings(GetSettings(helper), 365, false);
    });
    Auth::ServerConfig::Shutdown();
}

// Each setting saves on its own and leaves the other.
TEST(AdminPastEventSettingsTest, EachSettingSavesOnItsOwn) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachSettingSavesOnItsOwn", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        ExpectSettings(PutSettings(helper, R"({"archived_events_visible_days": 90})"), 90, false);
        ExpectSettings(PutSettings(helper, R"({"events_show_past_by_default": true})"), 90, true);
        ExpectSettings(GetSettings(helper), 90, true);
        // null is "not given", like an absent field.
        ExpectSettings(PutSettings(
                           helper,
                           R"({"archived_events_visible_days": null, "events_show_past_by_default": false})"),
                       90, false);
        // The public read sees the change.
        crow::response publicRead = Request(helper, crow::HTTPMethod::Get, "/api/events/settings");
        ASSERT_EQ(publicRead.code, 200);
        EXPECT_EQ(Json::Value::FromText(publicRead.body)["archived_events_visible_days"].Get<int64_t>(), 90);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(AdminPastEventSettingsTest, BadValuesAre400AndWriteNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BadValuesAre400AndWriteNothing", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        for (const std::string outOfRange : { "0", "731" }) {
            crow::response resp = PutSettings(
                helper, R"({"archived_events_visible_days": )" + outOfRange
                            + R"(, "events_show_past_by_default": true})");
            ASSERT_EQ(resp.code, 400) << outOfRange << ": " << resp.body;
            Json::Value body = Json::Value::FromText(resp.body);
            EXPECT_EQ(body["error_code"].Get<std::string>(), Events::kPastEventErrorDaysOutOfRange);
            EXPECT_EQ(body["field"].Get<std::string>(), Events::kPastEventSettingVisibleDays);
        }
        crow::response notANumber = PutSettings(helper, R"({"archived_events_visible_days": "a year"})");
        ASSERT_EQ(notANumber.code, 400) << notANumber.body;
        EXPECT_EQ(Json::Value::FromText(notANumber.body)["field"].Get<std::string>(),
                  Events::kPastEventSettingVisibleDays);
        crow::response notABoolean = PutSettings(helper, R"({"events_show_past_by_default": "true"})");
        ASSERT_EQ(notABoolean.code, 400) << notABoolean.body;
        EXPECT_EQ(Json::Value::FromText(notABoolean.body)["detail"].Get<std::string>(),
                  "events_show_past_by_default must be true or false");
        EXPECT_EQ(PutSettings(helper, "{}").code, 400);
        EXPECT_EQ(PutSettings(helper, "not json").code, 400);

        ExpectSettings(GetSettings(helper), 365, false);
    });
    Auth::ServerConfig::Shutdown();
}

// A shorter window takes effect on the next public read: the page of an event
// that falls outside it is gone, and so is its place in the past list.
TEST(AdminPastEventSettingsTest, AShorterWindowAppliesToTheNextRead) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AShorterWindowAppliesToTheNextRead", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        const int64_t nowUs = NowUs(transaction);
        int64_t lastWeek = AddEvent(transaction, databaseHelper, world,
                                    { "last-week", nowUs - 7 * Events::kUsPerDay, "archived" });
        int64_t lastMonth = AddEvent(transaction, databaseHelper, world,
                                     { "last-month", nowUs - 40 * Events::kUsPerDay, "archived" });
        auto pastIds = [&] {
            crow::response resp = Request(helper, crow::HTTPMethod::Get, "/api/events/past");
            EXPECT_EQ(resp.code, 200) << resp.body;
            return IdsOf(Json::Value::FromText(resp.body)["items"].GetArray());
        };
        EXPECT_EQ(pastIds(), (std::vector<int64_t>{ lastWeek, lastMonth }));

        SignIn(transaction, testDb, helper, "curator@example.com", true);
        ExpectSettings(PutSettings(helper, R"({"archived_events_visible_days": 30})"), 30, false);
        EXPECT_EQ(pastIds(), std::vector<int64_t>{ lastWeek });
        // A curator still previews it; the public would get a 404 (the detail
        // endpoint's own test signs out to show that).
        EXPECT_EQ(Request(helper, crow::HTTPMethod::Get, "/api/events/" + StringFromInt(lastMonth)).code,
                  200);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
