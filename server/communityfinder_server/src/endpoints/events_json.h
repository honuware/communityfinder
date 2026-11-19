#pragma once

#include <optional>
#include <vector>

#include "business_logic/events/event_helper.h"
#include "util/json_value.h"

namespace Endpoints {

// EV1 §5 — the events domain's JSON, shared by the public event and series
// endpoints. Built from the business layer's KeyValueTables
// (Events::*ToKeyValueTable, via SqlUtil::KeyValueTableToJson) with two fixes a
// flat string map can't carry:
//   - slug lists go out as real JSON arrays, not "dance,drag" (EV1 OQ10 (f));
//   - free-text fields stay strings. KeyValueTableToJson turns every all-digit
//     string into a number, so without this a cost of "10", or a title of
//     "1999", would reach the client as a number.
// Ids and times come out as numbers; a missing end, venue, series or
// verification time is the empty string, which the client maps to null.
//
// CM3 adds `photo` ({"table", "id"}) and `video` ({"provider", "id"}) to events
// and series, each a real null when there is none.
Json::Value PublicEventToJson(const Events::PublicEvent& event);
Json::Value PublicEventsToJson(const std::vector<Events::PublicEvent>& events);
Json::Value PublicSeriesToJson(const Events::PublicSeries& series);
Json::Value PublicSeriesListToJson(const std::vector<Events::PublicSeries>& seriesList);

// CM3: a gallery as [{"id", "alt_text", "caption", "credit"}], in order. The
// texts stay strings whatever they hold.
Json::Value GalleryToJson(const std::vector<Events::GalleryPhoto>& gallery);

// CM3: a video as {"provider", "id"}, or a real null. The same shape on every
// payload that carries one (events, series, "My submissions").
Json::Value VideoToJson(const std::optional<Events::VideoLink>& video);

}  // namespace Endpoints
