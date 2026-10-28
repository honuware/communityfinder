#include "business_logic/events/event_helper.h"

#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/categories.h"
#include "db_schema/event_sources.h"
#include "db_schema/events.h"
#include "db_schema/freshness.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/scene_tags.h"
#include "db_schema/series.h"
#include "db_schema/venues.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace Events {
namespace {

constexpr int64_t kReviewerPersonId = 77;
constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;

// The places, vocabulary and sources the tests share — built at the top of each
// test (no fixtures). The series T4T carries category `dance` and scene `trans`,
// which its instances inherit.
struct World {
    int64_t theCuff = 0;       // venue in capitol-hill
    int64_t sunsetTavern = 0;  // venue in ballard
    int64_t feed = 0;          // an ICS feed with no venue of its own
    int64_t cuffSite = 0;      // The Cuff's own site: its events happen there
    int64_t t4t = 0;           // series
};

World MakeWorld(Transaction& transaction, DatabaseHelper databaseHelper) {
    World world;
    TableHelpers::Neighborhoods neighborhoods(databaseHelper);
    auto addNeighborhood = [&](std::string_view slug) {
        return neighborhoods.AddNeighborhood(transaction, {
            { std::string(DbSchema::kNeighborhoodsSlug), std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsDisplayName), std::string(slug) },
            { std::string(DbSchema::kNeighborhoodsRegion),
              std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
        });
    };
    int64_t capitolHill = addNeighborhood("capitol-hill");
    int64_t ballard = addNeighborhood("ballard");

    TableHelpers::Venues venues(databaseHelper);
    world.theCuff = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "The Cuff" },
        { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(capitolHill) },
    });
    world.sunsetTavern = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), "Sunset Tavern" },
        { std::string(DbSchema::kVenuesNeighborhoodId), StringFromInt(ballard) },
    });

    TableHelpers::Categories categories(databaseHelper);
    auto addCategory = [&](std::string_view slug, int64_t sortOrder, bool enabled) {
        return categories.AddCategory(transaction, {
            { std::string(DbSchema::kCategoriesSlug), std::string(slug) },
            { std::string(DbSchema::kCategoriesDisplayName), std::string(slug) },
            { std::string(DbSchema::kCategoriesSortOrder), StringFromInt(sortOrder) },
            { std::string(DbSchema::kCategoriesEnabled), enabled ? "true" : "false" },
        });
    };
    int64_t dance = addCategory("dance", 10, true);
    addCategory("drag", 20, true);
    addCategory("bar", 30, true);
    addCategory("retired", 40, false);

    TableHelpers::SceneTags sceneTags(databaseHelper);
    auto addSceneTag = [&](std::string_view slug, int64_t sortOrder) {
        return sceneTags.AddSceneTag(transaction, {
            { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
            { std::string(DbSchema::kSceneTagsDisplayName), std::string(slug) },
            { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
        });
    };
    int64_t trans = addSceneTag("trans", 10);
    addSceneTag("bears", 20);

    TableHelpers::EventSources sources(databaseHelper);
    world.feed = sources.AddSource(transaction, {
        { std::string(DbSchema::kEventSourcesName), "Community calendar" },
        { std::string(DbSchema::kEventSourcesKind), std::string(DbSchema::kEventSourcesKindIcsFeed) },
    });
    world.cuffSite = sources.AddSource(transaction, {
        { std::string(DbSchema::kEventSourcesName), "The Cuff site" },
        { std::string(DbSchema::kEventSourcesKind),
          std::string(DbSchema::kEventSourcesKindVenueSite) },
        { std::string(DbSchema::kEventSourcesVenueId), StringFromInt(world.theCuff) },
    });

    TableHelpers::Series series(databaseHelper);
    world.t4t = series.AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), "T4T" },
        { std::string(DbSchema::kSeriesSlug), "t4t" },
    });
    TableHelpers::SeriesCategoryAssignments(databaseHelper).Assign(transaction, world.t4t, dance);
    TableHelpers::SeriesSceneTagAssignments(databaseHelper).Assign(transaction, world.t4t, trans);
    return world;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

IngestItem Item(int64_t sourceId, std::string_view externalKey, int64_t startsAtUs) {
    IngestItem item;
    item.sourceId = sourceId;
    item.externalKey = std::string(externalKey);
    item.title = "Event " + std::string(externalKey);
    item.startsAtUs = startsAtUs;
    return item;
}

IngestItemResult IngestOne(Transaction& transaction, EventHelper& helper, const IngestItem& item) {
    IngestResult result = helper.IngestEvents(transaction, { item });
    EXPECT_TRUE(result.ok) << result.errorCode;
    return result.ok && result.items.size() == 1 ? result.items[0] : IngestItemResult();
}

void Approve(Transaction& transaction, EventHelper& helper, int64_t eventId) {
    ReviewResult review =
        helper.ReviewEvent(transaction, eventId, ReviewAction::Approve, kReviewerPersonId, "ok");
    EXPECT_TRUE(review.ok) << review.errorCode;
}

std::string ColumnOf(const KeyValueTable& row, std::string_view column) {
    return row.at(std::string(column));
}

std::vector<std::string> OwnCategorySlugs(Transaction& transaction, DatabaseHelper databaseHelper,
                                          int64_t eventId) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : TableHelpers::EventCategoryAssignments(databaseHelper)
                                        .GetForEvent(transaction, eventId)) {
        slugs.push_back(row.at(std::string(TableHelpers::kEventCategoryAssignmentsCategorySlug)));
    }
    return slugs;
}

std::vector<int64_t> IdsOf(const std::vector<PublicEvent>& events) {
    std::vector<int64_t> ids;
    for (const PublicEvent& event : events) {
        ids.push_back(event.id);
    }
    return ids;
}

std::vector<int64_t> IdsOf(const UpcomingEvents& upcoming) {
    return IdsOf(upcoming.events);
}

const PublicEvent* FindEvent(const std::vector<PublicEvent>& events, int64_t id) {
    for (const PublicEvent& event : events) {
        if (event.id == id) return &event;
    }
    return nullptr;
}

// ---- The clamp (no database) ----

constexpr int64_t kNow = 1'800'000'000'000'000LL;

TEST(ClampUpcomingWindowTest, DefaultsToNowPlusTheDefaultWindow) {
    UpcomingWindow window = ClampUpcomingWindow(kNow, 0, 0);
    EXPECT_EQ(window.fromUs, kNow);
    EXPECT_EQ(window.toUs, kNow + kUpcomingDefaultWindowDays * kUsPerDay);
}

TEST(ClampUpcomingWindowTest, CapsAtTheMaxWindow) {
    UpcomingWindow window = ClampUpcomingWindow(kNow, kNow, kNow + 200 * kUsPerDay);
    EXPECT_EQ(window.toUs, kNow + kUpcomingMaxWindowDays * kUsPerDay);

    // Inside the cap, an explicit end is kept as asked.
    window = ClampUpcomingWindow(kNow, kNow, kNow + 10 * kUsPerDay);
    EXPECT_EQ(window.toUs, kNow + 10 * kUsPerDay);
}

TEST(ClampUpcomingWindowTest, FromReachesBackOneDayAtMost) {
    UpcomingWindow window = ClampUpcomingWindow(kNow, kNow - 10 * kUsPerDay, 0);
    EXPECT_EQ(window.fromUs, kNow - kUpcomingLookBackDays * kUsPerDay);
    EXPECT_EQ(window.toUs, window.fromUs + kUpcomingDefaultWindowDays * kUsPerDay);

    // Within the slack (an evening already under way), kept.
    window = ClampUpcomingWindow(kNow, kNow - 3 * kUsPerHour, 0);
    EXPECT_EQ(window.fromUs, kNow - 3 * kUsPerHour);
}

TEST(ClampUpcomingWindowTest, BackwardsRequestIsAnEmptyWindow) {
    UpcomingWindow window =
        ClampUpcomingWindow(kNow, kNow + 5 * kUsPerDay, kNow + 2 * kUsPerDay);
    EXPECT_EQ(window.toUs, window.fromUs);
}

// ---- Ingest ----

TEST(EventHelperTest, IngestCreatesPendingScannedEvents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("IngestCreatesPendingScannedEvents", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;

        IngestItem atTheCuff = Item(world.cuffSite, "cuff-1", startsAtUs);
        atTheCuff.endsAtUs = startsAtUs + 3 * kUsPerHour;
        atTheCuff.description = "Second Saturdays.";
        atTheCuff.costText = "$10";
        atTheCuff.seriesSlug = "t4t";
        atTheCuff.categorySlugs = std::vector<std::string>{ "drag" };
        IngestItem fromTheFeed = Item(world.feed, "feed-1", startsAtUs);
        IngestItem elsewhere = Item(world.cuffSite, "cuff-2", startsAtUs);
        elsewhere.venueId = world.sunsetTavern;

        IngestResult result =
            helper.IngestEvents(transaction, { atTheCuff, fromTheFeed, elsewhere });
        ASSERT_TRUE(result.ok) << result.errorCode;
        ASSERT_EQ(result.items.size(), 3u);
        for (const IngestItemResult& item : result.items) {
            EXPECT_EQ(item.outcome, kIngestOutcomeCreated);
            EXPECT_GT(item.eventId, 0);
        }

        TableHelpers::Events events(databaseHelper);
        KeyValueTable row = events.GetEvent(transaction, result.items[0].eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusPending);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsOrigin), DbSchema::kEventsOriginScanned);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsSourceId), StringFromInt(world.cuffSite));
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsExternalKey), "cuff-1");
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsEndsAtUs),
                  StringFromInt(startsAtUs + 3 * kUsPerHour));
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsDescription), "Second Saturdays.");
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsCostText), "$10");
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsSeriesId), StringFromInt(world.t4t));
        // No venue stated: a venue_site source's events happen at its venue.
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsVenueId), StringFromInt(world.theCuff));
        EXPECT_EQ(OwnCategorySlugs(transaction, databaseHelper, result.items[0].eventId),
                  std::vector<std::string>{ "drag" });

        // A feed with no venue of its own leaves the venue empty…
        row = events.GetEvent(transaction, result.items[1].eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsVenueId), "");
        // …and a stated venue beats the source's.
        row = events.GetEvent(transaction, result.items[2].eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsVenueId), StringFromInt(world.sunsetTavern));
    });
}

TEST(EventHelperTest, ReingestingAnIdenticalBatchIsAllUnchanged) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ReingestingAnIdenticalBatchIsAllUnchanged", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;

        IngestItem first = Item(world.cuffSite, "cuff-1", startsAtUs);
        first.endsAtUs = startsAtUs + kUsPerHour;
        first.description = "Bring a friend.";
        first.seriesSlug = "t4t";
        first.categorySlugs = std::vector<std::string>{ "drag", "bar" };
        first.sceneTagSlugs = std::vector<std::string>{ "bears" };
        IngestItem second = Item(world.feed, "feed-1", startsAtUs);
        second.venueId = world.sunsetTavern;
        std::vector<IngestItem> batch = { first, second };

        IngestResult created = helper.IngestEvents(transaction, batch);
        ASSERT_TRUE(created.ok) << created.errorCode;
        Approve(transaction, helper, created.items[0].eventId);

        IngestResult again = helper.IngestEvents(transaction, batch);
        ASSERT_TRUE(again.ok) << again.errorCode;
        ASSERT_EQ(again.items.size(), 2u);
        for (size_t index = 0; index < again.items.size(); ++index) {
            EXPECT_EQ(again.items[index].outcome, kIngestOutcomeUnchanged);
            EXPECT_EQ(again.items[index].eventId, created.items[index].eventId);
        }
        KeyValueTable row =
            TableHelpers::Events(databaseHelper).GetEvent(transaction, created.items[0].eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusApproved);
    });
}

TEST(EventHelperTest, MinorChangeUpdatesSilently) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MinorChangeUpdatesSilently", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.cuffSite, "cuff-1", NowUs(transaction) + kUsPerDay);
        item.categorySlugs = std::vector<std::string>{ "drag" };
        int64_t eventId = IngestOne(transaction, helper, item).eventId;
        Approve(transaction, helper, eventId);

        item.title = "Retitled";
        item.costText = "Free before 10";
        item.categorySlugs = std::vector<std::string>{ "bar" };
        EXPECT_EQ(IngestOne(transaction, helper, item).outcome, kIngestOutcomeUpdated);

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusApproved);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsTitle), "Retitled");
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsCostText), "Free before 10");
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsReviewNotes), "ok");
        EXPECT_EQ(OwnCategorySlugs(transaction, databaseHelper, eventId),
                  std::vector<std::string>{ "bar" });
    });
}

TEST(EventHelperTest, DateChangeOnAnApprovedEventRevertsToPending) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DateChangeOnAnApprovedEventRevertsToPending", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);
        int64_t eventId = IngestOne(transaction, helper, item).eventId;
        Approve(transaction, helper, eventId);

        item.startsAtUs += kUsPerHour;
        EXPECT_EQ(IngestOne(transaction, helper, item).outcome, kIngestOutcomeRevertedToPending);

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusPending);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStartsAtUs), StringFromInt(item.startsAtUs));
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsReviewNotes), kReviewNoteChangedByRescan);
        // Who approved it last stays on record.
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsReviewedByPersonId),
                  StringFromInt(kReviewerPersonId));
    });
}

TEST(EventHelperTest, VenueChangeOnAnApprovedEventRevertsToPending) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("VenueChangeOnAnApprovedEventRevertsToPending", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);
        item.venueId = world.theCuff;
        int64_t eventId = IngestOne(transaction, helper, item).eventId;
        Approve(transaction, helper, eventId);

        item.venueId = world.sunsetTavern;
        EXPECT_EQ(IngestOne(transaction, helper, item).outcome, kIngestOutcomeRevertedToPending);
        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusPending);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsVenueId), StringFromInt(world.sunsetTavern));
    });
}

TEST(EventHelperTest, MajorChangeOnAPendingEventJustUpdates) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MajorChangeOnAPendingEventJustUpdates", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);
        int64_t eventId = IngestOne(transaction, helper, item).eventId;

        item.startsAtUs += kUsPerHour;
        EXPECT_EQ(IngestOne(transaction, helper, item).outcome, kIngestOutcomeUpdated);
        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusPending);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsReviewNotes), "");
    });
}

TEST(EventHelperTest, RejectedAndArchivedEventsAreNeverTouched) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RejectedAndArchivedEventsAreNeverTouched", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        TableHelpers::Events events(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;
        IngestItem rejectedItem = Item(world.feed, "feed-1", startsAtUs);
        IngestItem archivedItem = Item(world.feed, "feed-2", startsAtUs);
        int64_t rejectedId = IngestOne(transaction, helper, rejectedItem).eventId;
        int64_t archivedId = IngestOne(transaction, helper, archivedItem).eventId;
        ASSERT_TRUE(helper.ReviewEvent(transaction, rejectedId, ReviewAction::Reject,
                                       kReviewerPersonId, "spam").ok);
        events.UpdateEvent(transaction, archivedId, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusArchived) },
        });

        rejectedItem.startsAtUs += kUsPerHour;
        rejectedItem.title = "Try again";
        archivedItem.startsAtUs += kUsPerHour;
        IngestResult result = helper.IngestEvents(transaction, { rejectedItem, archivedItem });
        ASSERT_TRUE(result.ok) << result.errorCode;
        EXPECT_EQ(result.items[0].outcome, kIngestOutcomeRejectedKept);
        EXPECT_EQ(result.items[0].eventId, rejectedId);
        EXPECT_EQ(result.items[1].outcome, kIngestOutcomeArchivedKept);

        KeyValueTable rejected = events.GetEvent(transaction, rejectedId);
        EXPECT_EQ(ColumnOf(rejected, DbSchema::kEventsStatus), DbSchema::kEventsStatusRejected);
        EXPECT_EQ(ColumnOf(rejected, DbSchema::kEventsTitle), "Event feed-1");
        EXPECT_EQ(ColumnOf(rejected, DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs));
        KeyValueTable archived = events.GetEvent(transaction, archivedId);
        EXPECT_EQ(ColumnOf(archived, DbSchema::kEventsStatus), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(ColumnOf(archived, DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs));
    });
}

TEST(EventHelperTest, AbsentFieldsKeepWhatTheEventHas) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AbsentFieldsKeepWhatTheEventHas", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        TableHelpers::Events events(databaseHelper);
        IngestItem item = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);
        item.description = "Feed text";
        item.seriesSlug = "t4t";
        item.categorySlugs = std::vector<std::string>{ "drag" };
        int64_t eventId = IngestOne(transaction, helper, item).eventId;
        // A curator fixes the description by hand.
        events.UpdateEvent(transaction, eventId, {
            { std::string(DbSchema::kEventsDescription), "Curator text" },
        });

        // The next scan says nothing about description, series or categories.
        item.description.reset();
        item.seriesSlug.reset();
        item.categorySlugs.reset();
        EXPECT_EQ(IngestOne(transaction, helper, item).outcome, kIngestOutcomeUnchanged);

        KeyValueTable row = events.GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsDescription), "Curator text");
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsSeriesId), StringFromInt(world.t4t));
        EXPECT_EQ(OwnCategorySlugs(transaction, databaseHelper, eventId),
                  std::vector<std::string>{ "drag" });
    });
}

TEST(EventHelperTest, StatedEmptyValuesClear) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("StatedEmptyValuesClear", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);
        item.seriesSlug = "t4t";
        item.categorySlugs = std::vector<std::string>{ "drag", "bar" };
        int64_t eventId = IngestOne(transaction, helper, item).eventId;

        // An empty list clears the event's own set (so the series' applies
        // again); an empty series slug detaches it.
        item.categorySlugs = std::vector<std::string>{};
        item.seriesSlug = "";
        EXPECT_EQ(IngestOne(transaction, helper, item).outcome, kIngestOutcomeUpdated);

        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsSeriesId), "");
        EXPECT_TRUE(OwnCategorySlugs(transaction, databaseHelper, eventId).empty());
    });
}

TEST(EventHelperTest, UnknownAndDisabledSlugsAreIgnoredAndReported) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownAndDisabledSlugsAreIgnoredAndReported", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);
        item.categorySlugs = std::vector<std::string>{ "drag", "no-such-category", "retired" };
        item.sceneTagSlugs = std::vector<std::string>{ "bears", "no-such-scene" };
        item.seriesSlug = "no-such-series";

        IngestItemResult result = IngestOne(transaction, helper, item);
        EXPECT_EQ(result.outcome, kIngestOutcomeCreated);
        EXPECT_EQ(result.ignoredCategorySlugs,
                  (std::vector<std::string>{ "no-such-category", "retired" }));
        EXPECT_EQ(result.ignoredSceneTagSlugs, std::vector<std::string>{ "no-such-scene" });
        EXPECT_EQ(result.ignoredSeriesSlug, "no-such-series");

        KeyValueTable row =
            TableHelpers::Events(databaseHelper).GetEvent(transaction, result.eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsSeriesId), "");
        EXPECT_EQ(OwnCategorySlugs(transaction, databaseHelper, result.eventId),
                  std::vector<std::string>{ "drag" });
    });
}

TEST(EventHelperTest, OneBadItemFailsTheBatchBeforeAnyWrite) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OneBadItemFailsTheBatchBeforeAnyWrite", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;
        IngestItem good = Item(world.feed, "feed-1", startsAtUs);
        IngestItem bad = Item(world.feed, "feed-2", startsAtUs);
        bad.title = "";

        IngestResult result = helper.IngestEvents(transaction, { good, bad });
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.errorCode, kEventErrorMissingTitle);
        EXPECT_EQ(result.errorItemIndex, 1);
        EXPECT_TRUE(result.items.empty());
        EXPECT_TRUE(TableHelpers::Events(databaseHelper)
                        .GetEventBySourceAndExternalKey(transaction, world.feed, "feed-1")
                        .empty());
    });
}

TEST(EventHelperTest, IngestValidationErrorCodes) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("IngestValidationErrorCodes", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        const IngestItem valid = Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay);

        struct Case {
            std::function<void(IngestItem&)> breakIt;
            std::string_view errorCode;
        };
        const std::vector<Case> cases = {
            { [](IngestItem& item) { item.sourceId = 999999; }, kEventErrorSourceNotFound },
            { [](IngestItem& item) { item.sourceId = 0; }, kEventErrorSourceNotFound },
            { [](IngestItem& item) { item.externalKey = ""; }, kEventErrorMissingExternalKey },
            { [](IngestItem& item) { item.title = ""; }, kEventErrorMissingTitle },
            { [](IngestItem& item) { item.startsAtUs = 0; }, kEventErrorMissingStart },
            { [](IngestItem& item) { item.endsAtUs = item.startsAtUs - 1; },
              kEventErrorEndBeforeStart },
            { [](IngestItem& item) { item.venueId = 999999; }, kEventErrorVenueNotFound },
        };
        for (const Case& testCase : cases) {
            IngestItem item = valid;
            testCase.breakIt(item);
            IngestResult result = helper.IngestEvents(transaction, { item });
            EXPECT_FALSE(result.ok) << testCase.errorCode;
            EXPECT_EQ(result.errorCode, testCase.errorCode);
            EXPECT_EQ(result.errorItemIndex, 0);
        }
        // An end equal to the start is fine (a zero-length listing).
        IngestItem instant = valid;
        instant.endsAtUs = instant.startsAtUs;
        EXPECT_TRUE(helper.IngestEvents(transaction, { instant }).ok);
    });
}

// ---- Review ----

TEST(EventHelperTest, ReviewTransitions) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ReviewTransitions", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t nowUs = NowUs(transaction);
        int64_t eventId =
            IngestOne(transaction, helper, Item(world.feed, "feed-1", nowUs + kUsPerDay)).eventId;

        ReviewResult approved = helper.ReviewEvent(
            transaction, eventId, ReviewAction::Approve, kReviewerPersonId, "Looks right");
        EXPECT_TRUE(approved.ok);
        EXPECT_EQ(approved.status, DbSchema::kEventsStatusApproved);
        KeyValueTable row = TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusApproved);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsReviewedByPersonId),
                  StringFromInt(kReviewerPersonId));
        // now_us() is the wall clock (clock_timestamp()), so the stamp is at or
        // after the time read at the top of the test, not equal to it.
        EXPECT_GE(std::stoll(ColumnOf(row, DbSchema::kEventsReviewedUs)), nowUs);
        EXPECT_EQ(ColumnOf(row, DbSchema::kEventsReviewNotes), "Looks right");

        // Re-applying the current status is a no-op the caller should hear about.
        ReviewResult again = helper.ReviewEvent(
            transaction, eventId, ReviewAction::Approve, kReviewerPersonId, "");
        EXPECT_FALSE(again.ok);
        EXPECT_EQ(again.errorCode, kEventErrorInvalidTransition);
        EXPECT_EQ(again.status, DbSchema::kEventsStatusApproved);

        // approved ↔ rejected are corrections.
        ReviewResult rejected = helper.ReviewEvent(
            transaction, eventId, ReviewAction::Reject, kReviewerPersonId, "Duplicate");
        EXPECT_TRUE(rejected.ok);
        EXPECT_EQ(rejected.status, DbSchema::kEventsStatusRejected);
        EXPECT_EQ(helper.ReviewEvent(transaction, eventId, ReviewAction::Reject,
                                     kReviewerPersonId, "").errorCode,
                  kEventErrorInvalidTransition);
        EXPECT_TRUE(helper.ReviewEvent(transaction, eventId, ReviewAction::Approve,
                                       kReviewerPersonId, "Not a duplicate").ok);

        // pending → rejected directly.
        int64_t otherId =
            IngestOne(transaction, helper, Item(world.feed, "feed-2", nowUs + kUsPerDay)).eventId;
        EXPECT_TRUE(helper.ReviewEvent(transaction, otherId, ReviewAction::Reject,
                                       kReviewerPersonId, "").ok);
    });
}

TEST(EventHelperTest, ReviewRefusesArchivedAndUnknownEvents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ReviewRefusesArchivedAndUnknownEvents", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t eventId = IngestOne(
            transaction, helper, Item(world.feed, "feed-1", NowUs(transaction) + kUsPerDay)).eventId;
        TableHelpers::Events(databaseHelper).UpdateEvent(transaction, eventId, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusArchived) },
        });

        for (ReviewAction action : { ReviewAction::Approve, ReviewAction::Reject }) {
            ReviewResult result =
                helper.ReviewEvent(transaction, eventId, action, kReviewerPersonId, "");
            EXPECT_FALSE(result.ok);
            EXPECT_EQ(result.errorCode, kEventErrorInvalidTransition);
            EXPECT_EQ(result.status, DbSchema::kEventsStatusArchived);
        }
        ReviewResult missing = helper.ReviewEvent(
            transaction, 999999, ReviewAction::Approve, kReviewerPersonId, "");
        EXPECT_FALSE(missing.ok);
        EXPECT_EQ(missing.errorCode, kEventErrorNotFound);
    });
}

// ---- The public list ----

TEST(EventHelperTest, OnlyApprovedEventsAreEverPublic) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OnlyApprovedEventsAreEverPublic", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;
        int64_t approved = IngestOne(transaction, helper, Item(world.feed, "approved", startsAtUs)).eventId;
        IngestOne(transaction, helper, Item(world.feed, "pending", startsAtUs));
        int64_t rejected = IngestOne(transaction, helper, Item(world.feed, "rejected", startsAtUs)).eventId;
        int64_t archived = IngestOne(transaction, helper, Item(world.feed, "archived", startsAtUs)).eventId;
        Approve(transaction, helper, approved);
        Approve(transaction, helper, archived);
        helper.ReviewEvent(transaction, rejected, ReviewAction::Reject, kReviewerPersonId, "");
        TableHelpers::Events(databaseHelper).UpdateEvent(transaction, archived, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusArchived) },
        });

        EXPECT_EQ(IdsOf(helper.GetUpcomingPublic(transaction, UpcomingQuery())),
                  std::vector<int64_t>{ approved });
    });
}

TEST(EventHelperTest, TheWindowDefaultsTo42DaysAndCapsAt92) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheWindowDefaultsTo42DaysAndCapsAt92", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t nowUs = NowUs(transaction);
        auto addApproved = [&](std::string_view key, int64_t startsAtUs) {
            int64_t id = IngestOne(transaction, helper, Item(world.feed, key, startsAtUs)).eventId;
            Approve(transaction, helper, id);
            return id;
        };
        addApproved("last-week", nowUs - 7 * kUsPerDay);  // before the one-day look-back
        int64_t tonight = addApproved("tonight", nowUs + 2 * kUsPerHour);
        int64_t inTwoMonths = addApproved("in-two-months", nowUs + 60 * kUsPerDay);
        addApproved("in-four-months", nowUs + 120 * kUsPerDay);  // past the 92-day cap

        EXPECT_EQ(IdsOf(helper.GetUpcomingPublic(transaction, UpcomingQuery())),
                  std::vector<int64_t>{ tonight });

        UpcomingQuery asMuchAsAllowed;
        asMuchAsAllowed.fromUs = nowUs - 30 * kUsPerDay;  // clamped to now - 1 day
        asMuchAsAllowed.toUs = nowUs + 365 * kUsPerDay;   // clamped to from + 92 days
        UpcomingEvents upcoming = helper.GetUpcomingPublic(transaction, asMuchAsAllowed);
        EXPECT_EQ(IdsOf(upcoming), (std::vector<int64_t>{ tonight, inTwoMonths }));
        // The window served comes back, clamped, for the caller to page from.
        EXPECT_GE(upcoming.window.fromUs, nowUs - kUpcomingLookBackDays * kUsPerDay);
        EXPECT_EQ(upcoming.window.toUs - upcoming.window.fromUs,
                  kUpcomingMaxWindowDays * kUsPerDay);
    });
}

TEST(EventHelperTest, UpcomingResolvesVenueNeighborhoodAndSeriesInStartOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UpcomingResolvesVenueNeighborhoodAndSeriesInStartOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t nowUs = NowUs(transaction);

        IngestItem later = Item(world.cuffSite, "cuff-1", nowUs + 2 * kUsPerDay);
        later.endsAtUs = later.startsAtUs + 3 * kUsPerHour;
        later.description = "Second Saturdays.";
        later.url = "https://example.org/t4t";
        later.costText = "$10";
        later.locationText = "Back patio";
        later.seriesSlug = "t4t";
        IngestItem sooner = Item(world.feed, "feed-1", nowUs + kUsPerDay);
        sooner.venueId = world.sunsetTavern;
        IngestItem nowhere = Item(world.feed, "feed-2", nowUs + 3 * kUsPerDay);
        IngestResult ingested = helper.IngestEvents(transaction, { later, sooner, nowhere });
        ASSERT_TRUE(ingested.ok) << ingested.errorCode;
        for (const IngestItemResult& item : ingested.items) {
            Approve(transaction, helper, item.eventId);
        }

        std::vector<PublicEvent> events =
            helper.GetUpcomingPublic(transaction, UpcomingQuery()).events;
        ASSERT_EQ(events.size(), 3u);
        EXPECT_EQ(IdsOf(events), (std::vector<int64_t>{ ingested.items[1].eventId,
                                                        ingested.items[0].eventId,
                                                        ingested.items[2].eventId }));

        const PublicEvent& t4t = events[1];
        EXPECT_EQ(t4t.title, "Event cuff-1");
        EXPECT_EQ(t4t.description, "Second Saturdays.");
        EXPECT_EQ(t4t.startsAtUs, later.startsAtUs);
        EXPECT_EQ(t4t.endsAtUs, later.endsAtUs);
        EXPECT_EQ(t4t.venueId, world.theCuff);
        EXPECT_EQ(t4t.venueName, "The Cuff");
        EXPECT_EQ(t4t.neighborhoodSlug, "capitol-hill");
        EXPECT_EQ(t4t.locationText, "Back patio");
        EXPECT_EQ(t4t.url, "https://example.org/t4t");
        EXPECT_EQ(t4t.costText, "$10");
        EXPECT_EQ(t4t.seriesId, world.t4t);
        EXPECT_EQ(t4t.seriesSlug, "t4t");
        EXPECT_EQ(t4t.seriesTitle, "T4T");
        EXPECT_EQ(t4t.origin, DbSchema::kEventsOriginScanned);

        EXPECT_EQ(events[0].neighborhoodSlug, "ballard");
        EXPECT_FALSE(events[0].seriesId.has_value());
        EXPECT_FALSE(events[2].venueId.has_value());
        EXPECT_EQ(events[2].venueName, "");
        EXPECT_FALSE(events[2].endsAtUs.has_value());
    });
}

TEST(EventHelperTest, UpcomingFilters) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UpcomingFilters", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;

        IngestItem atTheCuff = Item(world.cuffSite, "cuff-1", startsAtUs);
        atTheCuff.categorySlugs = std::vector<std::string>{ "drag" };
        atTheCuff.sceneTagSlugs = std::vector<std::string>{ "bears" };
        IngestItem atSunset = Item(world.feed, "feed-1", startsAtUs + kUsPerHour);
        atSunset.venueId = world.sunsetTavern;
        atSunset.categorySlugs = std::vector<std::string>{ "bar" };
        IngestResult ingested = helper.IngestEvents(transaction, { atTheCuff, atSunset });
        ASSERT_TRUE(ingested.ok) << ingested.errorCode;
        int64_t cuffId = ingested.items[0].eventId;
        int64_t sunsetId = ingested.items[1].eventId;
        Approve(transaction, helper, cuffId);
        Approve(transaction, helper, sunsetId);

        auto idsFor = [&](std::function<void(UpcomingQuery&)> filter) {
            UpcomingQuery query;
            filter(query);
            return IdsOf(helper.GetUpcomingPublic(transaction, query));
        };
        using Ids = std::vector<int64_t>;
        EXPECT_EQ(idsFor([](UpcomingQuery&) {}), (Ids{ cuffId, sunsetId }));
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.categorySlug = "drag"; }), Ids{ cuffId });
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.categorySlug = "bar"; }), Ids{ sunsetId });
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.sceneTagSlug = "bears"; }), Ids{ cuffId });
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.neighborhoodSlug = "ballard"; }), Ids{ sunsetId });
        EXPECT_EQ(idsFor([&](UpcomingQuery& q) { q.venueId = world.theCuff; }), Ids{ cuffId });
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.categorySlug = "no-such-category"; }), Ids{});
        EXPECT_EQ(idsFor([](UpcomingQuery& q) {
                      q.categorySlug = "drag";
                      q.neighborhoodSlug = "ballard";
                  }),
                  Ids{});
    });
}

// The OQ5 inheritance pin: an instance with no assignments of its own matches
// (and carries) its series'; own categories override, per axis, while scene
// tags still inherit.
TEST(EventHelperTest, InstancesInheritTheirSeriesAssignmentsPerAxis) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("InstancesInheritTheirSeriesAssignmentsPerAxis", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;

        IngestItem plain = Item(world.cuffSite, "t4t-plain", startsAtUs);
        plain.seriesSlug = "t4t";
        IngestItem dragNight = Item(world.cuffSite, "t4t-drag", startsAtUs + kUsPerHour);
        dragNight.seriesSlug = "t4t";
        dragNight.categorySlugs = std::vector<std::string>{ "drag" };
        IngestItem unrelated = Item(world.feed, "feed-1", startsAtUs + 2 * kUsPerHour);
        IngestResult ingested = helper.IngestEvents(transaction, { plain, dragNight, unrelated });
        ASSERT_TRUE(ingested.ok) << ingested.errorCode;
        int64_t plainId = ingested.items[0].eventId;
        int64_t dragId = ingested.items[1].eventId;
        int64_t unrelatedId = ingested.items[2].eventId;
        for (const IngestItemResult& item : ingested.items) {
            Approve(transaction, helper, item.eventId);
        }

        std::vector<PublicEvent> events =
            helper.GetUpcomingPublic(transaction, UpcomingQuery()).events;
        const PublicEvent* plainEvent = FindEvent(events, plainId);
        const PublicEvent* dragEvent = FindEvent(events, dragId);
        ASSERT_NE(plainEvent, nullptr);
        ASSERT_NE(dragEvent, nullptr);
        EXPECT_EQ(plainEvent->categorySlugs, std::vector<std::string>{ "dance" });
        EXPECT_EQ(plainEvent->sceneTagSlugs, std::vector<std::string>{ "trans" });
        EXPECT_EQ(dragEvent->categorySlugs, std::vector<std::string>{ "drag" });
        EXPECT_EQ(dragEvent->sceneTagSlugs, std::vector<std::string>{ "trans" });

        using Ids = std::vector<int64_t>;
        auto idsFor = [&](std::function<void(UpcomingQuery&)> filter) {
            UpcomingQuery query;
            filter(query);
            return IdsOf(helper.GetUpcomingPublic(transaction, query));
        };
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.categorySlug = "dance"; }), Ids{ plainId });
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.categorySlug = "drag"; }), Ids{ dragId });
        EXPECT_EQ(idsFor([](UpcomingQuery& q) { q.sceneTagSlug = "trans"; }),
                  (Ids{ plainId, dragId }));
        EXPECT_EQ(idsFor([&](UpcomingQuery& q) { q.seriesId = world.t4t; }),
                  (Ids{ plainId, dragId }));
        EXPECT_EQ(idsFor([](UpcomingQuery&) {}), (Ids{ plainId, dragId, unrelatedId }));
    });
}

// ---- Detail pages ----

TEST(EventHelperTest, EventDetailReturnsAnyStatusWithItsReviewFields) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EventDetailReturnsAnyStatusWithItsReviewFields", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        IngestItem item = Item(world.cuffSite, "cuff-1", NowUs(transaction) + kUsPerDay);
        item.seriesSlug = "t4t";
        int64_t eventId = IngestOne(transaction, helper, item).eventId;

        std::optional<EventDetail> pending = helper.GetEventDetail(transaction, eventId);
        ASSERT_TRUE(pending.has_value());
        EXPECT_EQ(pending->status, DbSchema::kEventsStatusPending);
        EXPECT_EQ(pending->reviewNotes, "");
        // The same resolved public shape the list carries.
        EXPECT_EQ(pending->event.id, eventId);
        EXPECT_EQ(pending->event.venueName, "The Cuff");
        EXPECT_EQ(pending->event.neighborhoodSlug, "capitol-hill");
        EXPECT_EQ(pending->event.seriesSlug, "t4t");
        EXPECT_EQ(pending->event.categorySlugs, std::vector<std::string>{ "dance" });

        ASSERT_TRUE(helper.ReviewEvent(transaction, eventId, ReviewAction::Reject,
                                       kReviewerPersonId, "Duplicate").ok);
        std::optional<EventDetail> rejected = helper.GetEventDetail(transaction, eventId);
        ASSERT_TRUE(rejected.has_value());
        EXPECT_EQ(rejected->status, DbSchema::kEventsStatusRejected);
        EXPECT_EQ(rejected->reviewNotes, "Duplicate");

        EXPECT_FALSE(helper.GetEventDetail(transaction, 999999).has_value());
    });
}

TEST(EventHelperTest, ActiveSeriesCarryTheirOwnSlugsAndPlaceInTitleOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ActiveSeriesCarryTheirOwnSlugsAndPlaceInTitleOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        TableHelpers::Series series(databaseHelper);
        int64_t bearHappyHour = series.AddSeries(transaction, {
            { std::string(DbSchema::kSeriesTitle), "Bear Happy Hour" },
            { std::string(DbSchema::kSeriesSlug), "bear-happy-hour" },
            { std::string(DbSchema::kSeriesCadenceText), "Fridays" },
            { std::string(DbSchema::kSeriesVenueId), StringFromInt(world.sunsetTavern) },
            { std::string(DbSchema::kSeriesStatus), std::string(DbSchema::kFreshnessStatusSeasonal) },
        });
        series.AddSeries(transaction, {
            { std::string(DbSchema::kSeriesTitle), "Closed Night" },
            { std::string(DbSchema::kSeriesSlug), "closed-night" },
            { std::string(DbSchema::kSeriesStatus), std::string(DbSchema::kFreshnessStatusClosed) },
        });

        std::vector<PublicSeries> active = helper.GetActiveSeries(transaction);
        ASSERT_EQ(active.size(), 2u);
        const PublicSeries& bears = active[0];  // "Bear…" sorts before "T4T"
        EXPECT_EQ(bears.id, bearHappyHour);
        EXPECT_EQ(bears.slug, "bear-happy-hour");
        EXPECT_EQ(bears.cadenceText, "Fridays");
        EXPECT_EQ(bears.venueId, world.sunsetTavern);
        EXPECT_EQ(bears.venueName, "Sunset Tavern");
        EXPECT_EQ(bears.neighborhoodSlug, "ballard");
        EXPECT_EQ(bears.status, DbSchema::kFreshnessStatusSeasonal);
        EXPECT_TRUE(bears.categorySlugs.empty());

        const PublicSeries& t4t = active[1];
        EXPECT_EQ(t4t.id, world.t4t);
        EXPECT_EQ(t4t.categorySlugs, std::vector<std::string>{ "dance" });
        EXPECT_EQ(t4t.sceneTagSlugs, std::vector<std::string>{ "trans" });
        EXPECT_FALSE(t4t.venueId.has_value());
        EXPECT_EQ(t4t.venueName, "");
    });
}

TEST(EventHelperTest, SeriesDetailCarriesOnlyItsApprovedUpcomingInstances) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SeriesDetailCarriesOnlyItsApprovedUpcomingInstances", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, databaseHelper);
        EventHelper helper(databaseHelper);
        int64_t startsAtUs = NowUs(transaction) + kUsPerDay;
        IngestItem approved = Item(world.cuffSite, "t4t-1", startsAtUs);
        approved.seriesSlug = "t4t";
        IngestItem pending = Item(world.cuffSite, "t4t-2", startsAtUs + kUsPerHour);
        pending.seriesSlug = "t4t";
        IngestItem elsewhere = Item(world.feed, "feed-1", startsAtUs);
        IngestResult ingested = helper.IngestEvents(transaction, { approved, pending, elsewhere });
        ASSERT_TRUE(ingested.ok) << ingested.errorCode;
        Approve(transaction, helper, ingested.items[0].eventId);
        Approve(transaction, helper, ingested.items[2].eventId);

        std::optional<SeriesDetail> detail = helper.GetSeriesDetail(transaction, "t4t");
        ASSERT_TRUE(detail.has_value());
        EXPECT_EQ(detail->series.id, world.t4t);
        EXPECT_EQ(detail->series.title, "T4T");
        EXPECT_EQ(IdsOf(detail->upcoming), std::vector<int64_t>{ ingested.items[0].eventId });

        // A closed series keeps its page.
        TableHelpers::Series(databaseHelper).AddSeries(transaction, {
            { std::string(DbSchema::kSeriesTitle), "Closed Night" },
            { std::string(DbSchema::kSeriesSlug), "closed-night" },
            { std::string(DbSchema::kSeriesStatus), std::string(DbSchema::kFreshnessStatusClosed) },
        });
        std::optional<SeriesDetail> closed = helper.GetSeriesDetail(transaction, "closed-night");
        ASSERT_TRUE(closed.has_value());
        EXPECT_EQ(closed->series.status, DbSchema::kFreshnessStatusClosed);
        EXPECT_TRUE(closed->upcoming.events.empty());

        EXPECT_FALSE(helper.GetSeriesDetail(transaction, "no-such-series").has_value());
    });
}

}  // namespace
}  // namespace Events
