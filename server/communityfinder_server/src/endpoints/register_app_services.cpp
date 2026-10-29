#include "register_app_services.h"

#include <memory>
#include <string>

#include "db_schema/events.h"
#include "db_schema/guide_pages.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/venues.h"
#include "endpoints/public_photo_tables.h"
#include "endpoints/web_app.h"

namespace Endpoints {

void RegisterAppServices(WebApp& webApp) {
    // Event and venue images are the public site's imagery, and so are the
    // guide's organization, place and page images (GD2 §4.10). Uploading them
    // still takes an admin (the framework's write checks are untouched); this
    // only opens the scaled read.
    webApp.SetService<PublicPhotoTables>(std::make_shared<PublicPhotoTables>(PublicPhotoTables{
        { std::string(DbSchema::kEventsTable), std::string(DbSchema::kVenuesTable),
          std::string(DbSchema::kOrganizationsTable), std::string(DbSchema::kPlacesTable),
          std::string(DbSchema::kGuidePagesTable) } }));
}

}  // namespace Endpoints
