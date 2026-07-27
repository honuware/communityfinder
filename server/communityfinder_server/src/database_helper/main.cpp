#include <cstdlib>
#include <iostream>
#include <string>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <pqxx/pqxx>

#include "business_logic/app_database_config.h"
#include "business_logic/migration/all_migrations.h"
#include "business_logic/migration/migrate_command.h"
#include "create_database.h"
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

}  // namespace

int main(int argc, char** argv)
{
    InitializeLogging();
    absl::ParseCommandLine(argc, argv);

    bool recreate = absl::GetFlag(FLAGS_recreate_database);
    bool migrate = absl::GetFlag(FLAGS_migrate);

    int modeCount = (recreate ? 1 : 0) + (migrate ? 1 : 0);
    if (modeCount != 1) {
        std::cerr <<
            "Specify exactly one of --recreate_database or --migrate.\n"
            "  --recreate_database  Drop + recreate the primary database from scratch.\n"
            "                       Requires HONUWARE_ALLOW_DESTRUCTIVE=1.\n"
            "  --migrate            Apply pending schema migrations (non-destructive).\n"
            "  (--create_tenant is wired in Phase 14 — multi-community control plane.)\n";
        return 1;
    }

    if (recreate) {
        return RunRecreate();
    }
    return RunMigrate();
}
