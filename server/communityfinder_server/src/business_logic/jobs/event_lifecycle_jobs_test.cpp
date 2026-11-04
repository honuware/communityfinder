#include "business_logic/jobs/event_lifecycle_jobs.h"

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/person.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/submission_helper.h"
#include "db_schema/event_sources.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "db_schema/people.h"
#include "sql_util/table_helpers/event_sources.h"
#include "sql_util/table_helpers/people.h"
#include "test/src/util/database_test_helper.h"
#include "util/secrets/secrets_helper.h"
#include "util/types.h"

// EV4 §4.1 — the archive and expiry sweeps.
namespace Jobs {
namespace {

constexpr int64_t kUsPerMinute = 60LL * 1'000'000LL;
constexpr int64_t kUsPerHour = 60 * kUsPerMinute;
constexpr int64_t kCuratorId = 77;  // reviewer ids aren't FKs (EV1 OQ8 (e))

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

// updated_us = 0, so a sweep's bump shows (CLAUDE.md: insert and update can
// share a microsecond).
int64_t AddEvent(Transaction& transaction, DatabaseHelper databaseHelper, std::string_view title,
                 std::string_view status, int64_t startsAtUs,
                 std::optional<int64_t> endsAtUs = std::nullopt) {
    KeyValueTable values = {
        { std::string(DbSchema::kEventsTitle), std::string(title) },
        { std::string(DbSchema::kEventsStatus), std::string(status) },
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs) },
        { std::string(DbSchema::kEventsUpdatedUs), "0" },
    };
    if (endsAtUs) values[std::string(DbSchema::kEventsEndsAtUs)] = StringFromInt(*endsAtUs);
    return TableHelpers::Events(databaseHelper).AddEvent(transaction, values);
}

KeyValueTable EventRow(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
}

std::string StatusOf(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return Column(EventRow(transaction, databaseHelper, eventId), DbSchema::kEventsStatus);
}

// Moves an event's start (and end, when it has one) by `deltaUs`, as the
// test_helper's age-event does: a submission's dates can only be made in the
// future, so the expiry tests age them into the past.
void ShiftEvent(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId,
                int64_t deltaUs) {
    KeyValueTable row = EventRow(transaction, databaseHelper, eventId);
    KeyValueTable updates = {
        { std::string(DbSchema::kEventsStartsAtUs),
          StringFromInt(std::stoll(Column(row, DbSchema::kEventsStartsAtUs)) + deltaUs) },
    };
    if (!Column(row, DbSchema::kEventsEndsAtUs).empty()) {
        updates[std::string(DbSchema::kEventsEndsAtUs)] =
            StringFromInt(std::stoll(Column(row, DbSchema::kEventsEndsAtUs)) + deltaUs);
    }
    TableHelpers::Events(databaseHelper).UpdateEvent(transaction, eventId, updates);
}

// A submission of `dateCount` daily dates from two days out, by a new verified
// person.
Events::SubmitResult Submit(Transaction& transaction, DatabaseHelper databaseHelper,
                            std::string_view email, size_t dateCount) {
    Auth::PersonHelper(databaseHelper).CreateFullyValidatedUser(
        transaction, { std::string(email), "Sam", "Sample" }, "Password123!");
    const int64_t personId = std::stoll(TableHelpers::People(databaseHelper)
                                            .LookupPersonByEmail(transaction, email)
                                            .at(std::string(DbSchema::kPeopleId)));
    Events::SubmissionRequest request;
    request.title = "Sample Dance Night";
    request.agreed = true;
    const int64_t nowUs = NowUs(transaction);
    for (size_t index = 0; index < dateCount; ++index) {
        request.dates.push_back(
            { nowUs + (2 + static_cast<int64_t>(index)) * Events::kUsPerDay, std::nullopt });
    }
    Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(databaseHelper);
    Events::SubmitResult result =
        Events::SubmissionHelper(databaseHelper).Submit(transaction, *secrets, request, personId);
    EXPECT_TRUE(result.ok) << result.errorCode;
    return result;
}

std::string SubmissionColumn(Transaction& transaction, DatabaseHelper databaseHelper,
                             int64_t submissionId, std::string_view column) {
    return Column(TableHelpers::EventSubmissions(databaseHelper).GetSubmission(transaction, submissionId),
                  column);
}

// --- ArchivePastEvents ---

TEST(EventLifecycleJobsTest, ArchiveWaitsSixHoursAfterTheEnd) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ArchiveWaitsSixHoursAfterTheEnd", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        const std::string_view approved = DbSchema::kEventsStatusApproved;
        int64_t endedLongEnough = AddEvent(transaction, db, "Ended 6h01m ago", approved,
                                           nowUs - 9 * kUsPerHour, nowUs - 6 * kUsPerHour - kUsPerMinute);
        int64_t endedRecently = AddEvent(transaction, db, "Ended 5h59m ago", approved,
                                         nowUs - 9 * kUsPerHour, nowUs - 6 * kUsPerHour + kUsPerMinute);
        // Started long ago, still going: the end decides, not the start.
        int64_t stillGoing = AddEvent(transaction, db, "Festival", approved,
                                      nowUs - 3 * Events::kUsPerDay, nowUs + kUsPerHour);

        ArchiveResult result = EventLifecycleJobs(db).ArchivePastEvents(
            transaction, nowUs, kEventSweepBatchLimit);
        EXPECT_EQ(result.archived, 1);
        EXPECT_EQ(StatusOf(transaction, db, endedLongEnough), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(StatusOf(transaction, db, endedRecently), DbSchema::kEventsStatusApproved);
        EXPECT_EQ(StatusOf(transaction, db, stillGoing), DbSchema::kEventsStatusApproved);
        // Through UpdateEvent, so updated_us bumps; the review stays as it was.
        KeyValueTable row = EventRow(transaction, db, endedLongEnough);
        EXPECT_GT(std::stoll(Column(row, DbSchema::kEventsUpdatedUs)), 0);
    });
}

// No end: it ends two hours after it starts (EV2's ICS default), so it's
// archived eight hours after the start.
TEST(EventLifecycleJobsTest, ArchiveCountsTwoHoursWhenThereIsNoEnd) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ArchiveCountsTwoHoursWhenThereIsNoEnd", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        const std::string_view approved = DbSchema::kEventsStatusApproved;
        int64_t old = AddEvent(transaction, db, "Started 8h01m ago", approved,
                               nowUs - 8 * kUsPerHour - kUsPerMinute);
        int64_t recent = AddEvent(transaction, db, "Started 7h59m ago", approved,
                                  nowUs - 8 * kUsPerHour + kUsPerMinute);

        EXPECT_EQ(EventLifecycleJobs(db).ArchivePastEvents(transaction, nowUs, kEventSweepBatchLimit).archived, 1);
        EXPECT_EQ(StatusOf(transaction, db, old), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(StatusOf(transaction, db, recent), DbSchema::kEventsStatusApproved);
    });
}

TEST(EventLifecycleJobsTest, ArchiveTakesTheOldestFirstUpToTheLimitAndThenStops) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ArchiveTakesTheOldestFirst", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        const std::string_view approved = DbSchema::kEventsStatusApproved;
        int64_t newest = AddEvent(transaction, db, "Two days ago", approved, nowUs - 2 * Events::kUsPerDay);
        int64_t oldest = AddEvent(transaction, db, "Four days ago", approved, nowUs - 4 * Events::kUsPerDay);
        int64_t middle = AddEvent(transaction, db, "Three days ago", approved, nowUs - 3 * Events::kUsPerDay);
        EventLifecycleJobs jobs(db);

        EXPECT_EQ(jobs.ArchivePastEvents(transaction, nowUs, 2).archived, 2);
        EXPECT_EQ(StatusOf(transaction, db, oldest), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(StatusOf(transaction, db, middle), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(StatusOf(transaction, db, newest), DbSchema::kEventsStatusApproved);

        // The next run drains the rest; after that there's nothing to do.
        EXPECT_EQ(jobs.ArchivePastEvents(transaction, nowUs, 2).archived, 1);
        EXPECT_EQ(StatusOf(transaction, db, newest), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(jobs.ArchivePastEvents(transaction, nowUs, 2).archived, 0);
    });
}

TEST(EventLifecycleJobsTest, ArchiveLeavesEverythingButPastApprovedEventsAlone) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ArchiveLeavesTheRestAlone", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        const int64_t lastWeek = nowUs - 7 * Events::kUsPerDay;
        int64_t pending = AddEvent(transaction, db, "Pending", DbSchema::kEventsStatusPending, lastWeek);
        int64_t rejected = AddEvent(transaction, db, "Rejected", DbSchema::kEventsStatusRejected, lastWeek);
        int64_t archived = AddEvent(transaction, db, "Archived", DbSchema::kEventsStatusArchived, lastWeek);
        int64_t future = AddEvent(transaction, db, "Tomorrow", DbSchema::kEventsStatusApproved,
                                  nowUs + Events::kUsPerDay);

        EXPECT_EQ(EventLifecycleJobs(db).ArchivePastEvents(transaction, nowUs, kEventSweepBatchLimit).archived, 0);
        EXPECT_EQ(StatusOf(transaction, db, pending), DbSchema::kEventsStatusPending);
        EXPECT_EQ(StatusOf(transaction, db, rejected), DbSchema::kEventsStatusRejected);
        EXPECT_EQ(StatusOf(transaction, db, future), DbSchema::kEventsStatusApproved);
        // Untouched, so not even updated_us moved.
        EXPECT_EQ(Column(EventRow(transaction, db, archived), DbSchema::kEventsUpdatedUs), "0");
    });
}

// EV1 OQ10 (d), now with a real archive in the loop: a re-scan can't reopen an
// archived event.
TEST(EventLifecycleJobsTest, AnArchivedEventIsKeptWhenItsSourceSendsItAgain) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ArchivedKeptOnRescan", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        int64_t sourceId = TableHelpers::EventSources(db).AddSource(transaction, {
            { std::string(DbSchema::kEventSourcesName), "Community calendar" },
            { std::string(DbSchema::kEventSourcesKind), std::string(DbSchema::kEventSourcesKindIcsFeed) },
        });
        Events::EventHelper helper(db);
        Events::IngestItem item;
        item.sourceId = sourceId;
        item.externalKey = "uid-1";
        item.title = "Last week's show";
        item.startsAtUs = nowUs - 7 * Events::kUsPerDay;
        Events::IngestResult first = helper.IngestEvents(transaction, { item });
        ASSERT_TRUE(first.ok) << first.errorCode;
        const int64_t eventId = first.items[0].eventId;
        ASSERT_TRUE(helper.ReviewEvent(transaction, eventId, Events::ReviewAction::Approve, kCuratorId, "").ok);
        ASSERT_EQ(EventLifecycleJobs(db).ArchivePastEvents(transaction, nowUs, kEventSweepBatchLimit).archived, 1);

        item.title = "Last week's show (renamed)";
        Events::IngestResult again = helper.IngestEvents(transaction, { item });
        ASSERT_TRUE(again.ok) << again.errorCode;
        EXPECT_EQ(again.items[0].outcome, Events::kIngestOutcomeArchivedKept);
        EXPECT_EQ(StatusOf(transaction, db, eventId), DbSchema::kEventsStatusArchived);
        EXPECT_EQ(Column(EventRow(transaction, db, eventId), DbSchema::kEventsTitle), "Last week's show");
    });
}

// --- ExpireUnreviewedEvents ---

TEST(EventLifecycleJobsTest, ExpireRejectsPendingEventsWhoseStartPassed) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ExpireRejectsPendingEvents", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        int64_t started = AddEvent(transaction, db, "Started a minute ago", DbSchema::kEventsStatusPending,
                                   nowUs - kUsPerMinute, nowUs + kUsPerHour);
        int64_t upcoming = AddEvent(transaction, db, "In a minute", DbSchema::kEventsStatusPending,
                                    nowUs + kUsPerMinute);
        int64_t approved = AddEvent(transaction, db, "Approved", DbSchema::kEventsStatusApproved,
                                    nowUs - Events::kUsPerDay);

        ExpireResult result = EventLifecycleJobs(db).ExpireUnreviewedEvents(
            transaction, nowUs, kEventSweepBatchLimit);
        EXPECT_EQ(result.expired, 1);
        EXPECT_TRUE(result.submissionNotices.empty());

        KeyValueTable row = EventRow(transaction, db, started);
        EXPECT_EQ(Column(row, DbSchema::kEventsStatus), DbSchema::kEventsStatusRejected);
        EXPECT_EQ(Column(row, DbSchema::kEventsReviewNotes), kReviewNoteDatePassed);
        EXPECT_TRUE(Column(row, DbSchema::kEventsReviewedByPersonId).empty());
        EXPECT_GE(std::stoll(Column(row, DbSchema::kEventsReviewedUs)), nowUs);
        EXPECT_GT(std::stoll(Column(row, DbSchema::kEventsUpdatedUs)), 0);
        EXPECT_EQ(StatusOf(transaction, db, upcoming), DbSchema::kEventsStatusPending);
        EXPECT_EQ(StatusOf(transaction, db, approved), DbSchema::kEventsStatusApproved);

        // Nothing left to do.
        EXPECT_EQ(EventLifecycleJobs(db).ExpireUnreviewedEvents(
                      transaction, nowUs, kEventSweepBatchLimit).expired, 0);
    });
}

TEST(EventLifecycleJobsTest, ExpireTakesTheOldestFirstUpToTheLimit) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ExpireTakesTheOldestFirst", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        const int64_t nowUs = NowUs(transaction);
        const std::string_view pending = DbSchema::kEventsStatusPending;
        int64_t newest = AddEvent(transaction, db, "An hour ago", pending, nowUs - kUsPerHour);
        int64_t oldest = AddEvent(transaction, db, "Three hours ago", pending, nowUs - 3 * kUsPerHour);
        int64_t middle = AddEvent(transaction, db, "Two hours ago", pending, nowUs - 2 * kUsPerHour);
        EventLifecycleJobs jobs(db);

        EXPECT_EQ(jobs.ExpireUnreviewedEvents(transaction, nowUs, 2).expired, 2);
        EXPECT_EQ(StatusOf(transaction, db, oldest), DbSchema::kEventsStatusRejected);
        EXPECT_EQ(StatusOf(transaction, db, middle), DbSchema::kEventsStatusRejected);
        EXPECT_EQ(StatusOf(transaction, db, newest), DbSchema::kEventsStatusPending);
        EXPECT_EQ(jobs.ExpireUnreviewedEvents(transaction, nowUs, 2).expired, 1);
    });
}

// EV3's hook: closing a submission's last pending date sets the `expired`
// reason (it had none) and claims the decision email for the caller to send.
TEST(EventLifecycleJobsTest, ExpiringASubmissionsLastDatesClaimsItsEmail) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ExpiringClaimsTheEmail", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Events::SubmitResult submitted = Submit(transaction, db, "submitter@example.com", 2);
        ASSERT_EQ(submitted.eventIds.size(), 2u);
        for (int64_t eventId : submitted.eventIds) {
            ShiftEvent(transaction, db, eventId, -5 * Events::kUsPerDay);
        }

        ExpireResult result = EventLifecycleJobs(db).ExpireUnreviewedEvents(
            transaction, NowUs(transaction), kEventSweepBatchLimit);
        EXPECT_EQ(result.expired, 2);
        ASSERT_EQ(result.submissionNotices.size(), 1u);
        const Events::DecisionNotice& notice = result.submissionNotices[0];
        EXPECT_EQ(notice.submissionId, submitted.submissionId);
        EXPECT_EQ(notice.recipientEmail, "submitter@example.com");
        EXPECT_EQ(notice.rejectReason, DbSchema::kEventSubmissionsRejectReasonExpired);
        ASSERT_EQ(notice.dates.size(), 2u);
        for (const auto& date : notice.dates) {
            EXPECT_EQ(date.status, DbSchema::kEventsStatusRejected);
        }
        EXPECT_EQ(SubmissionColumn(transaction, db, submitted.submissionId,
                                   DbSchema::kEventSubmissionsRejectReason),
                  DbSchema::kEventSubmissionsRejectReasonExpired);
        EXPECT_FALSE(SubmissionColumn(transaction, db, submitted.submissionId,
                                      DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
    });
}

// One date passed, one still to come: the passed one expires and the reason is
// set, but the email waits for the other date's decision.
TEST(EventLifecycleJobsTest, ExpiringOneOfTwoDatesSendsNoEmailYet) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ExpiringOneOfTwoDates", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Events::SubmitResult submitted = Submit(transaction, db, "submitter@example.com", 2);
        ASSERT_EQ(submitted.eventIds.size(), 2u);
        ShiftEvent(transaction, db, submitted.eventIds[0], -5 * Events::kUsPerDay);

        ExpireResult result = EventLifecycleJobs(db).ExpireUnreviewedEvents(
            transaction, NowUs(transaction), kEventSweepBatchLimit);
        EXPECT_EQ(result.expired, 1);
        EXPECT_TRUE(result.submissionNotices.empty());
        EXPECT_EQ(StatusOf(transaction, db, submitted.eventIds[1]), DbSchema::kEventsStatusPending);
        EXPECT_EQ(SubmissionColumn(transaction, db, submitted.submissionId,
                                   DbSchema::kEventSubmissionsRejectReason),
                  DbSchema::kEventSubmissionsRejectReasonExpired);
        EXPECT_TRUE(SubmissionColumn(transaction, db, submitted.submissionId,
                                     DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
    });
}

// A curator's reason stays: one date was turned down as not-a-fit, and the
// other's date then passed.
TEST(EventLifecycleJobsTest, ExpiryKeepsTheReasonACuratorGave) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ExpiryKeepsTheCuratorsReason", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Events::SubmitResult submitted = Submit(transaction, db, "submitter@example.com", 2);
        ASSERT_EQ(submitted.eventIds.size(), 2u);
        Events::SubmissionDecision decision;
        decision.reason = std::string(DbSchema::kEventSubmissionsRejectReasonNotAFit);
        Events::DateReviewResult reviewed = Events::SubmissionHelper(db).ReviewDate(
            transaction, submitted.eventIds[0], Events::ReviewAction::Reject, kCuratorId, "", decision);
        ASSERT_TRUE(reviewed.review.ok) << reviewed.review.errorCode;
        ASSERT_FALSE(reviewed.notice.has_value());
        ShiftEvent(transaction, db, submitted.eventIds[1], -5 * Events::kUsPerDay);

        ExpireResult result = EventLifecycleJobs(db).ExpireUnreviewedEvents(
            transaction, NowUs(transaction), kEventSweepBatchLimit);
        EXPECT_EQ(result.expired, 1);
        ASSERT_EQ(result.submissionNotices.size(), 1u);
        EXPECT_EQ(result.submissionNotices[0].rejectReason, DbSchema::kEventSubmissionsRejectReasonNotAFit);
        EXPECT_EQ(SubmissionColumn(transaction, db, submitted.submissionId,
                                   DbSchema::kEventSubmissionsRejectReason),
                  DbSchema::kEventSubmissionsRejectReasonNotAFit);
    });
}

}  // namespace
}  // namespace Jobs
