#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "endpoints/endpoint_test_helper.h"
#include "endpoints/web_app.h"
#include "business_logic/auth/cookie_manager_test_util.h"
#include "business_logic/auth/person.h"
#include "business_logic/auth/server_config.h"
#include "db_schema/people.h"
#include "db_schema/permissions.h"
#include "db_schema/role_assignments.h"
#include "db_schema/role_permissions.h"
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

// Role & permission MANAGEMENT is the generic CRUD (Phase 6) over the framework
// tables roles / permissions / role_permissions / role_assignments, and PROPAGATION
// flows through POST /api/get_user_info, which RE-RESOLVES the caller's roles +
// permissions from the DB on every call (Session stores only personId) — so a role
// assigned mid-session shows up on the very next call, no re-login. These CF tests
// cover the end-to-end HTTP flow in CF's build: an admin assigns/revokes a role via
// the CRUD endpoints and it appears/disappears in the target's get_user_info, and a
// non-admin cannot reach the role tables at all.
//
// NOTE (corrects an old plan assumption): get_user_info's `permissions` array is the
// DIRECT role→role_permissions→permissions set — it does NOT expand
// permission_implications. The transitive ExpandClosure helper exists but is consumed
// app-side (knottyyoga's pricing tiers), not by the framework's get_user_info /
// RequirePermission. CommunityFinder has no implication tiers, so this is moot here.
//
// NOTE (escalation): there is no fine-grained self-assignment guard anywhere — the
// ENTIRE defense is that roles/role_assignments/role_permissions are admin-only
// (not in any non-admin's allowed-tables set), so a non-admin add_item on them is a
// flat 400 "not an allowed table".
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

// Create a fully-validated person (optionally with the `admin` role) and a session
// token. Does NOT set the cookie — callers switch the active session with ActAs().
TestSession SetupPerson(Transaction& transaction, TestDatabaseUtil& testDb,
                        EndpointTestHelper& helper, std::string_view email, bool admin) {
    Auth::PersonHelper personHelper(testDb.GetDatabaseHelper());
    Auth::PersonInfo info{std::string(email), "Test", "User"};
    personHelper.CreateFullyValidatedUser(transaction, info, "Password123!");

    TableHelpers::People people(testDb.GetDatabaseHelper());
    int64_t personId = std::stoll(
        people.LookupPersonByEmail(transaction, info.email)
            .at(std::string(DbSchema::kPeopleId)));

    if (admin) {
        // Creates the `admin` role — call SetupPerson(admin=true) at most once per
        // test, since the role name is unique (AddRole throws on a duplicate).
        TableHelpers::Roles roles(testDb.GetDatabaseHelper());
        int64_t adminRoleId =
            roles.AddRole(transaction, std::string(DbSchema::kRoleNameAdmin), "Administrator");
        TableHelpers::RoleAssignments roleAssignments(testDb.GetDatabaseHelper());
        roleAssignments.AddRoleAssignment(transaction, personId, adminRoleId);
    }

    std::string token;
    EXPECT_TRUE(personHelper.CreateSessionToken(
        transaction, helper.GetSecretsHelper(), info.email, token));
    return {personId, token};
}

// Switch the active session by stamping the session_token cookie.
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

std::string AddItemBody(std::string_view table, const Json::JsonObject& value) {
    Json::Value body(Json::JsonObject{
        {"table_name", std::string(table)},
        {"value", Json::Value(value)},
    });
    return body.ToString();
}

bool ArrayContains(const Json::Value& array, std::string_view value) {
    if (!array.IsArray()) {
        return false;
    }
    for (const auto& item : array.GetArray()) {
        if (item.Is<std::string>() && item.Get<std::string>() == value) {
            return true;
        }
    }
    return false;
}

// The Phase 8 gate: assign a role via the CRUD endpoints → it propagates to the
// target's session; revoke it → it's gone. All the management is generic CRUD.
TEST(CommunityFinderRolesPermissionsTest, AdminAssignsRoleViaCrudPropagatesAndRevokes) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFRolesAssignPropagateRevoke", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kPermissionsTable);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRolesTable);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRolePermissionsTable);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRoleAssignmentsTable);
        EnsureSessionSecret(transaction, helper);

        TestSession admin = SetupPerson(transaction, testDb, helper, "roles-admin@example.com", true);
        TestSession target = SetupPerson(transaction, testDb, helper, "member@example.com", false);

        ActAs(helper, admin.token);

        // Create a permission + role, grant the permission to the role, assign the
        // role to the target — all via the generic CRUD endpoints.
        crow::response permResp = Handle(helper, crow::HTTPMethod::Post,
            "/api/add_item_fetch_primary_key",
            AddItemBody(DbSchema::kPermissionsTable, {
                {std::string(DbSchema::kPermissionsName), std::string("manage_events")},
                {std::string(DbSchema::kPermissionsDescription), std::string("Manage community events")},
            }));
        ASSERT_EQ(permResp.code, 200) << permResp.body;
        const std::string permId =
            Json::Value::FromText(permResp.body)[DbSchema::kPermissionsId].Get<std::string>();

        crow::response roleResp = Handle(helper, crow::HTTPMethod::Post,
            "/api/add_item_fetch_primary_key",
            AddItemBody(DbSchema::kRolesTable, {
                {std::string(DbSchema::kRolesName), std::string("event_manager")},
                {std::string(DbSchema::kRolesDescription), std::string("Community event managers")},
            }));
        ASSERT_EQ(roleResp.code, 200) << roleResp.body;
        const std::string roleId =
            Json::Value::FromText(roleResp.body)[DbSchema::kRolesId].Get<std::string>();

        crow::response grantResp = Handle(helper, crow::HTTPMethod::Post, "/api/add_item",
            AddItemBody(DbSchema::kRolePermissionsTable, {
                {std::string(DbSchema::kRolePermissionsRoleId), roleId},
                {std::string(DbSchema::kRolePermissionsPermissionId), permId},
            }));
        ASSERT_EQ(grantResp.code, 200) << grantResp.body;

        crow::response assignResp = Handle(helper, crow::HTTPMethod::Post,
            "/api/add_item_fetch_primary_key",
            AddItemBody(DbSchema::kRoleAssignmentsTable, {
                {std::string(DbSchema::kRoleAssignmentsPersonId), std::to_string(target.personId)},
                {std::string(DbSchema::kRoleAssignmentsRoleId), roleId},
            }));
        ASSERT_EQ(assignResp.code, 200) << assignResp.body;
        const std::string assignmentId =
            Json::Value::FromText(assignResp.body)[DbSchema::kRoleAssignmentsId].Get<std::string>();

        // The target's very next get_user_info reflects the new role + permission.
        ActAs(helper, target.token);
        crow::response infoResp = Handle(helper, crow::HTTPMethod::Post, "/api/get_user_info");
        ASSERT_EQ(infoResp.code, 200) << infoResp.body;
        Json::Value info = Json::Value::FromText(infoResp.body);
        EXPECT_TRUE(ArrayContains(info["roles"], "event_manager")) << infoResp.body;
        EXPECT_TRUE(ArrayContains(info["permissions"], "manage_events")) << infoResp.body;

        // Revoke via the CRUD endpoint → gone from the target's session.
        ActAs(helper, admin.token);
        crow::response deleteResp = Handle(helper, crow::HTTPMethod::Get,
            "/api/delete_item/role_assignments/id/" + assignmentId);
        ASSERT_EQ(deleteResp.code, 200) << deleteResp.body;

        ActAs(helper, target.token);
        crow::response infoResp2 = Handle(helper, crow::HTTPMethod::Post, "/api/get_user_info");
        ASSERT_EQ(infoResp2.code, 200) << infoResp2.body;
        Json::Value info2 = Json::Value::FromText(infoResp2.body);
        EXPECT_FALSE(ArrayContains(info2["roles"], "event_manager")) << infoResp2.body;
        EXPECT_FALSE(ArrayContains(info2["permissions"], "manage_events")) << infoResp2.body;
    });
    Auth::ServerConfig::Shutdown();
}

// A logged-in non-admin cannot escalate: role_assignments is admin-only, so trying to
// grant themselves the admin role is refused (400 "not an allowed table") and they do
// not become admin. This coarse admin-only gate is the whole escalation defense.
TEST(CommunityFinderRolesPermissionsTest, NonAdminCannotSelfAssignAdminRole) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CFNonAdminNoSelfAssign", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        // These are admin-only tables — a non-admin's allowed set never includes them.
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRolesTable);
        helper.AddAdminTopLevelTable(transaction, DbSchema::kRoleAssignmentsTable);
        EnsureSessionSecret(transaction, helper);

        TableHelpers::Roles roles(testDb.GetDatabaseHelper());
        int64_t adminRoleId =
            roles.AddRole(transaction, std::string(DbSchema::kRoleNameAdmin), "Administrator");

        TestSession user = SetupPerson(transaction, testDb, helper, "member@example.com", false);
        ActAs(helper, user.token);

        crow::response resp = Handle(helper, crow::HTTPMethod::Post, "/api/add_item",
            AddItemBody(DbSchema::kRoleAssignmentsTable, {
                {std::string(DbSchema::kRoleAssignmentsPersonId), std::to_string(user.personId)},
                {std::string(DbSchema::kRoleAssignmentsRoleId), std::to_string(adminRoleId)},
            }));
        ASSERT_EQ(resp.code, 400) << resp.body;
        EXPECT_EQ(Json::Value::FromText(resp.body)["type"].Get<std::string>(),
                  ErrorCodes::kValidationError);

        // And they are still not an admin.
        crow::response infoResp = Handle(helper, crow::HTTPMethod::Post, "/api/get_user_info");
        ASSERT_EQ(infoResp.code, 200) << infoResp.body;
        EXPECT_FALSE(ArrayContains(Json::Value::FromText(infoResp.body)["roles"], "admin"));
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
