#include "business_logic/events/event_helper.h"

#include <algorithm>
#include <map>
#include <set>

#include "db_schema/categories.h"
#include "db_schema/event_category_assignments.h"
#include "db_schema/event_scene_tag_assignments.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/series_category_assignments.h"
#include "db_schema/series_scene_tag_assignments.h"
#include "db_schema/venues.h"
#include "util/types.h"

namespace Events {

namespace {

using SlugToId = std::map<std::string, int64_t>;
using IdToSlugs = std::map<int64_t, std::vector<std::string>>;

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

// NULL reads back as "", which is "no value" here.
std::optional<int64_t> OptionalIdColumn(const KeyValueTable& row, std::string_view column) {
    std::string value = Column(row, column);
    if (value.empty()) {
        return std::nullopt;
    }
    return std::stoll(value);
}

// slug → id over a vocabulary's GetAll, which is enabled rows only — so a
// disabled slug resolves to nothing, exactly like an unknown one.
SlugToId SlugIds(const KeyValueTableArray& rows, std::string_view idColumn,
                 std::string_view slugColumn) {
    SlugToId ids;
    for (const KeyValueTable& row : rows) {
        ids[Column(row, slugColumn)] = std::stoll(Column(row, idColumn));
    }
    return ids;
}

// owner id → its slugs, in the order the GetFor* query returned them (the
// vocabulary's display order).
IdToSlugs SlugsByOwner(const KeyValueTableArray& rows, std::string_view ownerColumn,
                       std::string_view slugColumn) {
    IdToSlugs slugs;
    for (const KeyValueTable& row : rows) {
        slugs[std::stoll(Column(row, ownerColumn))].push_back(Column(row, slugColumn));
    }
    return slugs;
}

std::set<int64_t> IdSet(const KeyValueTableArray& rows, std::string_view idColumn) {
    std::set<int64_t> ids;
    for (const KeyValueTable& row : rows) {
        ids.insert(std::stoll(Column(row, idColumn)));
    }
    return ids;
}

bool Contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

// The item's stated assignments for one axis, resolved against the enabled
// vocabulary. Unresolvable slugs are dropped and reported, not fatal.
struct StatedAssignments {
    bool stated = false;
    std::set<int64_t> ids;
    std::vector<std::string> ignoredSlugs;
};

StatedAssignments ResolveSlugs(const std::optional<std::vector<std::string>>& slugs,
                               const SlugToId& known) {
    StatedAssignments resolved;
    if (!slugs) {
        return resolved;
    }
    resolved.stated = true;
    for (const std::string& slug : *slugs) {
        auto it = known.find(slug);
        if (it == known.end()) {
            resolved.ignoredSlugs.push_back(slug);
        } else {
            resolved.ids.insert(it->second);
        }
    }
    return resolved;
}

// Pass 1's output for one item: everything resolved, nothing written yet.
struct PreparedItem {
    std::optional<int64_t> sourceVenueId;
    bool statesSeries = false;
    std::optional<int64_t> seriesId;  // meaningful only when statesSeries
    StatedAssignments categories;
    StatedAssignments sceneTags;
    std::string ignoredSeriesSlug;
};

// Makes the event's own assignments on one axis exactly `wanted`.
template <class AssignmentHelper>
void ReplaceAssignments(Transaction& transaction, AssignmentHelper& helper, int64_t eventId,
                        const std::set<int64_t>& current, const std::set<int64_t>& wanted) {
    for (int64_t id : current) {
        if (wanted.count(id) == 0) {
            helper.Remove(transaction, eventId, id);
        }
    }
    for (int64_t id : wanted) {
        if (current.count(id) == 0) {
            helper.Assign(transaction, eventId, id);
        }
    }
}

}  // namespace

UpcomingWindow ClampUpcomingWindow(int64_t nowUs, int64_t fromUs, int64_t toUs) {
    const int64_t earliestUs = nowUs - kUpcomingLookBackDays * kUsPerDay;
    UpcomingWindow window;
    window.fromUs = fromUs > 0 ? std::max(fromUs, earliestUs) : nowUs;
    window.toUs = toUs > 0 ? toUs : window.fromUs + kUpcomingDefaultWindowDays * kUsPerDay;
    window.toUs = std::min(window.toUs, window.fromUs + kUpcomingMaxWindowDays * kUsPerDay);
    // A backwards request is an empty window, not an error.
    window.toUs = std::max(window.toUs, window.fromUs);
    return window;
}

EventHelper::EventHelper(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper),
      categories_(databaseHelper),
      sceneTags_(databaseHelper),
      neighborhoods_(databaseHelper),
      venues_(databaseHelper),
      eventSources_(databaseHelper),
      series_(databaseHelper),
      events_(databaseHelper),
      eventCategories_(databaseHelper),
      eventSceneTags_(databaseHelper),
      seriesCategories_(databaseHelper),
      seriesSceneTags_(databaseHelper) {
}

int64_t EventHelper::NowUs(Transaction& transaction) const {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

IngestResult EventHelper::IngestEvents(
    Transaction& transaction, const std::vector<IngestItem>& items) {
    IngestResult result;
    const SlugToId categoryIds = SlugIds(
        categories_.GetAll(transaction), DbSchema::kCategoriesId, DbSchema::kCategoriesSlug);
    const SlugToId sceneTagIds = SlugIds(
        sceneTags_.GetAll(transaction), DbSchema::kSceneTagsId, DbSchema::kSceneTagsSlug);

    // Pass 1 — validate and resolve every item before writing any.
    std::vector<PreparedItem> prepared(items.size());
    for (size_t index = 0; index < items.size(); ++index) {
        const IngestItem& item = items[index];
        PreparedItem& ready = prepared[index];
        auto fail = [&](std::string_view errorCode) {
            result.errorCode = std::string(errorCode);
            result.errorItemIndex = static_cast<int64_t>(index);
            return result;
        };

        KeyValueTable source = item.sourceId > 0
            ? eventSources_.GetSource(transaction, item.sourceId) : KeyValueTable();
        if (source.empty()) return fail(kEventErrorSourceNotFound);
        if (item.externalKey.empty()) return fail(kEventErrorMissingExternalKey);
        if (item.title.empty()) return fail(kEventErrorMissingTitle);
        if (item.startsAtUs <= 0) return fail(kEventErrorMissingStart);
        if (item.endsAtUs && *item.endsAtUs < item.startsAtUs) {
            return fail(kEventErrorEndBeforeStart);
        }
        if (item.venueId && (*item.venueId <= 0
                             || venues_.GetVenue(transaction, *item.venueId).empty())) {
            return fail(kEventErrorVenueNotFound);
        }

        ready.sourceVenueId = OptionalIdColumn(source, DbSchema::kEventSourcesVenueId);
        if (item.seriesSlug) {
            if (item.seriesSlug->empty()) {
                ready.statesSeries = true;  // stated: no series
            } else {
                KeyValueTable seriesRow = series_.GetSeriesBySlug(transaction, *item.seriesSlug);
                if (seriesRow.empty()) {
                    ready.ignoredSeriesSlug = *item.seriesSlug;
                } else {
                    ready.statesSeries = true;
                    ready.seriesId = std::stoll(Column(seriesRow, DbSchema::kSeriesId));
                }
            }
        }
        ready.categories = ResolveSlugs(item.categorySlugs, categoryIds);
        ready.sceneTags = ResolveSlugs(item.sceneTagSlugs, sceneTagIds);
    }

    // Pass 2 — upsert.
    result.items.reserve(items.size());
    for (size_t index = 0; index < items.size(); ++index) {
        const IngestItem& item = items[index];
        const PreparedItem& ready = prepared[index];
        IngestItemResult itemResult;
        itemResult.ignoredCategorySlugs = ready.categories.ignoredSlugs;
        itemResult.ignoredSceneTagSlugs = ready.sceneTags.ignoredSlugs;
        itemResult.ignoredSeriesSlug = ready.ignoredSeriesSlug;

        KeyValueTable existing = events_.GetEventBySourceAndExternalKey(
            transaction, item.sourceId, item.externalKey);

        if (existing.empty()) {
            KeyValueTable values = {
                { std::string(DbSchema::kEventsTitle), item.title },
                { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(item.startsAtUs) },
                { std::string(DbSchema::kEventsSourceId), StringFromInt(item.sourceId) },
                { std::string(DbSchema::kEventsExternalKey), item.externalKey },
                { std::string(DbSchema::kEventsOrigin), std::string(DbSchema::kEventsOriginScanned) },
                { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusPending) },
            };
            if (item.endsAtUs) {
                values[std::string(DbSchema::kEventsEndsAtUs)] = StringFromInt(*item.endsAtUs);
            }
            if (item.description) values[std::string(DbSchema::kEventsDescription)] = *item.description;
            if (item.url) values[std::string(DbSchema::kEventsUrl)] = *item.url;
            if (item.costText) values[std::string(DbSchema::kEventsCostText)] = *item.costText;
            if (item.locationText) {
                values[std::string(DbSchema::kEventsLocationText)] = *item.locationText;
            }
            std::optional<int64_t> venueId = item.venueId ? item.venueId : ready.sourceVenueId;
            if (venueId) values[std::string(DbSchema::kEventsVenueId)] = StringFromInt(*venueId);
            if (ready.statesSeries && ready.seriesId) {
                values[std::string(DbSchema::kEventsSeriesId)] = StringFromInt(*ready.seriesId);
            }

            itemResult.eventId = events_.AddEvent(transaction, values);
            ReplaceAssignments(transaction, eventCategories_, itemResult.eventId, {},
                               ready.categories.ids);
            ReplaceAssignments(transaction, eventSceneTags_, itemResult.eventId, {},
                               ready.sceneTags.ids);
            itemResult.outcome = std::string(kIngestOutcomeCreated);
            result.items.push_back(std::move(itemResult));
            continue;
        }

        itemResult.eventId = std::stoll(Column(existing, DbSchema::kEventsId));
        const std::string status = Column(existing, DbSchema::kEventsStatus);
        if (status == DbSchema::kEventsStatusRejected) {
            itemResult.outcome = std::string(kIngestOutcomeRejectedKept);
            result.items.push_back(std::move(itemResult));
            continue;
        }
        if (status == DbSchema::kEventsStatusArchived) {
            itemResult.outcome = std::string(kIngestOutcomeArchivedKept);
            result.items.push_back(std::move(itemResult));
            continue;
        }

        KeyValueTable updates;
        bool majorChange = false;
        auto setIfChanged = [&](std::string_view column, const std::string& value, bool major) {
            if (Column(existing, column) != value) {
                updates[std::string(column)] = value;
                majorChange = majorChange || major;
            }
        };
        setIfChanged(DbSchema::kEventsTitle, item.title, false);
        setIfChanged(DbSchema::kEventsStartsAtUs, StringFromInt(item.startsAtUs), true);
        if (item.endsAtUs) setIfChanged(DbSchema::kEventsEndsAtUs, StringFromInt(*item.endsAtUs), true);
        if (item.venueId) setIfChanged(DbSchema::kEventsVenueId, StringFromInt(*item.venueId), true);
        if (item.description) setIfChanged(DbSchema::kEventsDescription, *item.description, false);
        if (item.url) setIfChanged(DbSchema::kEventsUrl, *item.url, false);
        if (item.costText) setIfChanged(DbSchema::kEventsCostText, *item.costText, false);
        if (item.locationText) setIfChanged(DbSchema::kEventsLocationText, *item.locationText, false);
        if (ready.statesSeries) {
            std::string seriesValue = ready.seriesId ? StringFromInt(*ready.seriesId) : "";
            if (Column(existing, DbSchema::kEventsSeriesId) != seriesValue) {
                updates[std::string(DbSchema::kEventsSeriesId)] =
                    ready.seriesId ? seriesValue : "NULL";
            }
        }

        std::set<int64_t> currentCategories;
        std::set<int64_t> currentSceneTags;
        bool assignmentsChanged = false;
        if (ready.categories.stated) {
            currentCategories = IdSet(eventCategories_.GetForEvent(transaction, itemResult.eventId),
                                      DbSchema::kEventCategoryAssignmentsCategoryId);
            assignmentsChanged = assignmentsChanged || currentCategories != ready.categories.ids;
        }
        if (ready.sceneTags.stated) {
            currentSceneTags = IdSet(eventSceneTags_.GetForEvent(transaction, itemResult.eventId),
                                     DbSchema::kEventSceneTagAssignmentsSceneTagId);
            assignmentsChanged = assignmentsChanged || currentSceneTags != ready.sceneTags.ids;
        }

        if (updates.empty() && !assignmentsChanged) {
            itemResult.outcome = std::string(kIngestOutcomeUnchanged);
            result.items.push_back(std::move(itemResult));
            continue;
        }

        // A moved date or venue on a public event goes back past a human first:
        // a silently moved date could strand someone at the wrong bar.
        const bool revert = majorChange && status == DbSchema::kEventsStatusApproved;
        if (revert) {
            updates[std::string(DbSchema::kEventsStatus)] = std::string(DbSchema::kEventsStatusPending);
            updates[std::string(DbSchema::kEventsReviewNotes)] = std::string(kReviewNoteChangedByRescan);
        }
        // Always through UpdateEvent, so an assignment-only change still bumps updated_us.
        events_.UpdateEvent(transaction, itemResult.eventId, updates);
        if (ready.categories.stated) {
            ReplaceAssignments(transaction, eventCategories_, itemResult.eventId,
                               currentCategories, ready.categories.ids);
        }
        if (ready.sceneTags.stated) {
            ReplaceAssignments(transaction, eventSceneTags_, itemResult.eventId,
                               currentSceneTags, ready.sceneTags.ids);
        }
        itemResult.outcome = std::string(revert ? kIngestOutcomeRevertedToPending
                                                : kIngestOutcomeUpdated);
        result.items.push_back(std::move(itemResult));
    }

    result.ok = true;
    return result;
}

ReviewResult EventHelper::ReviewEvent(
    Transaction& transaction,
    int64_t eventId,
    ReviewAction action,
    int64_t reviewerPersonId,
    std::string_view notes) {
    ReviewResult result;
    KeyValueTable event = events_.GetEvent(transaction, eventId);
    if (event.empty()) {
        result.errorCode = std::string(kEventErrorNotFound);
        return result;
    }

    const std::string current = Column(event, DbSchema::kEventsStatus);
    const std::string target(action == ReviewAction::Approve ? DbSchema::kEventsStatusApproved
                                                             : DbSchema::kEventsStatusRejected);
    const bool allowed = current == DbSchema::kEventsStatusPending
        || (current == DbSchema::kEventsStatusApproved && target == DbSchema::kEventsStatusRejected)
        || (current == DbSchema::kEventsStatusRejected && target == DbSchema::kEventsStatusApproved);
    if (!allowed) {
        result.errorCode = std::string(kEventErrorInvalidTransition);
        result.status = current;
        return result;
    }

    events_.UpdateEvent(transaction, eventId, {
        { std::string(DbSchema::kEventsStatus), target },
        { std::string(DbSchema::kEventsReviewedByPersonId), StringFromInt(reviewerPersonId) },
        { std::string(DbSchema::kEventsReviewedUs), std::string(DbSchema::kDatabaseInfoDefaultNow) },
        { std::string(DbSchema::kEventsReviewNotes), std::string(notes) },
    });
    result.ok = true;
    result.status = target;
    return result;
}

std::vector<PublicEvent> EventHelper::GetUpcomingPublic(
    Transaction& transaction, const UpcomingQuery& query) {
    const UpcomingWindow window = ClampUpcomingWindow(NowUs(transaction), query.fromUs, query.toUs);
    if (window.toUs <= window.fromUs) {
        return {};
    }

    // The single-column filters first, so the batch reads below only cover
    // events that can still be returned.
    std::vector<KeyValueTable> candidates;
    for (KeyValueTable& row : events_.GetEventsInRange(
             transaction, window.fromUs, window.toUs, DbSchema::kEventsStatusApproved)) {
        if (query.venueId > 0
            && OptionalIdColumn(row, DbSchema::kEventsVenueId) != query.venueId) {
            continue;
        }
        if (query.seriesId > 0
            && OptionalIdColumn(row, DbSchema::kEventsSeriesId) != query.seriesId) {
            continue;
        }
        candidates.push_back(std::move(row));
    }
    if (candidates.empty()) {
        return {};
    }

    std::vector<int64_t> eventIds;
    std::set<int64_t> venueIdSet;
    std::set<int64_t> seriesIdSet;
    for (const KeyValueTable& row : candidates) {
        eventIds.push_back(std::stoll(Column(row, DbSchema::kEventsId)));
        if (auto venueId = OptionalIdColumn(row, DbSchema::kEventsVenueId)) venueIdSet.insert(*venueId);
        if (auto seriesId = OptionalIdColumn(row, DbSchema::kEventsSeriesId)) seriesIdSet.insert(*seriesId);
    }
    const std::vector<int64_t> venueIds(venueIdSet.begin(), venueIdSet.end());
    const std::vector<int64_t> seriesIds(seriesIdSet.begin(), seriesIdSet.end());

    std::map<int64_t, KeyValueTable> venuesById;
    for (KeyValueTable& venue : venues_.GetVenuesByIds(transaction, venueIds)) {
        int64_t id = std::stoll(Column(venue, DbSchema::kVenuesId));
        venuesById[id] = std::move(venue);
    }
    std::map<int64_t, std::string> neighborhoodSlugs;
    for (const KeyValueTable& neighborhood : neighborhoods_.GetAll(transaction)) {
        neighborhoodSlugs[std::stoll(Column(neighborhood, DbSchema::kNeighborhoodsId))] =
            Column(neighborhood, DbSchema::kNeighborhoodsSlug);
    }
    std::map<int64_t, KeyValueTable> seriesById;
    for (KeyValueTable& seriesRow : series_.GetSeriesByIds(transaction, seriesIds)) {
        int64_t id = std::stoll(Column(seriesRow, DbSchema::kSeriesId));
        seriesById[id] = std::move(seriesRow);
    }
    const IdToSlugs ownCategories = SlugsByOwner(
        eventCategories_.GetForEvents(transaction, eventIds),
        DbSchema::kEventCategoryAssignmentsEventId,
        TableHelpers::kEventCategoryAssignmentsCategorySlug);
    const IdToSlugs ownSceneTags = SlugsByOwner(
        eventSceneTags_.GetForEvents(transaction, eventIds),
        DbSchema::kEventSceneTagAssignmentsEventId,
        TableHelpers::kEventSceneTagAssignmentsSceneTagSlug);
    const IdToSlugs seriesCategories = SlugsByOwner(
        seriesCategories_.GetForSeriesIds(transaction, seriesIds),
        DbSchema::kSeriesCategoryAssignmentsSeriesId,
        TableHelpers::kSeriesCategoryAssignmentsCategorySlug);
    const IdToSlugs seriesSceneTags = SlugsByOwner(
        seriesSceneTags_.GetForSeriesIds(transaction, seriesIds),
        DbSchema::kSeriesSceneTagAssignmentsSeriesId,
        TableHelpers::kSeriesSceneTagAssignmentsSceneTagSlug);

    // The inheritance pin, per axis: the event's own set when it has one,
    // otherwise its series'.
    auto effective = [](const IdToSlugs& own, const IdToSlugs& inherited, int64_t eventId,
                        std::optional<int64_t> seriesId) {
        auto ownIt = own.find(eventId);
        if (ownIt != own.end() && !ownIt->second.empty()) return ownIt->second;
        if (seriesId) {
            auto seriesIt = inherited.find(*seriesId);
            if (seriesIt != inherited.end()) return seriesIt->second;
        }
        return std::vector<std::string>();
    };

    std::vector<PublicEvent> events;
    for (const KeyValueTable& row : candidates) {
        PublicEvent event;
        event.id = std::stoll(Column(row, DbSchema::kEventsId));
        event.title = Column(row, DbSchema::kEventsTitle);
        event.description = Column(row, DbSchema::kEventsDescription);
        event.startsAtUs = std::stoll(Column(row, DbSchema::kEventsStartsAtUs));
        event.endsAtUs = OptionalIdColumn(row, DbSchema::kEventsEndsAtUs);
        event.venueId = OptionalIdColumn(row, DbSchema::kEventsVenueId);
        event.locationText = Column(row, DbSchema::kEventsLocationText);
        event.url = Column(row, DbSchema::kEventsUrl);
        event.costText = Column(row, DbSchema::kEventsCostText);
        event.seriesId = OptionalIdColumn(row, DbSchema::kEventsSeriesId);
        event.origin = Column(row, DbSchema::kEventsOrigin);

        if (event.venueId) {
            auto venueIt = venuesById.find(*event.venueId);
            if (venueIt != venuesById.end()) {
                event.venueName = Column(venueIt->second, DbSchema::kVenuesName);
                if (auto neighborhoodId =
                        OptionalIdColumn(venueIt->second, DbSchema::kVenuesNeighborhoodId)) {
                    auto slugIt = neighborhoodSlugs.find(*neighborhoodId);
                    if (slugIt != neighborhoodSlugs.end()) event.neighborhoodSlug = slugIt->second;
                }
            }
        }
        if (event.seriesId) {
            auto seriesIt = seriesById.find(*event.seriesId);
            if (seriesIt != seriesById.end()) {
                event.seriesSlug = Column(seriesIt->second, DbSchema::kSeriesSlug);
                event.seriesTitle = Column(seriesIt->second, DbSchema::kSeriesTitle);
            }
        }
        event.categorySlugs = effective(ownCategories, seriesCategories, event.id, event.seriesId);
        event.sceneTagSlugs = effective(ownSceneTags, seriesSceneTags, event.id, event.seriesId);

        if (!query.neighborhoodSlug.empty() && event.neighborhoodSlug != query.neighborhoodSlug) {
            continue;
        }
        if (!query.categorySlug.empty() && !Contains(event.categorySlugs, query.categorySlug)) {
            continue;
        }
        if (!query.sceneTagSlug.empty() && !Contains(event.sceneTagSlugs, query.sceneTagSlug)) {
            continue;
        }
        events.push_back(std::move(event));
    }
    return events;
}

}  // namespace Events
