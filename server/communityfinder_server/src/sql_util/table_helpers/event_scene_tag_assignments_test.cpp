#include "event_scene_tag_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/event_scene_tag_assignments.h"
#include "db_schema/events.h"
#include "db_schema/scene_tags.h"
#include "events.h"
#include "scene_tags.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

int64_t AddSceneTag(Transaction& transaction, DatabaseHelper databaseHelper,
                    std::string_view slug, int64_t sortOrder) {
    SceneTags sceneTags(databaseHelper);
    return sceneTags.AddSceneTag(transaction, {
        { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
        { std::string(DbSchema::kSceneTagsDisplayName), std::string(slug) },
        { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder) },
    });
}

int64_t AddEvent(Transaction& transaction, DatabaseHelper databaseHelper,
                 std::string_view title) {
    Events events(databaseHelper);
    return events.AddEvent(transaction, {
        { std::string(DbSchema::kEventsTitle), std::string(title) },
        { std::string(DbSchema::kEventsStartsAtUs), "1000" },
    });
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(kEventSceneTagAssignmentsSceneTagSlug)));
    }
    return slugs;
}

TEST(EventSceneTagAssignmentsTest, AssignAndGetForEventInDisplayOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignAndGetForEventInDisplayOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventSceneTagAssignments assignments(databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, "Bear happy hour");
        int64_t sober = AddSceneTag(transaction, databaseHelper, "sober", 30);
        int64_t bear = AddSceneTag(transaction, databaseHelper, "bear", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);

        EXPECT_GT(assignments.Assign(transaction, eventId, sober), 0);
        assignments.Assign(transaction, eventId, bear);
        assignments.Assign(transaction, eventId, leather);

        KeyValueTableArray rows = assignments.GetForEvent(transaction, eventId);
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "bear", "leather", "sober" }));
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kEventSceneTagAssignmentsSceneTagId)),
                  StringFromInt(bear));
    });
}

TEST(EventSceneTagAssignmentsTest, GetForEventsBatchesByEvent) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetForEventsBatchesByEvent", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventSceneTagAssignments assignments(databaseHelper);
        int64_t first = AddEvent(transaction, databaseHelper, "First");
        int64_t second = AddEvent(transaction, databaseHelper, "Second");
        int64_t unasked = AddEvent(transaction, databaseHelper, "Unasked");
        int64_t bear = AddSceneTag(transaction, databaseHelper, "bear", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        assignments.Assign(transaction, second, leather);
        assignments.Assign(transaction, second, bear);
        assignments.Assign(transaction, first, leather);
        assignments.Assign(transaction, unasked, bear);

        KeyValueTableArray rows = assignments.GetForEvents(transaction, { second, first });
        ASSERT_EQ(rows.size(), 3u);
        const std::string eventIdColumn(DbSchema::kEventSceneTagAssignmentsEventId);
        EXPECT_EQ(rows[0].at(eventIdColumn), StringFromInt(first));
        EXPECT_EQ(rows[1].at(eventIdColumn), StringFromInt(second));
        EXPECT_EQ(rows[2].at(eventIdColumn), StringFromInt(second));
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "leather", "bear", "leather" }));

        EXPECT_TRUE(assignments.GetForEvents(transaction, {}).empty());
    });
}

TEST(EventSceneTagAssignmentsTest, GetEventIdsForSceneTag) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetEventIdsForSceneTag", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventSceneTagAssignments assignments(databaseHelper);
        int64_t first = AddEvent(transaction, databaseHelper, "First");
        int64_t second = AddEvent(transaction, databaseHelper, "Second");
        int64_t other = AddEvent(transaction, databaseHelper, "Other");
        int64_t bear = AddSceneTag(transaction, databaseHelper, "bear", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        assignments.Assign(transaction, second, bear);
        assignments.Assign(transaction, first, bear);
        assignments.Assign(transaction, other, leather);

        EXPECT_EQ(assignments.GetEventIdsForSceneTag(transaction, bear),
                  (std::vector<int64_t>{ first, second }));
        EXPECT_TRUE(assignments.GetEventIdsForSceneTag(
                        transaction, AddSceneTag(transaction, databaseHelper, "unused", 30))
                        .empty());
    });
}

TEST(EventSceneTagAssignmentsTest, RemoveDeletesOnlyThatPair) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RemoveDeletesOnlyThatPair", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventSceneTagAssignments assignments(databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, "Bear happy hour");
        int64_t otherEvent = AddEvent(transaction, databaseHelper, "Other");
        int64_t bear = AddSceneTag(transaction, databaseHelper, "bear", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        assignments.Assign(transaction, eventId, bear);
        assignments.Assign(transaction, eventId, leather);
        assignments.Assign(transaction, otherEvent, leather);

        assignments.Remove(transaction, eventId, leather);
        // Removing a pair that isn't there is a no-op.
        assignments.Remove(transaction, eventId, leather);

        EXPECT_EQ(Slugs(assignments.GetForEvent(transaction, eventId)),
                  (std::vector<std::string>{ "bear" }));
        EXPECT_EQ(Slugs(assignments.GetForEvent(transaction, otherEvent)),
                  (std::vector<std::string>{ "leather" }));
    });
}

TEST(EventSceneTagAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventSceneTagAssignments assignments(databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, "Bear happy hour");
        int64_t bear = AddSceneTag(transaction, databaseHelper, "bear", 10);
        assignments.Assign(transaction, eventId, bear);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, eventId, bear), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
