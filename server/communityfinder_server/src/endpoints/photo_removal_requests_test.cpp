#include "photo_removal_requests.h"

#include <string>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/events/photo_removal_request_helper.h"
#include "endpoints/events_endpoint_test_support.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/thread_pool.h"

// CM3 Phase 2 §12.3 — POST /api/photo_removal_requests, the public form.
namespace Endpoints {
namespace {

using namespace EventsEndpointTest;

// From `visitor`'s address, signed in or not: the route doesn't care.
crow::response Ask(EndpointTestHelper& helper, const std::string& body,
                   const std::string& visitor = "198.51.100.20") {
    crow::request req;
    req.method = crow::HTTPMethod::Post;
    req.url = "/api/photo_removal_requests";
    req.body = body;
    req.remote_ip_address = visitor;
    crow::response resp;
    helper.GetWebApp().GetApp().handle_full(req, resp);
    ThreadPool::GetInstance().Shutdown();
    return resp;
}

std::string Body(const std::string& extra = "") {
    return R"({"page_path": "/events/12", "page_title": "Board Game Night",)"
           R"( "photo": "photo 2 of 4")" + extra + "}";
}

std::string ErrorCodeIn(const crow::response& resp) {
    return Json::Value::FromText(resp.body)["error_code"].Get<std::string>();
}

// The site address, and a curator who holds manage_events.
void MakeRecipients(Transaction& transaction, TestDatabaseUtil& testDb, EndpointTestHelper& helper) {
    helper.GetSecretsHelper()->AddSecret(transaction, Secrets::kMailSenderAddress, "site@example.org");
    SignIn(transaction, testDb, helper, "curator@example.com", true);
}

TEST(PhotoRemovalRequestsEndpointTest, ARequestIsEmailedToTheSiteAndItsCurators) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    Events::SharedPhotoRemovalRateLimiter().Clear();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ARequestIsEmailedToTheSiteAndItsCurators", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeRecipients(transaction, testDb, helper);
        auto mail = helper.GetMailHelper();
        const size_t before = mail->GetMessages().size();

        crow::response resp = Ask(helper, Body(R"(, "message": "I'm in it.", "reply_email": "visitor@example.com")"));
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_TRUE(Json::Value::FromText(resp.body)["sent"].Get<bool>());
        ASSERT_EQ(mail->GetMessages().size(), before + 2);
        EXPECT_EQ(mail->GetMessages()[before].GetTo()[0].address, "site@example.org");
        EXPECT_EQ(mail->GetMessages()[before + 1].GetTo()[0].address, "curator@example.com");
        const std::string& html = mail->GetMessages()[before].GetBodyHtml();
        EXPECT_NE(html.find("/events/12\">Board Game Night</a>"), std::string::npos);
        EXPECT_NE(html.find("visitor@example.com"), std::string::npos);
    });
    Auth::ServerConfig::Shutdown();
}

// A signed-out visitor has no csrft cookie, so the route is let through the
// CSRF guard without one; no other route is. (handle_full skips the app's
// middleware, so this asks the app's own guard directly.)
TEST(PhotoRemovalRequestsEndpointTest, ASignedOutVisitorGetsPastTheCsrfGuardHereAndNowhereElse) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ASignedOutVisitorGetsPastTheCsrfGuardHereAndNowhereElse", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        CsrfGuard& guard = helper.GetWebApp().GetApp().get_middleware<CsrfGuard>();
        guard.SetEnabled(true);
        EXPECT_TRUE(guard.IsAllowedWithoutToken("/api/photo_removal_requests"));

        const auto refused = [&](const std::string& url) {
            crow::request req;
            req.method = crow::HTTPMethod::Post;
            req.url = url;
            crow::response resp;
            CsrfGuard::context context;
            guard.before_handle(req, resp, context);
            return resp.is_completed();
        };
        EXPECT_FALSE(refused("/api/photo_removal_requests"));
        EXPECT_TRUE(refused("/api/event_submissions"));
        EXPECT_TRUE(refused("/api/event_submissions/1/photo"));
        guard.SetEnabled(false);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(PhotoRemovalRequestsEndpointTest, TheHoneypotLooksLikeSuccessAndSendsNothing) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    Events::SharedPhotoRemovalRateLimiter().Clear();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheHoneypotLooksLikeSuccessAndSendsNothing", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeRecipients(transaction, testDb, helper);
        auto mail = helper.GetMailHelper();
        const size_t before = mail->GetMessages().size();

        crow::response resp = Ask(helper, Body(R"(, "website": "http://spam.example")"), "198.51.100.21");
        ASSERT_EQ(resp.code, 200) << resp.body;
        EXPECT_TRUE(Json::Value::FromText(resp.body)["sent"].Get<bool>());
        EXPECT_EQ(mail->GetMessages().size(), before);
    });
    Auth::ServerConfig::Shutdown();
}

TEST(PhotoRemovalRequestsEndpointTest, OneVisitorIsLimitedToThreeAnHour) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    Events::SharedPhotoRemovalRateLimiter().Clear();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OneVisitorIsLimitedToThreeAnHour", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeRecipients(transaction, testDb, helper);

        for (int64_t i = 0; i < Events::kPhotoRemovalMaxPerVisitorPerHour; ++i) {
            ASSERT_EQ(Ask(helper, Body(), "198.51.100.22").code, 200) << i;
        }
        crow::response limited = Ask(helper, Body(), "198.51.100.22");
        ASSERT_EQ(limited.code, 429) << limited.body;
        EXPECT_EQ(ErrorCodeIn(limited), Events::kPhotoRemovalErrorRateLimited);
        // Someone else can still ask.
        EXPECT_EQ(Ask(helper, Body(), "198.51.100.23").code, 200);
    });
    Events::SharedPhotoRemovalRateLimiter().Clear();
    Auth::ServerConfig::Shutdown();
}

TEST(PhotoRemovalRequestsEndpointTest, AFormThatFailsItsChecksIsA400ThatNamesTheField) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    Events::SharedPhotoRemovalRateLimiter().Clear();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AFormThatFailsItsChecksIsA400ThatNamesTheField", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        MakeRecipients(transaction, testDb, helper);
        auto mail = helper.GetMailHelper();
        const size_t before = mail->GetMessages().size();

        const std::string longMessage(Events::kPhotoRemovalMessageMaxLength + 1, 'a');
        crow::response tooLong = Ask(helper, Body(R"(, "message": ")" + longMessage + "\""));
        ASSERT_EQ(tooLong.code, 400) << tooLong.body;
        EXPECT_EQ(ErrorCodeIn(tooLong), Events::kPhotoRemovalErrorTooLong);
        EXPECT_EQ(Json::Value::FromText(tooLong.body)["field"].Get<std::string>(), "message");

        crow::response offSite = Ask(helper,
            R"({"page_path": "https://elsewhere.example/", "page_title": "x", "photo": "photo 1 of 1"})");
        ASSERT_EQ(offSite.code, 400) << offSite.body;
        EXPECT_EQ(ErrorCodeIn(offSite), Events::kPhotoRemovalErrorBadPage);

        crow::response notAString = Ask(helper, Body(R"(, "reply_email": 7)"));
        ASSERT_EQ(notAString.code, 400) << notAString.body;
        EXPECT_EQ(Json::Value::FromText(notAString.body)["field"].Get<std::string>(), "reply_email");

        EXPECT_EQ(Ask(helper, "not json").code, 400);
        EXPECT_EQ(mail->GetMessages().size(), before);
        // None of them counted against the visitor.
        for (int64_t i = 0; i < Events::kPhotoRemovalMaxPerVisitorPerHour; ++i) {
            EXPECT_EQ(Ask(helper, Body()).code, 200) << i;
        }
    });
    Events::SharedPhotoRemovalRateLimiter().Clear();
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
