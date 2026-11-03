#include "event_submissions.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/event_submissions.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeSubmission(int64_t personId) {
    return {
        { std::string(DbSchema::kEventSubmissionsSubmittedByPersonId), StringFromInt(personId) },
    };
}

KeyValueTable MakeSubmissionAt(int64_t personId, int64_t createdUs) {
    KeyValueTable values = MakeSubmission(personId);
    values[std::string(DbSchema::kEventSubmissionsCreatedUs)] = StringFromInt(createdUs);
    return values;
}

std::string IdOf(const KeyValueTable& row) {
    return row.at(std::string(DbSchema::kEventSubmissionsId));
}

TEST(EventSubmissionsTest, AddAndGetWithTheDefaults) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetWithTheDefaults", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSubmission(42);
        values[std::string(DbSchema::kEventSubmissionsSubmitterNote)] = "First Friday of the month";
        values[std::string(DbSchema::kEventSubmissionsRelationship)] =
            std::string(DbSchema::kEventSubmissionsRelationshipOrganizer);
        int64_t id = submissions.AddSubmission(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = submissions.GetSubmission(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsSubmittedByPersonId)), "42");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsSubmitterNote)),
                  "First Friday of the month");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRelationship)),
                  DbSchema::kEventSubmissionsRelationshipOrganizer);
        // Undecided: no reason, no message, no email yet.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRegularThing)), "f");
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventSubmissionsMessageToSubmitter)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs)).empty());
    });
}

TEST(EventSubmissionsTest, RelationshipDefaultsToOther) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RelationshipDefaultsToOther", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        int64_t id = submissions.AddSubmission(transaction, MakeSubmission(42));
        EXPECT_EQ(submissions.GetSubmission(transaction, id)
                      .at(std::string(DbSchema::kEventSubmissionsRelationship)),
                  DbSchema::kEventSubmissionsRelationshipOther);
    });
}

TEST(EventSubmissionsTest, GetSubmissionNotFoundIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetSubmissionNotFoundIsEmpty", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        EXPECT_TRUE(submissions.GetSubmission(transaction, 999999).empty());
    });
}

// The lock itself needs two connections to observe; one transaction can check
// that it reads the same row GetSubmission does (re-locking a held row is fine).
TEST(EventSubmissionsTest, GetSubmissionForUpdateReadsTheRow) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetSubmissionForUpdateReadsTheRow", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        int64_t id = submissions.AddSubmission(transaction, MakeSubmission(42));

        KeyValueTable locked = submissions.GetSubmissionForUpdate(transaction, id);
        EXPECT_EQ(locked, submissions.GetSubmission(transaction, id));
        EXPECT_EQ(IdOf(submissions.GetSubmissionForUpdate(transaction, id)), StringFromInt(id));
        EXPECT_TRUE(submissions.GetSubmissionForUpdate(transaction, 999999).empty());
    });
}

TEST(EventSubmissionsTest, GetSubmissionsForPersonIsNewestFirstAndOnlyTheirs) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetSubmissionsForPersonIsNewestFirstAndOnlyTheirs", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        int64_t oldest = submissions.AddSubmission(transaction, MakeSubmissionAt(42, 1000));
        int64_t newest = submissions.AddSubmission(transaction, MakeSubmissionAt(42, 3000));
        int64_t middleA = submissions.AddSubmission(transaction, MakeSubmissionAt(42, 2000));
        // Same moment as middleA: the higher id is the newer row.
        int64_t middleB = submissions.AddSubmission(transaction, MakeSubmissionAt(42, 2000));
        submissions.AddSubmission(transaction, MakeSubmissionAt(43, 4000));

        KeyValueTableArray rows = submissions.GetSubmissionsForPerson(transaction, 42);
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(IdOf(rows[0]), StringFromInt(newest));
        EXPECT_EQ(IdOf(rows[1]), StringFromInt(middleB));
        EXPECT_EQ(IdOf(rows[2]), StringFromInt(middleA));
        EXPECT_EQ(IdOf(rows[3]), StringFromInt(oldest));

        EXPECT_TRUE(submissions.GetSubmissionsForPerson(transaction, 44).empty());
    });
}

// The window includes its start: a submission at exactly sinceUs counts.
TEST(EventSubmissionsTest, CountSubmissionsSinceCountsOnlyTheWindowAndThePerson) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "CountSubmissionsSinceCountsOnlyTheWindowAndThePerson", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        submissions.AddSubmission(transaction, MakeSubmissionAt(42, 999));
        submissions.AddSubmission(transaction, MakeSubmissionAt(42, 1000));
        submissions.AddSubmission(transaction, MakeSubmissionAt(42, 5000));
        submissions.AddSubmission(transaction, MakeSubmissionAt(43, 5000));

        EXPECT_EQ(submissions.CountSubmissionsSince(transaction, 42, 1000), 2);
        EXPECT_EQ(submissions.CountSubmissionsSince(transaction, 42, 0), 3);
        EXPECT_EQ(submissions.CountSubmissionsSince(transaction, 42, 5001), 0);
        EXPECT_EQ(submissions.CountSubmissionsSince(transaction, 44, 0), 0);
    });
}

TEST(EventSubmissionsTest, UpdateBumpsUpdatedUsAndCanClearTheReason) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearTheReason", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSubmission(42);
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kEventSubmissionsUpdatedUs)] = "0";
        int64_t id = submissions.AddSubmission(transaction, values);

        submissions.UpdateSubmission(transaction, id, {
            { std::string(DbSchema::kEventSubmissionsRejectReason),
              std::string(DbSchema::kEventSubmissionsRejectReasonMissingDetails) },
            { std::string(DbSchema::kEventSubmissionsMessageToSubmitter),
              "Could you add the venue's address?" },
        });
        KeyValueTable row = submissions.GetSubmission(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsRejectReason)),
                  DbSchema::kEventSubmissionsRejectReasonMissingDetails);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsMessageToSubmitter)),
                  "Could you add the venue's address?");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventSubmissionsUpdatedUs))), 0);

        submissions.UpdateSubmission(transaction, id, {
            { std::string(DbSchema::kEventSubmissionsRejectReason), "NULL" },
        });
        EXPECT_TRUE(submissions.GetSubmission(transaction, id)
                        .at(std::string(DbSchema::kEventSubmissionsRejectReason))
                        .empty());
    });
}

// The decision email's exactly-once guard: only the first stamp wins, and a
// second attempt leaves the first stamp in place.
TEST(EventSubmissionsTest, MarkDecisionNotifiedStampsOnlyOnce) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MarkDecisionNotifiedStampsOnlyOnce", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSubmission(42);
        values[std::string(DbSchema::kEventSubmissionsUpdatedUs)] = "0";
        int64_t id = submissions.AddSubmission(transaction, values);
        int64_t other = submissions.AddSubmission(transaction, MakeSubmission(42));

        EXPECT_TRUE(submissions.MarkDecisionNotified(transaction, id, 1500));
        EXPECT_FALSE(submissions.MarkDecisionNotified(transaction, id, 2500));

        KeyValueTable row = submissions.GetSubmission(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs)), "1500");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventSubmissionsUpdatedUs))), 0);
        // Only the named submission is stamped.
        EXPECT_TRUE(submissions.GetSubmission(transaction, other)
                        .at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs))
                        .empty());
        // No such submission: nothing to stamp.
        EXPECT_FALSE(submissions.MarkDecisionNotified(transaction, 999999, 1500));
    });
}

// A failed send clears the stamp (EV3 Design pin-downs, "Mail"), after which
// the next decision may claim it again.
TEST(EventSubmissionsTest, AClearedStampCanBeClaimedAgain) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AClearedStampCanBeClaimedAgain", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        int64_t id = submissions.AddSubmission(transaction, MakeSubmission(42));
        ASSERT_TRUE(submissions.MarkDecisionNotified(transaction, id, 1500));

        submissions.UpdateSubmission(transaction, id, {
            { std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs), "NULL" },
        });
        EXPECT_TRUE(submissions.MarkDecisionNotified(transaction, id, 2500));
        EXPECT_EQ(submissions.GetSubmission(transaction, id)
                      .at(std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs)),
                  "2500");
    });
}

// `expired` is EV4's; a reason outside the eight is refused by the CHECK.
TEST(EventSubmissionsTest, EveryReasonIsAcceptedAndAnUnknownOneIsRejected) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "EveryReasonIsAcceptedAndAnUnknownOneIsRejected", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        for (std::string_view reason :
             {DbSchema::kEventSubmissionsRejectReasonDuplicate,
              DbSchema::kEventSubmissionsRejectReasonNotAFit,
              DbSchema::kEventSubmissionsRejectReasonMissingDetails,
              DbSchema::kEventSubmissionsRejectReasonPast,
              DbSchema::kEventSubmissionsRejectReasonSpam,
              DbSchema::kEventSubmissionsRejectReasonOther,
              DbSchema::kEventSubmissionsRejectReasonWithdrawn,
              DbSchema::kEventSubmissionsRejectReasonExpired}) {
            KeyValueTable values = MakeSubmission(42);
            values[std::string(DbSchema::kEventSubmissionsRejectReason)] = std::string(reason);
            EXPECT_GT(submissions.AddSubmission(transaction, values), 0) << reason;
        }

        KeyValueTable unknown = MakeSubmission(42);
        unknown[std::string(DbSchema::kEventSubmissionsRejectReason)] = "boring";
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(submissions.AddSubmission(transaction, unknown), std::exception);
    });
}

TEST(EventSubmissionsTest, UnknownRelationshipIsRejected) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownRelationshipIsRejected", [&](Transaction& transaction) {
        EventSubmissions submissions(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeSubmission(42);
        values[std::string(DbSchema::kEventSubmissionsRelationship)] = "fan";
        EXPECT_THROW(submissions.AddSubmission(transaction, values), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
