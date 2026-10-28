#include "event_category_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "categories.h"
#include "db_schema/categories.h"
#include "db_schema/event_category_assignments.h"
#include "db_schema/events.h"
#include "events.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

int64_t AddCategory(Transaction& transaction, DatabaseHelper databaseHelper,
                    std::string_view slug, int64_t sortOrder) {
    Categories categories(databaseHelper);
    return categories.AddCategory(transaction, {
        { std::string(DbSchema::kCategoriesSlug), std::string(slug) },
        { std::string(DbSchema::kCategoriesDisplayName), std::string(slug) },
        { std::string(DbSchema::kCategoriesSortOrder), StringFromInt(sortOrder) },
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
        slugs.push_back(row.at(std::string(kEventCategoryAssignmentsCategorySlug)));
    }
    return slugs;
}

TEST(EventCategoryAssignmentsTest, AssignAndGetForEventInDisplayOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignAndGetForEventInDisplayOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventCategoryAssignments assignments(databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, "Drag brunch");
        int64_t pride = AddCategory(transaction, databaseHelper, "pride", 30);
        int64_t bar = AddCategory(transaction, databaseHelper, "bar", 10);
        int64_t drag = AddCategory(transaction, databaseHelper, "drag", 20);

        // Assigned out of display order.
        EXPECT_GT(assignments.Assign(transaction, eventId, pride), 0);
        assignments.Assign(transaction, eventId, bar);
        assignments.Assign(transaction, eventId, drag);

        KeyValueTableArray rows = assignments.GetForEvent(transaction, eventId);
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "bar", "drag", "pride" }));
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kEventCategoryAssignmentsCategoryId)),
                  StringFromInt(bar));

        EXPECT_TRUE(assignments.GetForEvent(transaction,
                                            AddEvent(transaction, databaseHelper, "Bare"))
                        .empty());
    });
}

TEST(EventCategoryAssignmentsTest, GetForEventsBatchesByEvent) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetForEventsBatchesByEvent", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventCategoryAssignments assignments(databaseHelper);
        int64_t first = AddEvent(transaction, databaseHelper, "First");
        int64_t second = AddEvent(transaction, databaseHelper, "Second");
        int64_t unasked = AddEvent(transaction, databaseHelper, "Unasked");
        int64_t bar = AddCategory(transaction, databaseHelper, "bar", 10);
        int64_t drag = AddCategory(transaction, databaseHelper, "drag", 20);
        assignments.Assign(transaction, second, drag);
        assignments.Assign(transaction, second, bar);
        assignments.Assign(transaction, first, drag);
        assignments.Assign(transaction, unasked, bar);

        KeyValueTableArray rows = assignments.GetForEvents(transaction, { second, first });
        ASSERT_EQ(rows.size(), 3u);
        // Event order, then display order within an event.
        const std::string eventIdColumn(DbSchema::kEventCategoryAssignmentsEventId);
        EXPECT_EQ(rows[0].at(eventIdColumn), StringFromInt(first));
        EXPECT_EQ(rows[1].at(eventIdColumn), StringFromInt(second));
        EXPECT_EQ(rows[2].at(eventIdColumn), StringFromInt(second));
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "drag", "bar", "drag" }));

        EXPECT_TRUE(assignments.GetForEvents(transaction, {}).empty());
    });
}

TEST(EventCategoryAssignmentsTest, GetEventIdsForCategory) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetEventIdsForCategory", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventCategoryAssignments assignments(databaseHelper);
        int64_t first = AddEvent(transaction, databaseHelper, "First");
        int64_t second = AddEvent(transaction, databaseHelper, "Second");
        int64_t other = AddEvent(transaction, databaseHelper, "Other");
        int64_t drag = AddCategory(transaction, databaseHelper, "drag", 10);
        int64_t bar = AddCategory(transaction, databaseHelper, "bar", 20);
        assignments.Assign(transaction, second, drag);
        assignments.Assign(transaction, first, drag);
        assignments.Assign(transaction, other, bar);

        EXPECT_EQ(assignments.GetEventIdsForCategory(transaction, drag),
                  (std::vector<int64_t>{ first, second }));
        EXPECT_TRUE(assignments.GetEventIdsForCategory(
                        transaction, AddCategory(transaction, databaseHelper, "unused", 30))
                        .empty());
    });
}

TEST(EventCategoryAssignmentsTest, RemoveDeletesOnlyThatPair) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RemoveDeletesOnlyThatPair", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventCategoryAssignments assignments(databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, "Drag brunch");
        int64_t otherEvent = AddEvent(transaction, databaseHelper, "Other");
        int64_t bar = AddCategory(transaction, databaseHelper, "bar", 10);
        int64_t drag = AddCategory(transaction, databaseHelper, "drag", 20);
        assignments.Assign(transaction, eventId, bar);
        assignments.Assign(transaction, eventId, drag);
        assignments.Assign(transaction, otherEvent, drag);

        assignments.Remove(transaction, eventId, drag);
        // Removing a pair that isn't there is a no-op.
        assignments.Remove(transaction, eventId, drag);

        EXPECT_EQ(Slugs(assignments.GetForEvent(transaction, eventId)),
                  (std::vector<std::string>{ "bar" }));
        EXPECT_EQ(Slugs(assignments.GetForEvent(transaction, otherEvent)),
                  (std::vector<std::string>{ "drag" }));
    });
}

TEST(EventCategoryAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventCategoryAssignments assignments(databaseHelper);
        int64_t eventId = AddEvent(transaction, databaseHelper, "Drag brunch");
        int64_t drag = AddCategory(transaction, databaseHelper, "drag", 10);
        assignments.Assign(transaction, eventId, drag);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, eventId, drag), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
