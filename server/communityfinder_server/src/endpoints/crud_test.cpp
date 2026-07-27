#include <string>

#include <gtest/gtest.h>

#include "endpoints/endpoint_test_helper.h"
#include "endpoints/web_app.h"
#include "business_logic/auth/cookie_manager_test_util.h"
#include "business_logic/auth/person.h"
#include "business_logic/auth/server_config.h"
#include "db_schema/config_secrets.h"
#include "db_schema/people.h"
#include "db_schema/roles.h"
#include "sql_util/table_helpers/people.h"
#include "sql_util/table_helpers/role_assignments.h"
#include "sql_util/table_helpers/roles.h"
#include "test/src/util/database_test_helper.h"
#include "util/error_codes.h"
#include "util/json_value.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper.h"
#include "util/secrets/secrets_helper_test_util.h"

// Generic/admin CRUD is honuware framework code (registered via
// RegisterFrameworkEndpoints since Phase 2), and the honuware component suite —
// composed against CommunityFinder's schema in communityfinder_tests — already
// exercises the machinery end to end: PopulateFrameworkTables' CRUD metadata
// (CreateFrameworkTablesTest.PopulateFrameworkTablesSeedsBaseline), add/get/
// update/delete (add_item_test / get_row_test / update_item_test /
// delete_item_test / get_table_rows_test), the password_hash redaction
// (GetRow/GetTableRowsPeopleHidesPasswordHash), config_secrets exclusion
// (GetRowConfigSecrets*Forbidden), and admin-vs-anonymous gating. These CF-side
// tests are the app's own coverage of the loop in its build: a full admin CRUD
// round-trip, the anonymous denial, CF's secrets-exclusion policy, and schema
// discovery. Access to the generic CRUD endpoints is a function of the caller's
// effective allowed-tables set (public `allowed_tables` for everyone, all
// `admin_top_level_tables` for the `admin` role) — there is NO per-endpoint
// permission check, so an out-of-set table returns 400 ValidationError, never
// 401/403.
namespace Endpoints {
namespace {

// Establishes an authenticated admin session on the test cookie manager, mirroring
// honuware's own endpoint-test pattern (platform get_row_test.cpp): create a
// fully-validated person, mint a session token, grant the `admin` role, and stamp
// the session_token cookie so every subsequent handle_full() runs as that logged-in
// admin. Returns the new person's id.
int64_t LoginAsAdmin(Transaction& transaction, TestDatabaseUtil& testDb,
                     EndpointTestHelper& helper, std::string_view email) {
    auto secrets = helper.GetSecretsHelper();
    // Without a session-duration secret the freshly-minted token reads as expired.
    secrets->AddSecret(transaction, Secrets::kAuthSessionMaxDuractioninMicros,
                       std::to_string(10LL * 60LL * 1000000LL));

    Auth::PersonHelper personHelper(testDb.GetDatabaseHelper());
    Auth::PersonInfo info{std::string(email), "Admin", "User"};
    personHelper.CreateFullyValidatedUser(transaction, info, "Password123!");

    std::string sessionToken;
    EXPECT_TRUE(personHelper.CreateSessionToken(
        transaction, secrets, info.email, sessionToken));

    TableHelpers::Roles roles(testDb.GetDatabaseHelper());
    int64_t adminRoleId = roles.AddRole(
        transaction, std::string(DbSchema::kRoleNameAdmin), "Administrator");

    TableHelpers::People people(testDb.GetDatabaseHelper());
    int64_t personId = std::stoll(
        people.LookupPersonByEmail(transaction, info.email)
            .at(std::string(DbSchema::kPeopleId)));

    TableHelpers::RoleAssignments roleAssignments(testDb.GetDatabaseHelper());
    roleAssignments.AddRoleAssignment(transaction, personId, adminRoleId);

    auto cookieManager = helper.GetCookieManagerTest();
    Auth::CookieProperties cookieProps;
    cookieProps.path = "/";
    cookieProps.sameSite = Auth::CookieSameSitePolicy::None;
    cookieManager->SetCookie("session_token", sessionToken, cookieProps);
    return personId;
}

// Drives one in-process request through the framework routes.
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

// Pulls a single cell out of a {sortedColumnNames, dataTable} CRUD read response.
// Returns "" when the row set is empty or the column is absent.
std::string CellValue(const Json::Value& body, std::string_view columnName) {
    const Json::JsonArray& columns = body["sortedColumnNames"].GetArray();
    const Json::JsonArray& rows = body["dataTable"].GetArray();
    if (rows.empty()) {
        return "";
    }
    const Json::JsonArray& firstRow = rows[0].GetArray();
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].Get<std::string>() == columnName) {
            return firstRow[i].Get<std::string>();
        }
    }
    return "";
}

std::string AddRolesBody(std::string_view name, std::string_view description) {
    Json::Value body(Json::JsonObject{
        {"table_name", std::string(DbSchema::kRolesTable)},
        {"value", Json::Value(Json::JsonObject{
                      {std::string(DbSchema::kRolesName), std::string(name)},
                      {std::string(DbSchema::kRolesDescription),
                       std::string(description)},
                  })},
    });
    return body.ToString();
}

// The full generic-CRUD round-trip against a framework table, as a logged-in admin:
// create → read → update → read → delete → read-empty.
TEST(CommunityFinderCrudTest, AdminCanCrudFrameworkTableEndToEnd) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFAdminCrudLoop", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        // Register `roles` as an admin-editable table. In production this row comes
        // from PopulateFrameworkTables (create_database); the test DB carries schema
        // only, so the harness seeds the fixture like honuware's own CRUD tests.
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRolesTable);
        LoginAsAdmin(transaction, testDb, helper, "crud-admin@example.com");

        // CREATE — add_item_fetch_primary_key returns the new row's primary key.
        crow::response addResp = Handle(
            helper, crow::HTTPMethod::Post, "/api/add_item_fetch_primary_key",
            AddRolesBody("content_editors", "Edits site content"));
        ASSERT_EQ(addResp.code, 200) << addResp.body;
        Json::Value addBody = Json::Value::FromText(addResp.body);
        const std::string newId = addBody[DbSchema::kRolesId].Get<std::string>();
        ASSERT_FALSE(newId.empty());

        // READ
        crow::response getResp = Handle(helper, crow::HTTPMethod::Get,
                                        "/api/get_row/roles/id/" + newId);
        ASSERT_EQ(getResp.code, 200) << getResp.body;
        Json::Value getBody = Json::Value::FromText(getResp.body);
        EXPECT_EQ(CellValue(getBody, DbSchema::kRolesName), "content_editors");
        EXPECT_EQ(CellValue(getBody, DbSchema::kRolesDescription),
                  "Edits site content");

        // UPDATE
        Json::Value updateBody(Json::JsonObject{
            {"table_name", std::string(DbSchema::kRolesTable)},
            {"value", Json::Value(Json::JsonObject{
                          {std::string(DbSchema::kRolesDescription),
                           std::string("Edits everything")}})},
            {"column_name", std::string(DbSchema::kRolesId)},
            {"column_value", newId},
        });
        crow::response updateResp = Handle(helper, crow::HTTPMethod::Post,
                                           "/api/update_item", updateBody.ToString());
        ASSERT_EQ(updateResp.code, 200) << updateResp.body;

        crow::response getResp2 = Handle(helper, crow::HTTPMethod::Get,
                                         "/api/get_row/roles/id/" + newId);
        Json::Value getBody2 = Json::Value::FromText(getResp2.body);
        EXPECT_EQ(CellValue(getBody2, DbSchema::kRolesDescription),
                  "Edits everything");

        // DELETE
        crow::response deleteResp = Handle(helper, crow::HTTPMethod::Get,
                                           "/api/delete_item/roles/id/" + newId);
        ASSERT_EQ(deleteResp.code, 200) << deleteResp.body;

        crow::response getResp3 = Handle(helper, crow::HTTPMethod::Get,
                                         "/api/get_row/roles/id/" + newId);
        Json::Value getBody3 = Json::Value::FromText(getResp3.body);
        EXPECT_TRUE(getBody3["dataTable"].GetArray().empty())
            << "row should be gone after delete: " << getResp3.body;
    });
    Auth::ServerConfig::Shutdown();
}

// An anonymous caller cannot reach an admin-only table. Note the generic CRUD
// contract: this is 400 ValidationError ("not an allowed table"), NOT 401/403 —
// the endpoints gate on allowed-tables membership, not an explicit permission.
TEST(CommunityFinderCrudTest, AnonymousGenericCrudDenied) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFAnonCrudDenied", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        // No session cookie -> anonymous. `roles` is admin-only (not in the public
        // allowed_tables), so an anonymous read is refused.
        crow::response resp = Handle(helper, crow::HTTPMethod::Get,
                                     "/api/get_table_rows/roles");
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["type"].Get<std::string>(), ErrorCodes::kValidationError);
    });
    Auth::ServerConfig::Shutdown();
}

// CommunityFinder policy: config_secrets is never exposed through the generic CRUD
// editor, even to an admin (community settings get their own admin surface in a
// later phase). The framework enforces this by omitting config_secrets from every
// allowed/admin table registry.
TEST(CommunityFinderCrudTest, AdminCannotReadConfigSecretsViaCrud) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFAdminConfigSecretsForbidden",
                            [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        // Give the admin a genuine admin table so we know admin CRUD is wired up...
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRolesTable);
        LoginAsAdmin(transaction, testDb, helper, "secrets-admin@example.com");
        // ...yet config_secrets stays out of the generic editor.
        crow::response resp = Handle(helper, crow::HTTPMethod::Get,
                                     "/api/get_row/config_secrets/id/1");
        ASSERT_EQ(resp.code, 400) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_EQ(body["type"].Get<std::string>(), ErrorCodes::kValidationError);
    });
    Auth::ServerConfig::Shutdown();
}

// get_db_schema powers the admin CRUD editor's table list. An admin gets a JSON
// metadata object that surfaces the tables they can edit.
TEST(CommunityFinderCrudTest, AdminGetDbSchemaReturnsShape) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFAdminGetDbSchema", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kPeopleTable);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRolesTable);
        LoginAsAdmin(transaction, testDb, helper, "schema-admin@example.com");

        crow::response resp =
            Handle(helper, crow::HTTPMethod::Get, "/api/get_db_schema");
        ASSERT_EQ(resp.code, 200) << resp.body;
        Json::Value body = Json::Value::FromText(resp.body);
        EXPECT_TRUE(body.HasChildren());
        // The admin's editable tables appear somewhere in the metadata blob.
        EXPECT_NE(resp.body.find(std::string(DbSchema::kPeopleTable)),
                  std::string::npos);
        EXPECT_NE(resp.body.find(std::string(DbSchema::kRolesTable)),
                  std::string::npos);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
