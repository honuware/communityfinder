#include "events.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/event_sources.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "event_sources.h"
#include "event_submissions.h"
#include "series.h"
#include "sql_util/database_access/database_crud_helpers.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeEvent(std::string_view title, int64_t startsAtUs) {
    return {
        { std::string(DbSchema::kEventsTitle), std::string(title) },
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs) },
    };
}

KeyValueTable MakeIngestedEvent(std::string_view title, int64_t startsAtUs,
                                int64_t sourceId, std::string_view externalKey) {
    KeyValueTable values = MakeEvent(title, startsAtUs);
    values[std::string(DbSchema::kEventsSourceId)] = StringFromInt(sourceId);
    values[std::string(DbSchema::kEventsExternalKey)] = std::string(externalKey);
    values[std::string(DbSchema::kEventsOrigin)] = std::string(DbSchema::kEventsOriginScanned);
    return values;
}

KeyValueTable WithStatus(KeyValueTable values, std::string_view status) {
    values[std::string(DbSchema::kEventsStatus)] = std::string(status);
    return values;
}

int64_t AddSource(Transaction& transaction, DatabaseHelper databaseHelper,
                  std::string_view name) {
    EventSources sources(databaseHelper);
    return sources.AddSource(transaction, {
        { std::string(DbSchema::kEventSourcesName), std::string(name) },
        { std::string(DbSchema::kEventSourcesKind),
          std::string(DbSchema::kEventSourcesKindIcsFeed) },
    });
}

std::string IdOf(const KeyValueTable& row) {
    return row.at(std::string(DbSchema::kEventsId));
}

int64_t AddSubmission(Transaction& transaction, DatabaseHelper databaseHelper,
                      int64_t personId) {
    EventSubmissions submissions(databaseHelper);
    return submissions.AddSubmission(transaction, {
        { std::string(DbSchema::kEventSubmissionsSubmittedByPersonId),
          StringFromInt(personId) },
    });
}

// A date as EV3's Submit writes it: pending, user_submitted, on its envelope.
KeyValueTable MakeSubmittedEvent(std::string_view title, int64_t startsAtUs,
                                 int64_t submissionId) {
    KeyValueTable values = MakeEvent(title, startsAtUs);
    values[std::string(DbSchema::kEventsSubmissionId)] = StringFromInt(submissionId);
    values[std::string(DbSchema::kEventsOrigin)] =
        std::string(DbSchema::kEventsOriginUserSubmitted);
    return values;
}

TEST(EventsTest, AddAndGetWithTheDefaults) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetWithTheDefaults", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeEvent("Bear Happy Hour", 1'800'000'000'000'000);
        values[std::string(DbSchema::kEventsCostText)] = "Free";
        int64_t id = events.AddEvent(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = events.GetEvent(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsTitle)), "Bear Happy Hour");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStartsAtUs)), "1800000000000000");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsCostText)), "Free");
        // Hand-entered by default, and waiting for review.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), DbSchema::kEventsStatusPending);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsOrigin)), DbSchema::kEventsOriginManual);
        // Nullable columns left out read back as "".
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsEndsAtUs)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsSourceId)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsExternalKey)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsReviewedByPersonId)).empty());
    });
}

TEST(EventsTest, GetEventNotFoundIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetEventNotFoundIsEmpty", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        EXPECT_TRUE(events.GetEvent(transaction, 999999).empty());
    });
}

TEST(EventsTest, GetEventForUpdateReadsTheRowOrNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetEventForUpdateReadsTheRowOrNothing", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        int64_t id = events.AddEvent(transaction, MakeEvent("Locked", 1000));

        KeyValueTable row = events.GetEventForUpdate(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(IdOf(row), StringFromInt(id));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsTitle)), "Locked");
        EXPECT_TRUE(events.GetEventForUpdate(transaction, 999999).empty());
    });
}

TEST(EventsTest, GetEventBySourceAndExternalKeyMatchesBothParts) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventBySourceAndExternalKeyMatchesBothParts", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t kremwerk = AddSource(transaction, databaseHelper, "Kremwerk");
        int64_t wildrose = AddSource(transaction, databaseHelper, "Wildrose");
        int64_t id = events.AddEvent(
            transaction, MakeIngestedEvent("Night A", 1000, kremwerk, "uid-1"));
        // Same key from another source is a different item.
        int64_t otherSource = events.AddEvent(
            transaction, MakeIngestedEvent("Night B", 1000, wildrose, "uid-1"));

        KeyValueTable match = events.GetEventBySourceAndExternalKey(transaction, kremwerk, "uid-1");
        ASSERT_FALSE(match.empty());
        EXPECT_EQ(IdOf(match), StringFromInt(id));
        EXPECT_EQ(IdOf(events.GetEventBySourceAndExternalKey(transaction, wildrose, "uid-1")),
                  StringFromInt(otherSource));

        EXPECT_TRUE(events.GetEventBySourceAndExternalKey(transaction, kremwerk, "uid-2").empty());
    });
}

// The public calendar's read (EV4: approved and archived; it was approved-only
// GetEventsInRange before).
TEST(EventsTest, GetEventsStartingBetweenOrdersByStartThenId) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsStartingBetweenOrdersByStartThenId", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string approved(DbSchema::kEventsStatusApproved);
        int64_t atStart = events.AddEvent(transaction, WithStatus(MakeEvent("At from", 1000), approved));
        int64_t laterB = events.AddEvent(transaction, WithStatus(MakeEvent("Later B", 1500), approved));
        int64_t laterA = events.AddEvent(
            transaction, WithStatus(MakeEvent("Later A", 1200), DbSchema::kEventsStatusArchived));
        // Same start as laterB: id breaks the tie.
        int64_t tied = events.AddEvent(transaction, WithStatus(MakeEvent("Tied", 1500), approved));
        events.AddEvent(transaction, WithStatus(MakeEvent("Before", 999), approved));
        events.AddEvent(transaction, WithStatus(MakeEvent("At to", 2000), approved));
        events.AddEvent(transaction, MakeEvent("Pending inside", 1300));
        events.AddEvent(
            transaction, WithStatus(MakeEvent("Rejected inside", 1300), DbSchema::kEventsStatusRejected));

        KeyValueTableArray rows = events.GetEventsStartingBetween(
            transaction, 1000, 2000,
            { DbSchema::kEventsStatusApproved, DbSchema::kEventsStatusArchived });
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(atStart));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(laterA));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(laterB));
        EXPECT_EQ(IdOf(rows[3]), StringFromInt(tied));
    });
}

TEST(EventsTest, GetEventsBySeriesReturnsEveryStatusInStartOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsBySeriesReturnsEveryStatusInStartOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        Series series(databaseHelper);
        auto addSeries = [&](std::string_view slug) {
            return series.AddSeries(transaction, {
                { std::string(DbSchema::kSeriesTitle), std::string(slug) },
                { std::string(DbSchema::kSeriesSlug), std::string(slug) },
            });
        };
        int64_t t4t = addSeries("t4t");
        int64_t other = addSeries("other");
        auto addIn = [&](std::string_view title, int64_t startsAtUs, int64_t seriesId,
                         std::string_view status) {
            KeyValueTable values = WithStatus(MakeEvent(title, startsAtUs), status);
            values[std::string(DbSchema::kEventsSeriesId)] = StringFromInt(seriesId);
            return events.AddEvent(transaction, values);
        };
        int64_t march = addIn("March", 3000, t4t, DbSchema::kEventsStatusApproved);
        int64_t january = addIn("January", 1000, t4t, DbSchema::kEventsStatusArchived);
        int64_t february = addIn("February", 2000, t4t, DbSchema::kEventsStatusPending);
        addIn("Not this series", 1500, other, DbSchema::kEventsStatusApproved);
        events.AddEvent(transaction, MakeEvent("One-off", 1500));

        KeyValueTableArray rows = events.GetEventsBySeries(transaction, t4t);
        ASSERT_EQ(rows.size(), 3u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(january));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(february));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(march));
    });
}

TEST(EventsTest, GetPendingEventsIsTheReviewQueueSoonestFirst) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetPendingEventsIsTheReviewQueueSoonestFirst", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        int64_t later = events.AddEvent(transaction, MakeEvent("Later", 2000));
        int64_t sooner = events.AddEvent(transaction, MakeEvent("Sooner", 1000));
        events.AddEvent(transaction,
                        WithStatus(MakeEvent("Approved", 500), DbSchema::kEventsStatusApproved));
        events.AddEvent(transaction,
                        WithStatus(MakeEvent("Rejected", 500), DbSchema::kEventsStatusRejected));

        KeyValueTableArray rows = events.GetPendingEvents(transaction);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(sooner));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(later));
    });
}

TEST(EventsTest, UpdateBumpsUpdatedUsAndCanClearAColumn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearAColumn", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeEvent("Bear Happy Hour", 1000);
        values[std::string(DbSchema::kEventsEndsAtUs)] = "2000";
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kEventsUpdatedUs)] = "0";
        int64_t id = events.AddEvent(transaction, values);

        // The review quartet, as EventHelper::ReviewEvent will stamp it.
        events.UpdateEvent(transaction, id, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusApproved) },
            { std::string(DbSchema::kEventsReviewedByPersonId), "42" },
            { std::string(DbSchema::kEventsReviewedUs), "1500" },
            { std::string(DbSchema::kEventsReviewNotes), "Checked the venue page" },
            { std::string(DbSchema::kEventsEndsAtUs), "NULL" },
        });

        KeyValueTable row = events.GetEvent(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsStatus)), DbSchema::kEventsStatusApproved);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedByPersonId)), "42");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewedUs)), "1500");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventsReviewNotes)), "Checked the venue page");
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventsEndsAtUs)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventsUpdatedUs))), 0);
    });
}

// --- EV3: submitted events ---------------------------------------------------

TEST(EventsTest, GetEventsBySubmissionReturnsEveryStatusInStartOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsBySubmissionReturnsEveryStatusInStartOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t submission = AddSubmission(transaction, databaseHelper, 42);
        int64_t other = AddSubmission(transaction, databaseHelper, 42);
        int64_t third = events.AddEvent(transaction, MakeSubmittedEvent("Third", 3000, submission));
        int64_t first = events.AddEvent(
            transaction,
            WithStatus(MakeSubmittedEvent("First", 1000, submission),
                       DbSchema::kEventsStatusApproved));
        int64_t second = events.AddEvent(
            transaction,
            WithStatus(MakeSubmittedEvent("Second", 2000, submission),
                       DbSchema::kEventsStatusRejected));
        events.AddEvent(transaction, MakeSubmittedEvent("Another submission", 1500, other));
        events.AddEvent(transaction, MakeEvent("Not submitted", 1500));

        KeyValueTableArray rows = events.GetEventsBySubmission(transaction, submission);
        ASSERT_EQ(rows.size(), 3u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(first));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(second));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(third));
    });
}

TEST(EventsTest, GetEventsBySubmissionsGroupsBySubmissionThenStart) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsBySubmissionsGroupsBySubmissionThenStart", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t a = AddSubmission(transaction, databaseHelper, 42);
        int64_t b = AddSubmission(transaction, databaseHelper, 42);
        int64_t skipped = AddSubmission(transaction, databaseHelper, 42);
        int64_t bLate = events.AddEvent(transaction, MakeSubmittedEvent("B late", 5000, b));
        int64_t aLate = events.AddEvent(transaction, MakeSubmittedEvent("A late", 4000, a));
        int64_t bEarly = events.AddEvent(transaction, MakeSubmittedEvent("B early", 1000, b));
        int64_t aEarly = events.AddEvent(transaction, MakeSubmittedEvent("A early", 2000, a));
        events.AddEvent(transaction, MakeSubmittedEvent("Not asked for", 1000, skipped));

        KeyValueTableArray rows = events.GetEventsBySubmissions(transaction, {b, a, 999999});
        ASSERT_EQ(rows.size(), 4u);
        // Ordered by submission id, so a's dates come first whatever the input order.
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(aEarly));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(aLate));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(bEarly));
        EXPECT_EQ(IdOf(rows[3]), StringFromInt(bLate));

        EXPECT_TRUE(events.GetEventsBySubmissions(transaction, {}).empty());
    });
}

// The pending-dates rate limit counts only the person's own submitted dates
// that are still waiting: not decided ones, not someone else's, and not events
// they entered some other way.
TEST(EventsTest, CountPendingSubmittedByCountsOnlyTheirWaitingSubmittedDates) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "CountPendingSubmittedByCountsOnlyTheirWaitingSubmittedDates",
        [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t first = AddSubmission(transaction, databaseHelper, 42);
        int64_t second = AddSubmission(transaction, databaseHelper, 42);
        int64_t someoneElse = AddSubmission(transaction, databaseHelper, 43);
        events.AddEvent(transaction, MakeSubmittedEvent("Waiting 1", 1000, first));
        events.AddEvent(transaction, MakeSubmittedEvent("Waiting 2", 2000, first));
        events.AddEvent(transaction, MakeSubmittedEvent("Waiting 3", 3000, second));
        events.AddEvent(transaction,
                        WithStatus(MakeSubmittedEvent("Listed", 4000, second),
                                   DbSchema::kEventsStatusApproved));
        events.AddEvent(transaction,
                        WithStatus(MakeSubmittedEvent("Not listed", 5000, second),
                                   DbSchema::kEventsStatusRejected));
        events.AddEvent(transaction, MakeSubmittedEvent("Theirs", 1000, someoneElse));
        // Entered by person 42 in the admin editor: pending, but not a submission.
        KeyValueTable handEntered = MakeEvent("Hand-entered", 1000);
        handEntered[std::string(DbSchema::kEventsCreatedByPersonId)] = "42";
        events.AddEvent(transaction, handEntered);

        EXPECT_EQ(events.CountPendingSubmittedBy(transaction, 42), 3);
        EXPECT_EQ(events.CountPendingSubmittedBy(transaction, 43), 1);
        EXPECT_EQ(events.CountPendingSubmittedBy(transaction, 44), 0);
    });
}

TEST(EventsTest, GetEventsStartingBetweenIsHalfOpenAcrossTheGivenStatuses) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetEventsStartingBetweenIsHalfOpenAcrossTheGivenStatuses",
        [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        int64_t pending = events.AddEvent(transaction, MakeEvent("Pending", 1500));
        int64_t approved = events.AddEvent(
            transaction, WithStatus(MakeEvent("Approved", 1000), DbSchema::kEventsStatusApproved));
        events.AddEvent(
            transaction, WithStatus(MakeEvent("Rejected", 1200), DbSchema::kEventsStatusRejected));
        events.AddEvent(
            transaction, WithStatus(MakeEvent("Archived", 1300), DbSchema::kEventsStatusArchived));
        events.AddEvent(transaction, MakeEvent("Before", 999));
        events.AddEvent(transaction, MakeEvent("At to", 2000));

        const std::vector<std::string_view> statuses = {
            DbSchema::kEventsStatusPending, DbSchema::kEventsStatusApproved };
        KeyValueTableArray rows = events.GetEventsStartingBetween(transaction, 1000, 2000, statuses);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(approved));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(pending));

        EXPECT_TRUE(events.GetEventsStartingBetween(transaction, 1000, 2000, {}).empty());
        // A status that matches nothing still makes a valid array.
        EXPECT_TRUE(events.GetEventsStartingBetween(
            transaction, 1000, 2000, {"no \"such\" status"}).empty());
    });
}

// --- EV4: the sweeps ----------------------------------------------------------

KeyValueTable MakeTimedEvent(std::string_view title, int64_t startsAtUs,
                             std::optional<int64_t> endsAtUs, std::string_view status) {
    KeyValueTable values = WithStatus(MakeEvent(title, startsAtUs), status);
    if (endsAtUs) values[std::string(DbSchema::kEventsEndsAtUs)] = StringFromInt(*endsAtUs);
    return values;
}

std::vector<std::string> TitlesOf(const KeyValueTableArray& rows) {
    std::vector<std::string> titles;
    for (const KeyValueTable& row : rows) titles.push_back(row.at(std::string(DbSchema::kEventsTitle)));
    return titles;
}

// "Ended" is the end time, or start + the default duration without one. Only
// approved events, and only ones that also started before the cutoff.
TEST(EventsTest, GetApprovedEndedBeforeUsesTheEndOrTheDefaultDuration) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetApprovedEndedBeforeUsesTheEndOrTheDefaultDuration", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string_view approved = DbSchema::kEventsStatusApproved;
        // Cutoff 10'000, default duration 500.
        events.AddEvent(transaction, MakeTimedEvent("Ended at 9000", 8000, 9000, approved));
        events.AddEvent(transaction, MakeTimedEvent("No end, so 9999", 9499, std::nullopt, approved));
        events.AddEvent(transaction, MakeTimedEvent("No end, so 10000", 9500, std::nullopt, approved));
        events.AddEvent(transaction, MakeTimedEvent("Ends at the cutoff", 7000, 10'000, approved));
        events.AddEvent(transaction, MakeTimedEvent("Still going", 9000, 12'000, approved));
        // An end before its start can't drag a future event in.
        events.AddEvent(transaction, MakeTimedEvent("Bad end, future start", 11'000, 1000, approved));
        events.AddEvent(transaction, MakeTimedEvent("Pending", 1000, 2000, DbSchema::kEventsStatusPending));
        events.AddEvent(transaction, MakeTimedEvent("Rejected", 1000, 2000, DbSchema::kEventsStatusRejected));
        events.AddEvent(transaction, MakeTimedEvent("Archived", 1000, 2000, DbSchema::kEventsStatusArchived));

        EXPECT_EQ(TitlesOf(events.GetApprovedEndedBefore(transaction, 10'000, 500, 100)),
                  (std::vector<std::string>{"Ended at 9000", "No end, so 9999"}));
    });
}

TEST(EventsTest, GetApprovedEndedBeforeTakesTheOldestFirstUpToTheLimit) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetApprovedEndedBeforeTakesTheOldestFirstUpToTheLimit", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string_view approved = DbSchema::kEventsStatusApproved;
        events.AddEvent(transaction, MakeTimedEvent("Third", 3000, 3100, approved));
        events.AddEvent(transaction, MakeTimedEvent("First", 1000, 1100, approved));
        events.AddEvent(transaction, MakeTimedEvent("Second", 2000, 2100, approved));

        EXPECT_EQ(TitlesOf(events.GetApprovedEndedBefore(transaction, 10'000, 500, 2)),
                  (std::vector<std::string>{"First", "Second"}));
    });
}

// --- EV4 §10: the past-events list --------------------------------------------

// Started before the bound, ending at or after the earliest end (the end, or
// start + the default duration), in the given statuses, newest start first.
TEST(EventsTest, GetStartedBeforeEndingSinceIsTheWindowNewestFirst) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetStartedBeforeEndingSinceIsTheWindowNewestFirst", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string_view approved = DbSchema::kEventsStatusApproved;
        const std::string_view archived = DbSchema::kEventsStatusArchived;
        // Started before 10'000, ending at or after 5000; default duration 500.
        events.AddEvent(transaction, MakeTimedEvent("Ended at 5000", 4000, 5000, archived));
        events.AddEvent(transaction, MakeTimedEvent("Ended at 4999", 4000, 4999, archived));
        events.AddEvent(transaction, MakeTimedEvent("No end, so 5000", 4500, std::nullopt, archived));
        events.AddEvent(transaction, MakeTimedEvent("No end, so 4999", 4499, std::nullopt, archived));
        events.AddEvent(transaction, MakeTimedEvent("Going now", 9000, 12'000, approved));
        events.AddEvent(transaction, MakeTimedEvent("Starts at the bound", 10'000, 10'500, approved));
        events.AddEvent(transaction, MakeTimedEvent("Pending", 8000, 8100, DbSchema::kEventsStatusPending));
        events.AddEvent(transaction, MakeTimedEvent("Rejected", 8000, 8100, DbSchema::kEventsStatusRejected));

        EXPECT_EQ(TitlesOf(events.GetStartedBeforeEndingSince(
                      transaction, 10'000, 5000, 500, {approved, archived})),
                  (std::vector<std::string>{"Going now", "No end, so 5000", "Ended at 5000"}));
        EXPECT_EQ(TitlesOf(events.GetStartedBeforeEndingSince(
                      transaction, 10'000, 5000, 500, {approved})),
                  (std::vector<std::string>{"Going now"}));
        EXPECT_TRUE(events.GetStartedBeforeEndingSince(transaction, 10'000, 5000, 500, {}).empty());
    });
}

// The same start breaks its tie by id, newest first too.
TEST(EventsTest, GetStartedBeforeEndingSinceBreaksStartTiesByIdDescending) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetStartedBeforeEndingSinceBreaksStartTiesByIdDescending", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string_view archived = DbSchema::kEventsStatusArchived;
        events.AddEvent(transaction, MakeTimedEvent("Earlier id", 3000, 3100, archived));
        events.AddEvent(transaction, MakeTimedEvent("Later id", 3000, 3100, archived));
        events.AddEvent(transaction, MakeTimedEvent("Older", 2000, 2100, archived));

        EXPECT_EQ(TitlesOf(events.GetStartedBeforeEndingSince(
                      transaction, 10'000, 0, 500, {archived})),
                  (std::vector<std::string>{"Later id", "Earlier id", "Older"}));
    });
}

TEST(EventsTest, GetPendingStartedBeforeTakesTheOldestPendingUpToTheLimit) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetPendingStartedBeforeTakesTheOldestPendingUpToTheLimit", [&](Transaction& transaction) {
        Events events(testDb.GetDatabaseHelper());
        const std::string_view pending = DbSchema::kEventsStatusPending;
        events.AddEvent(transaction, MakeTimedEvent("Second", 2000, std::nullopt, pending));
        events.AddEvent(transaction, MakeTimedEvent("First", 1000, std::nullopt, pending));
        events.AddEvent(transaction, MakeTimedEvent("Third", 3000, std::nullopt, pending));
        events.AddEvent(transaction, MakeTimedEvent("Starts at now", 5000, std::nullopt, pending));
        events.AddEvent(transaction, MakeTimedEvent("Future", 6000, std::nullopt, pending));
        events.AddEvent(transaction,
                        MakeTimedEvent("Approved", 1000, std::nullopt, DbSchema::kEventsStatusApproved));
        events.AddEvent(transaction,
                        MakeTimedEvent("Rejected", 1000, std::nullopt, DbSchema::kEventsStatusRejected));

        EXPECT_EQ(TitlesOf(events.GetPendingStartedBefore(transaction, 5000, 100)),
                  (std::vector<std::string>{"First", "Second", "Third"}));
        EXPECT_EQ(TitlesOf(events.GetPendingStartedBefore(transaction, 5000, 2)),
                  (std::vector<std::string>{"First", "Second"}));
    });
}

// EV3 §1.2: events.submission_id is a real FK, so deleting a spam submission
// deletes its dates. Events from elsewhere are untouched.
TEST(EventsTest, DeletingASubmissionDeletesItsDates) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DeletingASubmissionDeletesItsDates", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t spam = AddSubmission(transaction, databaseHelper, 42);
        int64_t kept = AddSubmission(transaction, databaseHelper, 43);
        int64_t spamA = events.AddEvent(transaction, MakeSubmittedEvent("Spam A", 1000, spam));
        int64_t spamB = events.AddEvent(transaction, MakeSubmittedEvent("Spam B", 2000, spam));
        int64_t keptDate = events.AddEvent(transaction, MakeSubmittedEvent("Real", 1000, kept));
        int64_t notSubmitted = events.AddEvent(transaction, MakeEvent("Hand-entered", 1000));

        DbCrud::DeleteRow(transaction, databaseHelper, DbSchema::kEventSubmissionsTable,
                          DbSchema::kEventSubmissionsId, StringFromInt(spam));

        EXPECT_TRUE(events.GetEvent(transaction, spamA).empty());
        EXPECT_TRUE(events.GetEvent(transaction, spamB).empty());
        EXPECT_FALSE(events.GetEvent(transaction, keptDate).empty());
        EXPECT_FALSE(events.GetEvent(transaction, notSubmitted).empty());
    });
}

// Postgres treats NULLs as distinct, so neither kind of hand-entered event
// trips the (source_id, external_key) rule: no source at all, or a source with
// no key (an event credited to a source but typed in by hand).
TEST(EventsTest, HandEnteredEventsNeverCollide) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("HandEnteredEventsNeverCollide", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t sourceId = AddSource(transaction, databaseHelper, "Kremwerk");

        EXPECT_GT(events.AddEvent(transaction, MakeEvent("No source 1", 1000)), 0);
        EXPECT_GT(events.AddEvent(transaction, MakeEvent("No source 2", 1000)), 0);

        KeyValueTable credited = MakeEvent("Credited 1", 1000);
        credited[std::string(DbSchema::kEventsSourceId)] = StringFromInt(sourceId);
        EXPECT_GT(events.AddEvent(transaction, credited), 0);
        credited[std::string(DbSchema::kEventsTitle)] = "Credited 2";
        EXPECT_GT(events.AddEvent(transaction, credited), 0);
    });
}

// The ingestion idempotency anchor: a source can't hold the same item twice.
TEST(EventsTest, DuplicateSourceAndExternalKeyThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSourceAndExternalKeyThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Events events(databaseHelper);
        int64_t sourceId = AddSource(transaction, databaseHelper, "Kremwerk");
        events.AddEvent(transaction, MakeIngestedEvent("Night", 1000, sourceId, "uid-1"));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(
            events.AddEvent(transaction,
                            MakeIngestedEvent("Night again", 2000, sourceId, "uid-1")),
            std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
