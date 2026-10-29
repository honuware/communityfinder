#include "get_guide_pages.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "endpoints/guide_endpoint_test_support.h"

// GD2 §4.9 — GET /api/guide/pages/<slug> (the publish rule over HTTP, and the
// manage_guide preview) and /api/guide/pages?section=.
namespace Endpoints {
namespace {

using namespace GuideEndpointTest;
using Slugs = std::vector<std::string>;

TEST(GetGuidePagesTest, APublishedPageServesToEveryone) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("APublishedPageServesToEveryone", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/pages/health-basics");
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_EQ(resp.get_header_value("Content-Type"), "application/json");
        Json::Value page = Json::Value::FromText(resp.body);
        EXPECT_EQ(page["id"].Get<int64_t>(), world.healthBasics);
        EXPECT_EQ(page["title"].Get<std::string>(), "Title of health-basics");
        EXPECT_EQ(page["section"].Get<std::string>(), "health");
        EXPECT_EQ(page["body_markdown"].Get<std::string>(), "Body of health-basics");
        EXPECT_EQ(page["adult"].Get<bool>(), false);
        EXPECT_EQ(page["draft"].Get<bool>(), false);
        EXPECT_EQ(page["published"].Get<bool>(), true);
        EXPECT_TRUE(page["publish_at_us"].Is<int64_t>());
        EXPECT_EQ(Strings(page["scene_tag_slugs"]), Slugs{ "leather" });
        EXPECT_TRUE(page["status"].Is<std::string>());
        EXPECT_EQ(page["last_verified_at_us"].Get<std::string>(), "");
    });
    Auth::ServerConfig::Shutdown();
}

// A draft and a page scheduled for later are both 404 — the same answer as a
// missing slug — to visitors and to people without manage_guide.
TEST(GetGuidePagesTest, UnpublishedPagesAre404WithoutManageGuide) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnpublishedPagesAre404WithoutManageGuide", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeWorld(transaction, testDb.GetDatabaseHelper());

        EXPECT_EQ(Get(helper, "/api/guide/pages/draft-page").code, 404);
        EXPECT_EQ(Get(helper, "/api/guide/pages/scheduled-page").code, 404);
        EXPECT_EQ(Get(helper, "/api/guide/pages/no-such-page").code, 404);

        SignIn(transaction, testDb, helper, "reader@example.com", false);
        EXPECT_EQ(Get(helper, "/api/guide/pages/draft-page").code, 404);
        EXPECT_EQ(Get(helper, "/api/guide/pages/scheduled-page").code, 404);
        EXPECT_EQ(Get(helper, "/api/guide/pages/health-basics").code, 200);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuidePagesTest, ManageGuidePreviewsUnpublishedPages) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ManageGuidePreviewsUnpublishedPages", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());
        SignIn(transaction, testDb, helper, "curator@example.com", true);

        crow::response draft = Get(helper, "/api/guide/pages/draft-page");
        ASSERT_EQ(draft.code, 200) << draft.body;
        Json::Value draftPage = Json::Value::FromText(draft.body);
        EXPECT_EQ(draftPage["id"].Get<int64_t>(), world.draftPage);
        EXPECT_EQ(draftPage["body_markdown"].Get<std::string>(), "Body of draft-page");
        EXPECT_EQ(draftPage["draft"].Get<bool>(), true);
        EXPECT_EQ(draftPage["published"].Get<bool>(), false);  // the preview marker

        crow::response scheduled = Get(helper, "/api/guide/pages/scheduled-page");
        ASSERT_EQ(scheduled.code, 200) << scheduled.body;
        Json::Value scheduledPage = Json::Value::FromText(scheduled.body);
        EXPECT_EQ(scheduledPage["draft"].Get<bool>(), false);
        EXPECT_EQ(scheduledPage["published"].Get<bool>(), false);

        crow::response published = Get(helper, "/api/guide/pages/health-basics");
        ASSERT_EQ(published.code, 200) << published.body;
        EXPECT_EQ(Json::Value::FromText(published.body)["published"].Get<bool>(), true);

        EXPECT_EQ(Get(helper, "/api/guide/pages/no-such-page").code, 404);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(GetGuidePagesTest, ASectionListsItsPublishedPagesWithoutBodies) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASectionListsItsPublishedPagesWithoutBodies", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        World world = MakeWorld(transaction, testDb.GetDatabaseHelper());

        crow::response resp = Get(helper, "/api/guide/pages", "section=health");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        // The draft and the scheduled page are left out.
        ASSERT_EQ(SlugsOf(body["items"]), Slugs{ "health-basics" });
        const Json::Value& entry = body["items"].GetArray()[0];
        EXPECT_EQ(entry["id"].Get<int64_t>(), world.healthBasics);
        EXPECT_EQ(entry["title"].Get<std::string>(), "Title of health-basics");
        EXPECT_EQ(entry["adult"].Get<bool>(), false);
        EXPECT_EQ(entry["last_verified_at_us"].Get<std::string>(), "");
        EXPECT_FALSE(entry.HasChild("body_markdown", nullptr));

        crow::response empty = Get(helper, "/api/guide/pages", "section=freeze");
        ASSERT_EQ(empty.code, 200) << empty.body;
        EXPECT_TRUE(Json::Value::FromText(empty.body)["items"].GetArray().empty());

        EXPECT_EQ(Get(helper, "/api/guide/pages").code, 400);
        EXPECT_EQ(Get(helper, "/api/guide/pages", "section=").code, 400);

        // Published only, for curators too: previews are one page at a time.
        SignIn(transaction, testDb, helper, "curator@example.com", true);
        crow::response curator = Get(helper, "/api/guide/pages", "section=health");
        ASSERT_EQ(curator.code, 200) << curator.body;
        EXPECT_EQ(SlugsOf(Json::Value::FromText(curator.body)["items"]), Slugs{ "health-basics" });
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
