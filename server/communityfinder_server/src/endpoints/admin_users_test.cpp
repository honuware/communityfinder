#include <cstdint>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "endpoints/endpoint_test_helper.h"
#include "endpoints/web_app.h"
#include "business_logic/auth/cookie_manager_test_util.h"
#include "business_logic/auth/person.h"
#include "business_logic/auth/server_config.h"
#include "db_schema/people.h"
#include "db_schema/roles.h"
#include "sql_util/table_helpers/people.h"
#include "sql_util/table_helpers/permissions.h"
#include "sql_util/table_helpers/role_assignments.h"
#include "sql_util/table_helpers/role_permissions.h"
#include "sql_util/table_helpers/roles.h"
#include "test/src/util/database_test_helper.h"
#include "util/json_value.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper.h"
#include "util/secrets/secrets_helper_test_util.h"

// The bespoke admin user-management endpoints (Phase 9.2):
//   POST /api/admin/create_user    — QuickAccountHelper: temp password + welcome email
//   POST /api/admin/reset_password — fresh temp password + must_change + email
// Both gate on the `admin_portal` permission the framework grants the Administrator
// role. These CF tests drive the full HTTP flow: an admin creates/resets and the
// effect lands in the DB with an email queued (captured by the test mail helper), and
// a non-admin is refused with 403.
namespace Endpoints {
namespace {

struct TestSession {
    int64_t personId;
    std::string token;
};

void EnsureSessionSecret(Transaction& transaction, EndpointTestHelper& helper) {
    helper.GetSecretsHelper()->AddSecret(transaction,
        Secrets::kAuthSessionMaxDuractioninMicros,
        std::to_string(30LL * 60LL * 1000000LL));
}

// Branding for the welcome / reset emails. LoadTenantBranding reads these; absent,
// the message still queues, but this mirrors production and register_test.
void AddMailSecrets(Transaction& transaction, EndpointTestHelper& helper) {
    auto secrets = helper.GetSecretsHelper();
    secrets->AddSecret(transaction, Secrets::kMailSenderName, "CommunityFinder");
    secrets->AddSecret(transaction, Secrets::kMailSenderAddress,
        "noreply@communityfinder.local");
}

int64_t LookupPersonId(Transaction& transaction, TestDatabaseUtil& testDb,
                       std::string_view email) {
    TableHelpers::People people(testDb.GetDatabaseHelper());
    return std::stoll(people.LookupPersonByEmail(transaction, std::string(email))
                          .at(std::string(DbSchema::kPeopleId)));
}

// Create a fully-validated person + session token. When `admin`, also create the
// `admin` role, assign it, and grant it `admin_portal` — the test harness doesn't run
// the framework's PopulateFrameworkTables seeding, so the permission grant that
// production gets for free is set up here. Call with admin=true at most once per test
// (the role name is unique). Does NOT set the cookie — use ActAs().
TestSession SetupPerson(Transaction& transaction, TestDatabaseUtil& testDb,
                        EndpointTestHelper& helper, std::string_view email,
                        std::string_view password, bool admin) {
    Auth::PersonHelper personHelper(testDb.GetDatabaseHelper());
    Auth::PersonInfo info{std::string(email), "Test", "User"};
    personHelper.CreateFullyValidatedUser(transaction, info, std::string(password));
    int64_t personId = LookupPersonId(transaction, testDb, email);

    if (admin) {
        TableHelpers::Roles roles(testDb.GetDatabaseHelper());
        int64_t adminRoleId =
            roles.AddRole(transaction, std::string(DbSchema::kRoleNameAdmin), "Administrator");
        TableHelpers::RoleAssignments roleAssignments(testDb.GetDatabaseHelper());
        roleAssignments.AddRoleAssignment(transaction, personId, adminRoleId);
        TableHelpers::Permissions permissions(testDb.GetDatabaseHelper());
        int64_t permId = permissions.AddPermission(
            transaction, "admin_portal", "Permission to manage user accounts.");
        TableHelpers::RolePermissions rolePermissions(testDb.GetDatabaseHelper());
        rolePermissions.AddRolePermission(transaction, adminRoleId, permId);
    }

    std::string token;
    EXPECT_TRUE(personHelper.CreateSessionToken(
        transaction, helper.GetSecretsHelper(), info.email, token));
    return {personId, token};
}

void ActAs(EndpointTestHelper& helper, const std::string& token) {
    auto cookieManager = helper.GetCookieManagerTest();
    Auth::CookieProperties cookieProps;
    cookieProps.path = "/";
    cookieProps.sameSite = Auth::CookieSameSitePolicy::None;
    cookieManager->SetCookie("session_token", token, cookieProps);
}

crow::response Handle(EndpointTestHelper& helper, crow::HTTPMethod method,
                      const std::string& url, const std::string& body = "") {
    crow::request req;
    req.method = method;
    req.url = url;
    req.body = body;
    crow::response resp;
    helper.GetWebApp().GetApp().handle_full(req, resp);
    return resp;
}

std::string CreateUserBody(std::string_view first, std::string_view last,
                           std::string_view email) {
    Json::Value body(Json::JsonObject{
        {"first_name", std::string(first)},
        {"last_name", std::string(last)},
        {"email", std::string(email)},
    });
    return body.ToString();
}

std::string ResetBody(std::string_view email) {
    Json::Value body(Json::JsonObject{{"email", std::string(email)}});
    return body.ToString();
}

// Admin creates a user → the account exists, a welcome email carrying the temporary
// password is queued, already_exists=false. A second create with the same email
// reuses the account (already_exists=true, no duplicate, no second email).
TEST(CommunityFinderAdminUsersTest, AdminCreateUserCreatesAccountAndEmails) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFAdminCreateUser", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        EnsureSessionSecret(transaction, helper);
        AddMailSecrets(transaction, helper);

        TestSession admin = SetupPerson(transaction, testDb, helper,
            "admin@example.com", "Password123!", true);
        ActAs(helper, admin.token);

        crow::response resp = Handle(helper, crow::HTTPMethod::Post,
            "/api/admin/create_user",
            CreateUserBody("Ada", "Lovelace", "ada@example.com"));
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value result = Json::Value::FromText(resp.body);
        EXPECT_FALSE(result["already_exists"].Get<bool>());
        EXPECT_EQ(result["first_name"].Get<std::string>(), "Ada");
        const int64_t createdId = result["person_id"].Get<int64_t>();

        // The account now exists.
        TableHelpers::People people(testDb.GetDatabaseHelper());
        EXPECT_FALSE(people.LookupPersonByEmail(transaction, "ada@example.com").empty());

        // A welcome email (the temporary password) was queued.
        auto mail = helper.GetMailHelper();
        ASSERT_TRUE(mail);
        EXPECT_EQ(mail->GetMessages().size(), 1u);

        // Creating again with the same email reuses the account, no duplicate.
        crow::response resp2 = Handle(helper, crow::HTTPMethod::Post,
            "/api/admin/create_user",
            CreateUserBody("Ada", "Lovelace", "ada@example.com"));
        ASSERT_EQ(resp2.code, 200) << resp2.body;
        Json::Value result2 = Json::Value::FromText(resp2.body);
        EXPECT_TRUE(result2["already_exists"].Get<bool>());
        EXPECT_EQ(result2["person_id"].Get<int64_t>(), createdId);
        // No second welcome email on the reuse path.
        EXPECT_EQ(mail->GetMessages().size(), 1u);
    });
    Auth::ServerConfig::Shutdown();
}

// Admin resets a user's password → the old password stops verifying, the account is
// flagged must_change_password, and a reset email is queued.
TEST(CommunityFinderAdminUsersTest, AdminResetPasswordChangesPasswordAndEmails) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFAdminResetPassword", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        EnsureSessionSecret(transaction, helper);
        AddMailSecrets(transaction, helper);

        TestSession admin = SetupPerson(transaction, testDb, helper,
            "admin@example.com", "Password123!", true);
        SetupPerson(transaction, testDb, helper, "member@example.com", "OldPass123!", false);

        ActAs(helper, admin.token);
        crow::response resp = Handle(helper, crow::HTTPMethod::Post,
            "/api/admin/reset_password", ResetBody("member@example.com"));
        ASSERT_EQ(resp.code, 200) << resp.body;

        // The old password no longer verifies (a new temporary one was set).
        Auth::PersonHelper personHelper(testDb.GetDatabaseHelper());
        EXPECT_FALSE(personHelper.VerifyPassword(
            transaction, "member@example.com", "OldPass123!"));

        // must_change_password is set so the emailed temp password forces a change.
        const std::string mustChange = transaction.RunSqlStatementReturningOneValue(
            "SELECT must_change_password::int FROM people WHERE email = $1",
            std::string("member@example.com"));
        EXPECT_EQ(mustChange, "1");

        // A reset email (the temporary password) was queued.
        auto mail = helper.GetMailHelper();
        ASSERT_TRUE(mail);
        EXPECT_EQ(mail->GetMessages().size(), 1u);
    });
    Auth::ServerConfig::Shutdown();
}

// A logged-in non-admin (no admin_portal) cannot create users — refused with 403 and
// no account is created.
TEST(CommunityFinderAdminUsersTest, NonAdminCannotCreateUser) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFNonAdminNoCreateUser", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        EnsureSessionSecret(transaction, helper);

        TestSession user = SetupPerson(transaction, testDb, helper,
            "member@example.com", "Password123!", false);
        ActAs(helper, user.token);

        crow::response resp = Handle(helper, crow::HTTPMethod::Post,
            "/api/admin/create_user",
            CreateUserBody("Ada", "Lovelace", "ada@example.com"));
        EXPECT_EQ(resp.code, 403) << resp.body;

        // No account was created.
        TableHelpers::People people(testDb.GetDatabaseHelper());
        EXPECT_TRUE(people.LookupPersonByEmail(transaction, "ada@example.com").empty());
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
