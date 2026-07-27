#include "business_logic/migration/app_migrations.h"

#include "business_logic/migration/migration_namespace.h"

namespace Migration {

std::vector<Migration> BuildAppMigrations() {
    // Intentionally empty pre-deploy. App (CommunityFinder) schema changes that
    // must run against an already-provisioned database are appended here, each id
    // built with NamespacedMigrationId(kAppMigrationNamespace, "NNNN_name") so
    // it stays in the app id namespace.
    return {};
}

}  // namespace Migration
