#include "create_database.h"

#include <string>
#include <string_view>
#include <utility>

#include "business_logic/app_database_config.h"
#include "business_logic/app_secret_values.h"
#include "business_logic/auth/person.h"
#include "business_logic/create_framework_tables.h"
#include "db_schema/config_secrets.h"
#include "db_schema/make_database_info.h"
#include "db_schema/role_assignments.h"
#include "db_schema/roles.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_access/database_helper_init.h"
#include "sql_util/database_access/db_and_table_operations.h"
#include "sql_util/database_access/extensions.h"
#include "sql_util/database_common.h"
#include "sql_util/schema/database_info.h"
#include "sql_util/stored_procedures/create_stored_procedures.h"
#include "util/env.h"
#include "util/logging.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secrets_helper.h"

namespace {

	// The Gmail app password for the CommunityFinder sender account. Read from the
	// environment (the local, gitignored VS launch profile / shell) so the live
	// credential never lands in this PUBLIC repo. See PopulateConfigSecrets.
	constexpr char kEnvMailAppPassword[] = "HONUWARE_MAIL_APP_PASSWORD";

	DatabaseHelper CreateDatabaseHelper(DbSchema::DatabaseInfo databaseInfo) {
		return MakeProductionDatabaseHelper(databaseInfo.GetDatabaseName());
	}

	/*
	When adding a table, please be sure to modify all three:
		create_database.cpp    - CreateTables
		create_database.cpp    - PopulateTables
		make_database_info.cpp - MakeDatabaseInfo
	*/
	void CreateTables(
		Transaction& transaction, DbSchema::DatabaseInfo databaseInfo) {
		// honuware provides the framework tables (schema_migrations, the admin
		// metadata tables, allowed_tables, config_secrets, identity/auth, photos,
		// idempotency_keys, login_attempts, auth_events) + their indexes. They are
		// created first because the app tables will FK into them; no framework
		// table references an app table.
		CreateFrameworkTables(transaction, databaseInfo);
		// CommunityFinder has NO app tables yet — the app schema (its own
		// roles/permissions beyond the framework baseline, and the domain tables)
		// arrives in Phase 10. Until then the framework tables are the entire
		// schema, so there is nothing else to create here.
	}

	std::pair<std::string, std::string> DbPair(std::string_view key, std::string_view value) {
		return std::make_pair<std::string, std::string>(std::string(key), std::string(value));
	}

	inline auto MakeAddRowLambda(
		Transaction& transaction, DatabaseHelper databaseHelper, std::string_view tableName) {
		// Capture databaseHelper and tableName BY VALUE, not by reference: this
		// lambda OUTLIVES the MakeAddRowLambda call (callers invoke the returned
		// lambda after this function has returned), so a [&] capture would leave
		// dangling references to the destroyed-on-return `databaseHelper` and
		// `tableName` parameters. Reading the dangling std::string_view yielded an
		// empty table name on Linux (where the stack slot gets clobbered; it
		// happened to survive on MSVC), which made DbCrud::AddRowToTable ->
		// DbMeta::ListColumns run `('' || '')::regclass` and fail with the Postgres
		// error "invalid name syntax". transaction is a reference to the caller's
		// long-lived Transaction, so capturing it by reference is correct (and
		// required — Transaction is non-copyable).
		return [&transaction, databaseHelper, tableName](const KeyValueTable& keyValueTable) {
			DbCrud::AddRowToTable(transaction, databaseHelper, tableName, keyValueTable);
		};
	}

	// honuware's PopulateFrameworkTables seeds the framework roles (Administrator,
	// User) ahead of anything the app adds, so their ids are framework-assigned.
	// The app therefore references roles by NAME rather than by a hard-coded id.
	std::string RoleIdByName(Transaction& transaction, std::string_view roleName) {
		return transaction.RunSqlStatementReturningOneValue(
			std::string("SELECT ") + std::string(DbSchema::kRolesId) + " FROM " +
				std::string(DbSchema::kRolesTable) + " WHERE " +
				std::string(DbSchema::kRolesName) + " = $1",
			std::string(roleName));
	}

	void PopulatePeople(
		Transaction& transaction, DatabaseHelper databaseHelper, DbSchema::DatabaseInfo databaseInfo) {
		Auth::PersonHelper personHelper(databaseHelper);
		// Pass the production SecretsHelper so the seeded users get an Argon2id
		// hash with production cost. config_secrets isn't populated until later in
		// the bootstrap, but HashPasswordWithSecrets falls back to MODERATE on
		// empty/missing values — which is exactly what we want for seed users.
		Secrets::SecretsHelperPtr secrets =
			Secrets::MakeSecretsHelper(databaseHelper);

		// =====================================================================
		// DEV SEED ONLY — these are local-dev credentials; production admins
		// should reset via the activation/forgot-password flow (revisit in
		// Phase 15 deploy).
		// =====================================================================
		// Created in this exact order so their person ids are 1, 2, 3 — the ids
		// PopulateRoleAssignments below relies on.
		personHelper.CreateFullyValidatedUser(transaction,
			Auth::PersonInfo{"masonbendixen@gmail.com", "Mason", "Bendixen"}, "changeme", secrets);
		personHelper.CreateFullyValidatedUser(transaction,
			Auth::PersonInfo{"ljkuhn33@gmail.com", "Levi", "Kuhn"}, "changeme", secrets);
		personHelper.CreateFullyValidatedUser(transaction,
			Auth::PersonInfo{"mr.calebault@gmail.com", "Caleb", "Ault"}, "changeme", secrets);
	}

	void PopulateRoleAssignments(
		Transaction& transaction, DatabaseHelper databaseHelper, DbSchema::DatabaseInfo databaseInfo) {
		auto AddRow = [&](
			int personId,
			std::string_view roleName) {
				MakeAddRowLambda(
					transaction, databaseHelper, DbSchema::kRoleAssignmentsTable)(
						{
							DbPair(DbSchema::kRoleAssignmentsPersonId, std::to_string(personId)),
							DbPair(DbSchema::kRoleAssignmentsRoleId,
								RoleIdByName(transaction, roleName)) });
			};
		// Person ids are app data (PopulatePeople creates them 1,2,3 in order);
		// the framework Administrator role is looked up by NAME (its id is
		// framework-assigned by PopulateFrameworkTables).
		const int kMasonId = 1;
		const int kLeviId = 2;
		const int kCalebId = 3;
		AddRow(kMasonId, DbSchema::kRoleNameAdmin);  // Mason  -> admin
		AddRow(kLeviId, DbSchema::kRoleNameAdmin);   // Levi   -> admin
		AddRow(kCalebId, DbSchema::kRoleNameAdmin);  // Caleb  -> admin
	}

	void PopulateConfigSecrets(
		Transaction& transaction, DatabaseHelper databaseHelper, DbSchema::DatabaseInfo databaseInfo) {
		auto AddRow = [&](
			std::string_view name,
			std::string_view value) {
				MakeAddRowLambda(
					transaction, databaseHelper, DbSchema::kConfigSecretsTable)(
						{
							DbPair(DbSchema::kConfigSecretsName, name),
							DbPair(DbSchema::kConfigSecretsValue, value) });
			};

		// framework config-secret defaults come from honuware's
		// PopulateFrameworkTables; only app-owned + brand defaults live here.
		App::FillInAppSecretDefaults([&](std::string_view name, std::string_view value) {
			AddRow(name, value);
			});

		// The mail app password comes from the environment, NEVER source control.
		// honuware's PopulateFrameworkTables seeds a SHARED default app password
		// that is committed to the public server_components repo — CommunityFinder
		// must not inherit it. Overwrite the framework-seeded row (this is why we
		// UPDATE rather than AddRow — the name already exists, and adding it app-side
		// would also break the framework/app no-overlap invariant) with the value
		// from HONUWARE_MAIL_APP_PASSWORD, or empty when it is unset so an
		// unconfigured build simply cannot send mail rather than authenticating as
		// the shared account. mailio logs into Gmail using the SENDER ADDRESS as the
		// SMTP username, so this password must belong to kMailSenderAddress
		// (App::app_secret_values) — a mismatch yields "Mail sender rejection".
		const char* mailAppPasswordEnv =
			Util::GetEnvWithFallback(kEnvMailAppPassword, kEnvMailAppPassword);
		const std::string mailAppPassword =
			mailAppPasswordEnv ? mailAppPasswordEnv : "";
		transaction.RunSqlStatement(
			std::string("UPDATE ") + std::string(DbSchema::kConfigSecretsTable) +
				" SET " + std::string(DbSchema::kConfigSecretsValue) + " = $1 WHERE " +
				std::string(DbSchema::kConfigSecretsName) + " = $2",
			mailAppPassword,
			std::string(Secrets::kMailAppPassword));

		// DIAGNOSTIC (Phase 5 mail bring-up): confirm the env password actually
		// reached this process and that the sender address seeded, WITHOUT ever
		// logging the password value (length only). Read the sender back so this
		// reflects the row as stored, not just the source constant.
		const std::string seededSender = transaction.RunSqlStatementReturningOneValue(
			std::string("SELECT ") + std::string(DbSchema::kConfigSecretsValue) +
				" FROM " + std::string(DbSchema::kConfigSecretsTable) + " WHERE " +
				std::string(DbSchema::kConfigSecretsName) + " = $1",
			std::string(Secrets::kMailSenderAddress));
		LogInfo() << "[mail-seed] env " << kEnvMailAppPassword << "="
			<< (mailAppPasswordEnv ? "present" : "MISSING")
			<< " appPasswordLength=" << mailAppPassword.size()
			<< " seededSender=" << seededSender << "\n";
	}

	void PopulateTables(
		Transaction& transaction, DatabaseHelper databaseHelper, DbSchema::DatabaseInfo databaseInfo) {
		// honuware seeds the framework baseline FIRST (framework roles/permissions
		// + their grants, config-secret defaults, the permission_implications
		// allow-list, people photo support, the security redactions, and the
		// framework tables' admin metadata). Order matters: the Administrator role
		// assignment below requires the framework roles to already exist.
		PopulateFrameworkTables(transaction, databaseHelper, databaseInfo);
		PopulatePeople(transaction, databaseHelper, databaseInfo);
		PopulateRoleAssignments(transaction, databaseHelper, databaseInfo);
		PopulateConfigSecrets(transaction, databaseHelper, databaseInfo);

		// DEFERRED to Phase 12 (scheduler): the scheduler service account is
		// provisioned by Auth::EnsureSchedulerServiceAccount, which currently hard-
		// requires the `manage_subscriptions` permission (a knottyyoga coupling) and
		// throws if it is absent. CommunityFinder has no app permissions yet (they
		// arrive with the events domain / roles in Phase 10), and
		// communityfinder_helper (the scheduler exe) is Phase 12 — so the account is
		// seeded then, once CF's permission model exists and honuware's
		// EnsureSchedulerServiceAccount is parameterized on the required
		// permission(s). Seeding it here would make --recreate_database throw.
	}

}  // namespace

/*
When adding a table, please be sure to modify all three:
	create_database.cpp    - CreateTables
	create_database.cpp    - PopulateTables
	make_database_info.cpp - MakeDatabaseInfo
*/
void CreateSchemaAndPopulate(
	DatabaseHelper databaseHelper, DbSchema::DatabaseInfo databaseInfo) {
	databaseHelper.RunInTransaction("CreateSchemaAndPopulate", [&](Transaction& transaction) {
		// Extensions must load before any table that references one of their
		// types (e.g. people.email is CITEXT).
		Extensions::CreateExtensions(transaction);
		StoredProcedures::CreateStoredProceduresBeforeTables(transaction);
		CreateTables(transaction, databaseInfo);
		StoredProcedures::CreateStoredProceduresAfterTables(transaction);
		PopulateTables(transaction, databaseHelper, databaseInfo);
		});
}

void CreateAndPopulateDatabases() {
	DbSchema::DatabaseInfo databaseInfo = DbSchema::MakeDatabaseInfo(App::kDatabaseName);
	DatabaseHelper noDatabaseHelper = MakeNoDatabaseHelper();
	noDatabaseHelper.RunInTransaction(
		"RemoveAndCreateDatabaseBasic",
		[&](Transaction& transaction) {
		DbUtil::RemoveAndCreateDatabase(transaction, databaseInfo.GetDatabaseName());
		});
	DatabaseHelper databaseHelper = CreateDatabaseHelper(databaseInfo);
	CreateSchemaAndPopulate(databaseHelper, databaseInfo);
}
