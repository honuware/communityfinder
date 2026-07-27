#include "business_logic/migration/all_migrations.h"

#include <iterator>
#include <vector>

#include "business_logic/migration/app_migrations.h"
#include "business_logic/migration/framework_migrations.h"

namespace Migration {

std::vector<Migration> BuildAllMigrations() {
    // Composition root (app-side): the honuware framework migration stream runs
    // first, then the app stream. Both streams are namespaced (honuware/… and
    // app/…, see migration_namespace.h), so they occupy disjoint
    // schema_migrations id spaces and evolve independently — the tenant plan
    // relies on this to migrate every tenant DB in a loop without honuware
    // upgrades and app changes colliding. Both are empty pre-deploy.
    std::vector<Migration> all = BuildFrameworkMigrations();
    std::vector<Migration> app = BuildAppMigrations();
    all.insert(all.end(),
        std::make_move_iterator(app.begin()),
        std::make_move_iterator(app.end()));
    return all;
}

}  // namespace Migration
