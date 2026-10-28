#pragma once

#include <crow.h>

#include "util/json_value.h"

class EndpointAuthHelper;

namespace Endpoints {

// POST /api/admin/ingest_events (EV1 §5.4) — the single door for the scanner
// (EV5) and every feed importer. Gated on `manage_events`. Body:
//   {"items": [{source_id, external_key, title, starts_at_us, ends_at_us?,
//               description?, url?, cost_text?, location_text?, venue_id?,
//               series_slug?, category_slugs?: [], scene_tag_slugs?: []}]}
// A field left out (or null) means "not stated" (EV1 OQ10 (a)). The batch is
// all-or-nothing: a wrongly typed field or a failed check is a 400 validation
// error naming the item (`item_index`, and the business `error_code` where
// there is one), and nothing is written. Success responds
//   {"items": [{event_id, outcome, ignored_category_slugs, ignored_scene_tag_slugs,
//               ignored_series_slug}], "counts": {<outcome>: n, …}}.
Json::Value PostAdminIngestEvents(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp);

}  // namespace Endpoints
