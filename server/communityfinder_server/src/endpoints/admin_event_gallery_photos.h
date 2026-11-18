#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// CM3 §4.2 — adds a gallery row for a curator (`manage_events`). The one route
// generic CRUD can't stand in for: it records the curator as the consent's
// confirmer and enforces the site's gallery cap (the admin events settings
// page). The row's photo is uploaded next through honuware's
// /api/upload_photo/event_gallery_photos/<id>/<type>; edits, reordering and
// deletes stay with generic CRUD.

// POST /api/admin/event_gallery_photos. Body: {"event_id" | "series_id",
// "alt_text", "caption"?, "credit"?, "consent_confirmed": true}. A wrong type
// is a 400 naming the `field`; the rest are 400s with `error_code` and `field`
// (Events::EventGalleryHelper::AddGalleryPhoto), and GALLERY_FULL also carries
// `max_photos`. Responds {"id"}.
Json::Value PostAdminEventGalleryPhoto(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
