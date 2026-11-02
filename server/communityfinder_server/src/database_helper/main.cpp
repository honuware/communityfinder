#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <pqxx/pqxx>

#include "business_logic/app_database_config.h"
#include "business_logic/migration/all_migrations.h"
#include "business_logic/migration/migrate_command.h"
#include "business_logic/populate_sample_data.h"
#include "create_database.h"
#include "db_schema/role_assignments.h"
#include "db_schema/roles.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/database_access/production_transaction_provider.h"
#include "sql_util/database_access/transaction_provider.h"
#include "util/destructive_guard.h"
#include "util/logging.h"

ABSL_FLAG(bool, recreate_database, false,
    "Drop and recreate the entire database from scratch (destructive). "
    "Requires HONUWARE_ALLOW_DESTRUCTIVE=1 in the environment.");

ABSL_FLAG(bool, migrate, false,
    "Apply pending schema migrations to the existing database. Non-destructive. "
    "Suitable for production deploys.");

ABSL_FLAG(bool, populate_sample_data, false,
    "Add made-up sample data for trying the site out: a small guide and four "
    "weeks of events from today. Safe to re-run (a later run moves the events "
    "along). Runs after --recreate_database when both are given. Requires "
    "HONUWARE_ALLOW_DESTRUCTIVE=1, so it can never reach production.");

namespace {

int RunRecreate() {
    try {
        EnsureDestructiveAllowed();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    LogInfo() << "[database_helper] event=recreate_database_starting\n";
    try {
        CreateAndPopulateDatabases();
    } catch (const pqxx::sql_error& e) {
        std::cerr << "recreate_database SQL error: " << e.what()
                  << "\n  QUERY: " << e.query() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "recreate_database failed: " << e.what() << "\n";
        return 1;
    }
    LogInfo() << "[database_helper] event=recreate_database_done\n";
    return 0;
}

int RunMigrate() {
    LogInfo() << "[database_helper] event=migrate_starting\n";
    DatabaseHelper databaseHelper = MakeProductionDatabaseHelper(App::kDatabaseName);
    auto transactionProvider = MakeProductionTransactionProvider(databaseHelper);
    int rc = Migration::RunMigrateCommand(
        *transactionProvider,
        databaseHelper,
        Migration::BuildAllMigrations());
    LogInfo() << "[database_helper] event=migrate_done exit_code=" << rc << "\n";
    return rc;
}

// Who the sample events say approved them: the first administrator (Mason, on
// a freshly recreated database), or 0 when there's none. A plain id, as
// EventHelper::ReviewEvent stores it.
int64_t FirstAdministratorPersonId(Transaction& transaction) {
    std::string personId = transaction.RunSqlStatementReturningOneValue(
        std::string("SELECT COALESCE(MIN(ra.") + std::string(DbSchema::kRoleAssignmentsPersonId)
            + "), 0) FROM " + std::string(DbSchema::kRoleAssignmentsTable) + " ra JOIN "
            + std::string(DbSchema::kRolesTable) + " r ON r." + std::string(DbSchema::kRolesId)
            + " = ra." + std::string(DbSchema::kRoleAssignmentsRoleId) + " WHERE r."
            + std::string(DbSchema::kRolesName) + " = $1",
        std::string(DbSchema::kRoleNameAdmin));
    return std::stoll(personId);
}

int RunPopulateSampleData() {
    try {
        EnsureDestructiveAllowed();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    LogInfo() << "[database_helper] event=populate_sample_data_starting\n";
    SampleDataReport report;
    try {
        DatabaseHelper databaseHelper = MakeProductionDatabaseHelper(App::kDatabaseName);
        databaseHelper.RunInTransaction("PopulateSampleData", [&](Transaction& transaction) {
            report = PopulateSampleData(
                transaction, databaseHelper, FirstAdministratorPersonId(transaction));
            });
    } catch (const pqxx::sql_error& e) {
        std::cerr << "populate_sample_data SQL error: " << e.what()
                  << "\n  QUERY: " << e.query() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "populate_sample_data failed: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Sample data: " << report.listingsCreated << " listings created, "
              << report.listingsAlreadyThere << " already there.\n"
              << "Sample events: " << report.eventsCreated << " created, "
              << report.eventsUpdated << " moved or updated, " << report.eventsUnchanged
              << " unchanged; " << report.eventsApproved << " approved.\n";
    if (!report.missingVocabulary.empty()) {
        std::cout << "Not in this database, so left off:";
        for (const std::string& item : report.missingVocabulary) std::cout << " " << item << ";";
        std::cout << "\n";
    }
    LogInfo() << "[database_helper] event=populate_sample_data_done\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    InitializeLogging();
    absl::ParseCommandLine(argc, argv);

    bool recreate = absl::GetFlag(FLAGS_recreate_database);
    bool migrate = absl::GetFlag(FLAGS_migrate);
    bool populateSampleData = absl::GetFlag(FLAGS_populate_sample_data);

    if ((recreate && migrate) || (!recreate && !migrate && !populateSampleData)) {
        std::cerr <<
            "Specify one of --recreate_database or --migrate, and/or --populate_sample_data.\n"
            "  --recreate_database     Drop + recreate the primary database from scratch.\n"
            "                          Requires HONUWARE_ALLOW_DESTRUCTIVE=1.\n"
            "  --migrate               Apply pending schema migrations (non-destructive).\n"
            "  --populate_sample_data  Add made-up sample data: a small guide and four weeks\n"
            "                          of events from today. Safe to re-run. After the other\n"
            "                          when combined. Requires HONUWARE_ALLOW_DESTRUCTIVE=1.\n"
            "  (--create_tenant is wired in Phase 14 — multi-community control plane.)\n";
        return 1;
    }

    int rc = 0;
    if (recreate) {
        rc = RunRecreate();
    } else if (migrate) {
        rc = RunMigrate();
    }
    if (rc == 0 && populateSampleData) {
        rc = RunPopulateSampleData();
    }
    return rc;
}
