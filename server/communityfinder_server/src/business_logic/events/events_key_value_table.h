#pragma once

#include <vector>

#include "business_logic/events/event_helper.h"
#include "sql_util/database_common.h"

namespace Events {

// EV1 Slice 4.2 — domain → KeyValueTable, the layer boundary the endpoints
// convert to JSON at the edge (SqlUtil::KeyValueTableToJson).
//
// Conventions the network layer relies on (knottyyoga's):
//   - an absent optional (ends_at_us, venue_id, series_id) is the EMPTY STRING,
//     not "0", so the client maps it to null;
//   - a slug list is comma-joined ("dance,drag") — a KeyValueTable is a flat
//     string map. The endpoint may nest it as a real JSON array (get_calendar
//     does that for tags).
//
// Built from PublicEvent, so the review quartet (status, reviewed_by, reviewed_us,
// review_notes) and created_by can't reach a public response: the struct never
// carries them.
KeyValueTable PublicEventToKeyValueTable(const PublicEvent& event);
KeyValueTableArray PublicEventsToKeyValueTableArray(const std::vector<PublicEvent>& events);

// A series for the index and its page: no volatility tier or timestamps (those
// are the verifier's bookkeeping), but `status` and `last_verified_at_us` — the
// freshness a visitor is shown.
KeyValueTable PublicSeriesToKeyValueTable(const PublicSeries& series);
KeyValueTableArray PublicSeriesListToKeyValueTableArray(const std::vector<PublicSeries>& seriesList);

}  // namespace Events
