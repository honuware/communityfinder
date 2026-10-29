#pragma once

#include <optional>
#include <vector>

#include "business_logic/guide/guide_helper.h"
#include "util/json_value.h"

namespace Endpoints {

// GD2 §4 — the guide's JSON, shared by the /api/guide/* endpoints. Built from
// the business layer's KeyValueTables (Guide::*ToKeyValueTable, via
// SqlUtil::KeyValueTableToJson) with the fixes a flat string map can't carry,
// as events_json does for EV1:
//   - slug lists go out as real JSON arrays, not "bears,leather";
//   - checkboxes (adult, is_health, draft, published) go out as real JSON
//     booleans, not "true" / "false";
//   - free-text fields stay strings. KeyValueTableToJson turns every all-digit
//     string into a number, so without this a phone number, or a name of
//     "1999", would reach the client as a number.
// Ids, times and counts come out as numbers; a never-verified date or an
// unscheduled page is the empty string, which the client maps to null.
Json::Value GuideVenueToJson(const Guide::GuideVenue& venue);
Json::Value GuideVenuesToJson(const std::vector<Guide::GuideVenue>& venues);

Json::Value GuideOrganizationToJson(const Guide::GuideOrganization& organization);
Json::Value GuideOrganizationsToJson(const std::vector<Guide::GuideOrganization>& organizations);

Json::Value GuidePlaceToJson(const Guide::GuidePlace& place);
Json::Value GuidePlacesToJson(const std::vector<Guide::GuidePlace>& places);

Json::Value GuideServiceToJson(const Guide::GuideService& service);
Json::Value GuideServicesToJson(const std::vector<Guide::GuideService>& services);

// The whole page, body included. The optional form is null when absent (a
// scene without an intro, a neighborhood without a rundown).
Json::Value GuidePageToJson(const Guide::GuidePage& page);
Json::Value OptionalGuidePageToJson(const std::optional<Guide::GuidePage>& page);

Json::Value GuidePageSummariesToJson(const std::vector<Guide::GuidePageSummary>& pages);

Json::Value GuideLinksToJson(const std::vector<Guide::GuideLink>& links);

Json::Value SceneSummaryToJson(const Guide::SceneSummary& scene);
Json::Value SceneSummariesToJson(const std::vector<Guide::SceneSummary>& scenes);

Json::Value NeighborhoodSummaryToJson(const Guide::NeighborhoodSummary& neighborhood);
Json::Value NeighborhoodSummariesToJson(
    const std::vector<Guide::NeighborhoodSummary>& neighborhoods);

}  // namespace Endpoints
