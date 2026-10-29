#pragma once

#include <vector>

#include "business_logic/guide/guide_helper.h"
#include "sql_util/database_common.h"

namespace Guide {

// GD2 §3.2 — domain → KeyValueTable, the layer boundary the endpoints convert
// to JSON at the edge. EV1's conventions (events_key_value_table.h):
//   - an absent optional (a never-verified date, an unscheduled page) is the
//     EMPTY STRING, not "0", so the client maps it to null;
//   - a slug list is comma-joined ("bears,leather");
//   - snake_case keys.
// And for the guide's booleans (adult, is_health, draft, published): "true" /
// "false".
//
// Every listing carries `status` and `last_verified_at_us` (Interface Contract
// 3), and every listing that can be 18+ carries `adult`. A detail page's parts
// (the item, its links, a scene's lists) convert separately; the endpoint nests
// them.

KeyValueTable GuideVenueToKeyValueTable(const GuideVenue& venue);
KeyValueTableArray GuideVenuesToKeyValueTableArray(const std::vector<GuideVenue>& venues);

KeyValueTable GuideOrganizationToKeyValueTable(const GuideOrganization& organization);
KeyValueTableArray GuideOrganizationsToKeyValueTableArray(
    const std::vector<GuideOrganization>& organizations);

KeyValueTable GuidePlaceToKeyValueTable(const GuidePlace& place);
KeyValueTableArray GuidePlacesToKeyValueTableArray(const std::vector<GuidePlace>& places);

KeyValueTable GuideServiceToKeyValueTable(const GuideService& service);
KeyValueTableArray GuideServicesToKeyValueTableArray(const std::vector<GuideService>& services);

// The whole page, body included.
KeyValueTable GuidePageToKeyValueTable(const GuidePage& page);

KeyValueTable GuidePageSummaryToKeyValueTable(const GuidePageSummary& page);
KeyValueTableArray GuidePageSummariesToKeyValueTableArray(
    const std::vector<GuidePageSummary>& pages);

KeyValueTable GuideLinkToKeyValueTable(const GuideLink& link);
KeyValueTableArray GuideLinksToKeyValueTableArray(const std::vector<GuideLink>& links);

KeyValueTable SceneSummaryToKeyValueTable(const SceneSummary& scene);
KeyValueTableArray SceneSummariesToKeyValueTableArray(const std::vector<SceneSummary>& scenes);

KeyValueTable NeighborhoodSummaryToKeyValueTable(const NeighborhoodSummary& neighborhood);
KeyValueTableArray NeighborhoodSummariesToKeyValueTableArray(
    const std::vector<NeighborhoodSummary>& neighborhoods);

}  // namespace Guide
