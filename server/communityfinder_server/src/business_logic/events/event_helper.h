#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/past_event_settings.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/table_helpers/categories.h"
#include "sql_util/table_helpers/event_category_assignments.h"
#include "sql_util/table_helpers/event_scene_tag_assignments.h"
#include "sql_util/table_helpers/event_sources.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/series.h"
#include "sql_util/table_helpers/series_category_assignments.h"
#include "sql_util/table_helpers/series_scene_tag_assignments.h"
#include "sql_util/table_helpers/venues.h"

namespace Events {

// EV1 Slice 4 — the events domain: ingest (the scanner's and every importer's
// single door), review (approve/reject), and the public upcoming list.
//
// All error paths return a descriptive errorCode string instead of throwing so
// endpoints can surface validation failures as 400s (knottyyoga's
// ClassScheduleHelper convention).

// Ingest validation error codes. One bad item fails the whole batch before
// anything is written; IngestResult::errorItemIndex says which.
inline constexpr std::string_view kEventErrorSourceNotFound = "SOURCE_NOT_FOUND";
inline constexpr std::string_view kEventErrorMissingExternalKey = "MISSING_EXTERNAL_KEY";
inline constexpr std::string_view kEventErrorMissingTitle = "MISSING_TITLE";
inline constexpr std::string_view kEventErrorMissingStart = "MISSING_START";
inline constexpr std::string_view kEventErrorEndBeforeStart = "END_BEFORE_START";
inline constexpr std::string_view kEventErrorVenueNotFound = "VENUE_NOT_FOUND";

// Review error codes.
inline constexpr std::string_view kEventErrorNotFound = "EVENT_NOT_FOUND";
inline constexpr std::string_view kEventErrorInvalidTransition = "INVALID_TRANSITION";

// Per-item ingest outcomes.
inline constexpr std::string_view kIngestOutcomeCreated = "created";
inline constexpr std::string_view kIngestOutcomeUpdated = "updated";
inline constexpr std::string_view kIngestOutcomeUnchanged = "unchanged";
inline constexpr std::string_view kIngestOutcomeRevertedToPending = "reverted_to_pending";
// A rejected event is never resurrected by a re-scan — that's what makes reject
// meaningful against a weekly scanner.
inline constexpr std::string_view kIngestOutcomeRejectedKept = "rejected_kept";
// An archived event is past and settled (EV4); a re-scan leaves it alone.
inline constexpr std::string_view kIngestOutcomeArchivedKept = "archived_kept";

// The review note a re-scan leaves when it pulls an approved event back to
// pending because its date or venue moved.
inline constexpr std::string_view kReviewNoteChangedByRescan = "date/venue changed by re-scan";

// The public window (behavior pin): default now → +42 days, at most 92 days
// per request, and `from` may reach back one day (time-zone slack), or as far
// as the past-events window (EV4 §10) so the calendar can show past months.
inline constexpr int64_t kUsPerDay = 86'400LL * 1'000'000LL;
inline constexpr int64_t kUpcomingDefaultWindowDays = 42;
inline constexpr int64_t kUpcomingMaxWindowDays = 92;
inline constexpr int64_t kUpcomingLookBackDays = 1;

// The past-events list's page (EV4 §10): 20 by default, at most 50.
inline constexpr int64_t kPastEventsDefaultPageSize = 20;
inline constexpr int64_t kPastEventsMaxPageSize = 50;

// The ICS subscribe feed's window (EV2 pin): now − 1 day → now + 60 days.
inline constexpr int64_t kFeedLookBackDays = 1;
inline constexpr int64_t kFeedWindowDays = 60;

// The statuses the public may read (EV4 OQ3): approved events, and archived
// ones. Archived means "past and settled," not hidden. Pending and rejected
// never show. The windows still start at now, so the upcoming list and the
// feeds show what's coming.
bool IsPublicEventStatus(std::string_view status);
const std::vector<std::string_view>& PublicEventStatuses();

// When an event is over: its end, or its start + kICalDefaultDurationUs when it
// has none (EV2's ICS default, which the archive sweep uses too).
int64_t EventEndUs(int64_t startsAtUs, std::optional<int64_t> endsAtUs);

// The past-events window (EV4 §10, OQ8): a public-status event stays public
// until it has been over for `pastEventVisibleDays` days, and is hidden after
// that, not deleted. This is the earliest end a public event can have.
int64_t PastEventCutoffUs(int64_t nowUs, int64_t pastEventVisibleDays);

// The whole public rule: a public status and an end inside the window.
bool IsPublicEvent(std::string_view status, int64_t startsAtUs,
                   std::optional<int64_t> endsAtUs, int64_t nowUs,
                   int64_t pastEventVisibleDays);

// One item from a feed. Optional fields follow one rule: **absent = not
// stated** — a new event takes the column default, an existing one keeps what
// it has (so a curator's edit survives a feed that never mentions the field).
// Present = authoritative, even when empty.
struct IngestItem {
    int64_t sourceId = 0;
    std::string externalKey;
    std::string title;
    int64_t startsAtUs = 0;
    std::optional<int64_t> endsAtUs;
    std::optional<std::string> description;
    std::optional<std::string> url;
    std::optional<std::string> costText;
    std::optional<std::string> locationText;
    // Absent on a new event: the source's own venue (a venue_site feed's events
    // happen there), or none.
    std::optional<int64_t> venueId;
    std::optional<std::string> seriesSlug;
    // The event's OWN assignments, replacing whatever it has. Absent leaves
    // them alone; an empty list clears them, so the series' set applies again.
    std::optional<std::vector<std::string>> categorySlugs;
    std::optional<std::vector<std::string>> sceneTagSlugs;
};

struct IngestItemResult {
    int64_t eventId = 0;
    std::string outcome;  // kIngestOutcome*
    // Slugs that name nothing usable — unknown or disabled — dropped from the
    // item rather than failing the batch (a vocabulary edit must not stop the
    // weekly scan). The scanner logs them for a curator.
    std::vector<std::string> ignoredCategorySlugs;
    std::vector<std::string> ignoredSceneTagSlugs;
    std::string ignoredSeriesSlug;
};

struct IngestResult {
    bool ok = false;
    std::string errorCode;
    int64_t errorItemIndex = -1;
    // Parallel to the request's items; empty when !ok.
    std::vector<IngestItemResult> items;
};

enum class ReviewAction { Approve, Reject };

struct ReviewResult {
    bool ok = false;
    std::string errorCode;
    std::string status;  // the event's status after the review
};

// [fromUs, toUs) after clamping.
struct UpcomingWindow {
    int64_t fromUs = 0;
    int64_t toUs = 0;
};

// The public lists' filters. 0 / empty = not filtered.
struct EventFilters {
    std::string categorySlug;
    std::string sceneTagSlug;
    std::string neighborhoodSlug;
    int64_t venueId = 0;
    int64_t seriesId = 0;
};

struct UpcomingQuery : EventFilters {
    int64_t fromUs = 0;  // 0 = now
    int64_t toUs = 0;    // 0 = from + the default window
};

// EV4 §10's past-events list, newest first.
struct PastQuery : EventFilters {
    int64_t offset = 0;                         // matching events to skip
    int64_t limit = kPastEventsDefaultPageSize;  // clamped to 1..kPastEventsMaxPageSize
};

// A public (approved or archived) event as the public sees it. Deliberately has
// no review fields: a public shape cannot leak what it never carries.
struct PublicEvent {
    int64_t id = 0;
    std::string title;
    std::string description;
    int64_t startsAtUs = 0;
    std::optional<int64_t> endsAtUs;
    std::optional<int64_t> venueId;
    std::string venueName;
    // One line from the venue's parts (FormatVenueAddress); empty without a venue.
    std::string venueAddress;
    std::string neighborhoodSlug;
    std::string locationText;
    std::string url;
    std::string costText;
    // Effective: the event's own, else its series' (per axis).
    std::vector<std::string> categorySlugs;
    std::vector<std::string> sceneTagSlugs;
    std::optional<int64_t> seriesId;
    std::string seriesSlug;
    std::string seriesTitle;
    std::string origin;
};

// GetUpcomingPublic's answer: the window actually served (after the clamp), so
// a client paging through windows (EV2's calendar) knows where the next starts.
struct UpcomingEvents {
    UpcomingWindow window;
    std::vector<PublicEvent> events;
};

// GetPastPublic's answer: one page, and whether another follows.
struct PastEvents {
    std::vector<PublicEvent> events;
    bool hasMore = false;
    // The window's start (PastEventCutoffUs): nothing that ended before it is
    // listed.
    int64_t sinceUs = 0;
};

// One event, whatever its status, for the detail page. `status` and
// `reviewNotes` are for a curator's preview; the endpoint decides who sees them.
struct EventDetail {
    PublicEvent event;
    std::string status;
    std::string reviewNotes;
    // IsPublicEvent, with the helper's past-events window.
    bool isPublic = false;
};

// A series as the public sees it: the durable label plus its OWN category and
// scene-tag slugs (what its instances inherit). `status` is the freshness
// status (open / seasonal / unverified / closed / moved), which is public.
struct PublicSeries {
    int64_t id = 0;
    std::string title;
    std::string slug;
    std::string description;
    std::string cadenceText;
    std::string url;
    std::optional<int64_t> venueId;
    std::string venueName;
    std::string neighborhoodSlug;
    std::string status;
    std::optional<int64_t> lastVerifiedAtUs;
    std::vector<std::string> categorySlugs;
    std::vector<std::string> sceneTagSlugs;
};

// A series page: the series plus its approved instances in the default window.
struct SeriesDetail {
    PublicSeries series;
    UpcomingEvents upcoming;
};

// The clamp itself, separate so tests can pin it without a DB. `from` reaches
// back the larger of kUpcomingLookBackDays and the past-events window.
UpcomingWindow ClampUpcomingWindow(int64_t nowUs, int64_t fromUs, int64_t toUs,
                                   int64_t pastEventVisibleDays);

// A venue's address parts as one line — "1533 13th Ave, Seattle, WA 98122" —
// skipping whatever is blank.
std::string FormatVenueAddress(std::string_view street, std::string_view city,
                               std::string_view state, std::string_view zip);

class EventHelper {
public:
    // `pastEventVisibleDays` is the past-events window every public read
    // applies (PastEventCutoffUs). Endpoints pass the site's setting
    // (LoadPastEventSettings); the default is that setting's default.
    explicit EventHelper(
        DatabaseHelper databaseHelper,
        int64_t pastEventVisibleDays = kArchivedEventsVisibleDaysRange.defaultValue);
    EventHelper(const EventHelper&) = default;
    EventHelper& operator=(const EventHelper&) = default;
    ~EventHelper() = default;

    // The upsert pin: match on (source_id, external_key). New → pending +
    // scanned. Existing → minor changes (title, description, url, cost,
    // location, series, categories, scene tags) update and keep the status;
    // major changes (start, end, venue) update and pull an approved event back
    // to pending with kReviewNoteChangedByRescan; no change → unchanged.
    // Rejected and archived events are never touched. All items are validated
    // before any is written, and the caller's transaction makes the batch atomic.
    IngestResult IngestEvents(Transaction& transaction, const std::vector<IngestItem>& items);

    // pending → approved | rejected, and approved ↔ rejected (corrections).
    // Anything else — including archived (un-archiving is a raw admin edit) and
    // re-applying the current status — is INVALID_TRANSITION. Stamps the review
    // quartet with now_us().
    ReviewResult ReviewEvent(
        Transaction& transaction,
        int64_t eventId,
        ReviewAction action,
        int64_t reviewerPersonId,
        std::string_view notes);

    // Public events (IsPublicEvent) starting in the clamped window, in start
    // order, each with its venue, neighborhood, series and EFFECTIVE category /
    // scene-tag slugs resolved in a handful of batch queries. Category and
    // scene filters match the effective sets (the OQ5 inheritance pin).
    UpcomingEvents GetUpcomingPublic(Transaction& transaction, const UpcomingQuery& query);

    // EV4 §10: public events that started before now and are still inside the
    // past-events window, newest start first, one page at a time, with the
    // upcoming list's filters. An event that is still going counts: it has
    // left the upcoming list, which starts at now.
    PastEvents GetPastPublic(Transaction& transaction, const PastQuery& query);

    // The ICS feed's events: GetUpcomingPublic over now − kFeedLookBackDays →
    // now + kFeedWindowDays with the caller's filters. Any window in `filters`
    // is ignored — the feed's window is fixed.
    UpcomingEvents GetFeedEvents(Transaction& transaction, UpcomingQuery filters);

    // The feed's filters by name, for its calendar name: the category's,
    // scene's and neighborhood's display names and the series' title, in that
    // order, skipping the unset ones. A value that names nothing is shown as
    // given. The window and venueId (the feed takes no venue) are not filters here.
    std::vector<std::string> GetFeedFilterNames(
        Transaction& transaction, const UpcomingQuery& filters);

    // One event in the public shape, any status; nullopt when there is no such
    // event. Visibility is the caller's call: `isPublic` says whether the
    // public may see it.
    std::optional<EventDetail> GetEventDetail(Transaction& transaction, int64_t eventId);

    // The series index: every active series (Series::GetActiveSeries — all but
    // closed and moved), ordered by title, with its own slugs.
    std::vector<PublicSeries> GetActiveSeries(Transaction& transaction);

    // A series page by slug, any freshness status (a closed series keeps its
    // page); nullopt when there is no such series.
    std::optional<SeriesDetail> GetSeriesDetail(Transaction& transaction, std::string_view slug);

private:
    DatabaseHelper databaseHelper_;
    int64_t pastEventVisibleDays_;
    TableHelpers::Categories categories_;
    TableHelpers::SceneTags sceneTags_;
    TableHelpers::Neighborhoods neighborhoods_;
    TableHelpers::Venues venues_;
    TableHelpers::EventSources eventSources_;
    TableHelpers::Series series_;
    TableHelpers::Events events_;
    TableHelpers::EventCategoryAssignments eventCategories_;
    TableHelpers::EventSceneTagAssignments eventSceneTags_;
    TableHelpers::SeriesCategoryAssignments seriesCategories_;
    TableHelpers::SeriesSceneTagAssignments seriesSceneTags_;

    int64_t NowUs(Transaction& transaction) const;

    // Event / series rows → public shapes, resolving venues, neighborhoods,
    // series and slugs in batch queries (a fixed number, however many rows).
    std::vector<PublicEvent> AssemblePublicEvents(
        Transaction& transaction, const KeyValueTableArray& rows);
    std::vector<PublicSeries> AssemblePublicSeries(
        Transaction& transaction, const KeyValueTableArray& rows);
};

// An EventHelper with the site's past-events window (LoadPastEventSettings),
// for the endpoints that serve the public.
EventHelper MakePublicEventHelper(
    DatabaseHelper databaseHelper, Secrets::SecretsHelper& secrets, Transaction& transaction);

}  // namespace Events
