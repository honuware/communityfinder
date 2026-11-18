#include "register_app_services.h"

#include <memory>
#include <string>

#include "business_logic/events/photo_visibility.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/guide_pages.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/series.h"
#include "db_schema/venues.h"
#include "endpoints/public_photo_tables.h"
#include "endpoints/web_app.h"

namespace Endpoints {

void RegisterAppServices(WebApp& webApp) {
    // Event and venue images are the public site's imagery, and so are the
    // guide's organization, place and page images (GD2 §4.10), and a series'
    // photo and the event and series galleries (CM3). Uploading them still
    // takes write access to the table (the framework's checks are untouched);
    // this only opens the scaled read.
    //
    // CM3 §0.1 (H15): the per-row rule keeps an event's photos, and its
    // gallery's, private until the event is public (Events::IsPhotoRowPublic).
    webApp.SetService<PublicPhotoTables>(std::make_shared<PublicPhotoTables>(PublicPhotoTables{
        { std::string(DbSchema::kEventsTable), std::string(DbSchema::kVenuesTable),
          std::string(DbSchema::kOrganizationsTable), std::string(DbSchema::kPlacesTable),
          std::string(DbSchema::kGuidePagesTable), std::string(DbSchema::kSeriesTable),
          std::string(DbSchema::kEventGalleryPhotosTable) },
        &Events::IsPhotoRowPublic }));
}

}  // namespace Endpoints
