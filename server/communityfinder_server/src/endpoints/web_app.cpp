// The app endpoint registration table. It anchors the honuware framework endpoints
// (via RegisterFrameworkEndpoints) and — as they are added — the app's own
// endpoints, so a static-library object is never dead-stripped and its
// self-registering RoutingBase runs. This TU is itself anchored into the final link
// by Endpoints::RegisterAllEndpoints(), which every WebApp host (main.cpp, the test
// main) calls before RoutingBase::AddRoutes().
#include "endpoint_registrations.h"

// honuware's framework endpoints (login, register, verify, account, photo, generic
// CRUD, health, site_info, ...). Its RegisterFrameworkEndpoints() anchors them.
#include "endpoints/register_framework_endpoints.h"

// Include each APP endpoint header here as it is added (Phase 4 account surface,
// Phase 10 events), and add its `anchor = ...` line in RegisterAllEndpoints below.
#include "admin_create_user.h"
#include "admin_ingest_events.h"
#include "admin_reset_password.h"
#include "admin_review_event.h"
#include "get_event_detail.h"
#include "get_event_ics.h"
#include "get_event_series.h"
#include "get_events_ics_feed.h"
#include "get_upcoming_events.h"

// Stream B — the guide (GD2 §4).
#include "get_guide_graveyard.h"
#include "get_guide_neighborhoods.h"
#include "get_guide_organizations.h"
#include "get_guide_pages.h"
#include "get_guide_places.h"
#include "get_guide_scenes.h"
#include "get_guide_services.h"
#include "get_guide_venues.h"

namespace Endpoints {

// Anchor every endpoint's translation unit into the final link.
//
// Each endpoint .cpp registers its route through a file-scope, self-registering
// RoutingBase object living in a STATIC library (communityfinder_core). A static
// archive only yields an object when the linker needs it to resolve an undefined
// symbol, so without a genuine reference the object is never extracted, the route
// is never registered, and the endpoint silently 404s at runtime.
//
// THE VOLATILE STORE IS LOAD-BEARING. Taking each address into an unused
// anonymous-namespace variable works at -O0 but NOT at -O2 — the optimiser deletes
// the unused internal-linkage variable, the relocation disappears, the linker never
// extracts the object, and EVERY app route 404s in a RELEASE build. A store through
// a volatile object is an observable side effect the compiler may not elide, so each
// address-of survives optimisation as a real relocation. (Casting between function
// pointer types is well-defined; these are only ever stored, never called through.)
//
// To add an app endpoint: add its include above and one `anchor = ...` line below.
void RegisterAllEndpoints() {
    // Framework endpoints are anchored by honuware.
    RegisterFrameworkEndpoints();

    // App endpoints — anchor each TU so its self-registering RoutingBase runs
    // (see the header comment: a static-archive object is dropped without a real
    // reference, and the route silently 404s — worse at -O2).
    using AnchorFunc = void (*)();
    static AnchorFunc volatile anchor = nullptr;
    (void)anchor;
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::PostAdminCreateUser);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::PostAdminIngestEvents);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::PostAdminResetPassword);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::PostAdminReviewEvent);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetEventDetail);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetEventICal);
    // One anchor pulls in the whole TU, so both series routes register.
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetEventSeriesList);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetEventsICalFeed);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetUpcomingEvents);

    // Stream B — the guide. One anchor per TU; a TU's other route rides along.
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuideGraveyard);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuideNeighborhoods);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuideOrganizations);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuidePage);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuidePlaces);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuideScenes);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuideServices);
    anchor = reinterpret_cast<AnchorFunc>(&Endpoints::GetGuideVenues);
}

}  // namespace Endpoints
