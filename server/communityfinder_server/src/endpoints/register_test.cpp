#include "endpoints/register.h"

#include <string>

#include <gtest/gtest.h>

#include "endpoints/endpoint_test_helper.h"
#include "endpoints/web_app.h"
#include "db_schema/people.h"
#include "sql_util/table_helpers/people.h"
#include "util/json_value.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper_test_util.h"
#include "util/types.h"

namespace Endpoints {
namespace {

// The /api/register endpoint is honuware framework code (extracted in 0.2). Unlike
// knottyyoga, CommunityFinder registers NO PostRegisterHook (no gift-invitation
// domain), so registration is the plain framework path: create the (unverified)
// person + send the verification email. These app-side tests cover that path in
// CF's build + the CF open-registration policy (a fresh account has no roles).
void AddRegistrationSecrets(Transaction& transaction, EndpointTestHelper& helper) {
    auto secrets = helper.GetSecretsHelper();
    secrets->AddSecret(transaction, Secrets::kWebsiteAddress, "http://localhost:4201");
    secrets->AddSecret(transaction, Secrets::kWebsiteApiLinkPrefix, "http://localhost:18081");
    secrets->AddSecret(transaction, Secrets::kWebsiteActivationLink,
        "{website_address}/verify?email={email}&secret={encoded_secret}");
    secrets->AddSecret(transaction, Secrets::kMailSenderName, "CommunityFinder");
    secrets->AddSecret(transaction, Secrets::kMailSenderAddress, "noreply@communityfinder.local");
    secrets->AddSecret(transaction, Secrets::kMailActivationEmailSubject, "Verify your email");
}

std::string BuildRegisterBody(std::string_view firstName, std::string_view lastName,
                              std::string_view email, std::string_view password) {
    Json::Value body(Json::JsonObject{
        {"first_name", std::string(firstName)},
        {"last_name", std::string(lastName)},
        {"email", std::string(email)},
        {"password", std::string(password)},
    });
    return body.ToString();
}

void IssueRegister(EndpointTestHelper& helper, std::string_view firstName,
                   std::string_view lastName, std::string_view email,
                   std::string_view password, crow::response& resp) {
    crow::request req;
    req.method = crow::HTTPMethod::Post;
    req.url = "/api/register";
    req.body = BuildRegisterBody(firstName, lastName, email, password);
    helper.GetWebApp().GetApp().handle_full(req, resp);
}

TEST(CommunityFinderRegisterTest, RegisterCreatesUnverifiedUserAndSendsEmail) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFRegisterCreatesUnverifiedUserAndSendsEmail",
        [&](Transaction& transaction) {
            EndpointTestHelper helper(transaction, testDb);
            AddRegistrationSecrets(transaction, helper);

            crow::response resp;
            IssueRegister(helper, "Ada", "Lovelace", "ada_at_example.com",
                          "Password123!", resp);
            ASSERT_EQ(resp.code, 200) << resp.body;

            TableHelpers::People people(testDb.GetDatabaseHelper());
            KeyValueTable person =
                people.LookupPersonByEmail(transaction, "ada_at_example.com");
            ASSERT_FALSE(person.empty());
            // Not verified until the /api/verify step.
            EXPECT_TRUE(person.at(std::string(DbSchema::kPeopleEmailVerifiedAt)).empty());

            auto mail = helper.GetMailHelper();
            ASSERT_TRUE(mail);
            EXPECT_EQ(mail->GetMessages().size(), 1u);
        });
}

// Open registration (Q7): a fresh account carries NO roles — non-admin by default.
TEST(CommunityFinderRegisterTest, FreshRegistrationHasNoRoles) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFFreshRegistrationHasNoRoles",
        [&](Transaction& transaction) {
            EndpointTestHelper helper(transaction, testDb);
            AddRegistrationSecrets(transaction, helper);

            crow::response resp;
            IssueRegister(helper, "Grace", "Hopper", "grace_at_example.com",
                          "Password123!", resp);
            ASSERT_EQ(resp.code, 200) << resp.body;

            TableHelpers::People people(testDb.GetDatabaseHelper());
            KeyValueTable person =
                people.LookupPersonByEmail(transaction, "grace_at_example.com");
            ASSERT_FALSE(person.empty());
            const std::string personId = person.at(std::string(DbSchema::kPeopleId));

            const std::string roleCount = transaction.RunSqlStatementReturningOneValue(
                "SELECT count(*) FROM role_assignments WHERE person_id = $1", personId);
            EXPECT_EQ(roleCount, "0");
        });
}

// A missing password is rejected up front — no account with an empty password.
TEST(CommunityFinderRegisterTest, RegisterMissingPasswordRejected) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFRegisterMissingPasswordRejected",
        [&](Transaction& transaction) {
            EndpointTestHelper helper(transaction, testDb);
            AddRegistrationSecrets(transaction, helper);

            Json::Value body(Json::JsonObject{
                {"first_name", std::string("No")},
                {"last_name", std::string("Password")},
                {"email", std::string("nopw_at_example.com")},
            });
            crow::request req;
            req.method = crow::HTTPMethod::Post;
            req.url = "/api/register";
            req.body = body.ToString();
            crow::response resp;
            helper.GetWebApp().GetApp().handle_full(req, resp);

            EXPECT_EQ(resp.code, 400);
            TableHelpers::People people(testDb.GetDatabaseHelper());
            EXPECT_TRUE(
                people.LookupPersonByEmail(transaction, "nopw_at_example.com").empty());
        });
}

}  // namespace
}  // namespace Endpoints
