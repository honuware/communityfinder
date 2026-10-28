#include "venues.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

#include "db_schema/events.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/venues.h"
#include "events.h"
#include "neighborhoods.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeVenue(std::string_view name) {
    return { { std::string(DbSchema::kVenuesName), std::string(name) } };
}

int64_t AddNeighborhood(Transaction& transaction, DatabaseHelper databaseHelper,
                        std::string_view slug) {
    Neighborhoods neighborhoods(databaseHelper);
    return neighborhoods.AddNeighborhood(transaction, {
        { std::string(DbSchema::kNeighborhoodsSlug), std::string(slug) },
        { std::string(DbSchema::kNeighborhoodsDisplayName), std::string(slug) },
        { std::string(DbSchema::kNeighborhoodsRegion),
          std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
    });
}

std::set<std::string> Ids(const KeyValueTableArray& rows) {
    std::set<std::string> ids;
    for (const KeyValueTable& row : rows) {
        ids.insert(row.at(std::string(DbSchema::kVenuesId)));
    }
    return ids;
}

TEST(VenuesTest, AddAndGet) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGet", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Venues venues(databaseHelper);
        int64_t neighborhoodId = AddNeighborhood(transaction, databaseHelper, "capitol-hill");

        KeyValueTable values = MakeVenue("Kremwerk");
        values[std::string(DbSchema::kVenuesAddress)] = "1809 Minor Ave";
        values[std::string(DbSchema::kVenuesNeighborhoodId)] = StringFromInt(neighborhoodId);
        values[std::string(DbSchema::kVenuesLatitude)] = "47.6162";
        int64_t id = venues.AddVenue(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable row = venues.GetVenue(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesName)), "Kremwerk");
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesAddress)), "1809 Minor Ave");
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesNeighborhoodId)),
                  StringFromInt(neighborhoodId));
        // The freshness defaults (GD1 §5).
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesStatus)), "open");
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesVolatilityTier)), "monthly");
        // Never-verified and no longitude given: NULL, read back as "".
        EXPECT_TRUE(row.at(std::string(DbSchema::kVenuesLastVerifiedAtUs)).empty());
        EXPECT_TRUE(row.at(std::string(DbSchema::kVenuesLongitude)).empty());
    });
}

TEST(VenuesTest, GetVenueNotFoundIsEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetVenueNotFoundIsEmpty", [&](Transaction& transaction) {
        Venues venues(testDb.GetDatabaseHelper());
        EXPECT_TRUE(venues.GetVenue(transaction, 999999).empty());
    });
}

TEST(VenuesTest, GetVenuesByIdsReturnsTheKnownOnes) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetVenuesByIdsReturnsTheKnownOnes", [&](Transaction& transaction) {
        Venues venues(testDb.GetDatabaseHelper());
        int64_t first = venues.AddVenue(transaction, MakeVenue("Neighbours"));
        int64_t second = venues.AddVenue(transaction, MakeVenue("Wildrose"));
        int64_t unasked = venues.AddVenue(transaction, MakeVenue("Pony"));

        KeyValueTableArray rows =
            venues.GetVenuesByIds(transaction, { second, first, 999999 });
        ASSERT_EQ(rows.size(), 2u);
        // Ordered by id, whatever order they were asked for in.
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kVenuesId)), StringFromInt(first));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kVenuesId)), StringFromInt(second));
        EXPECT_EQ(Ids(rows).count(StringFromInt(unasked)), 0u);

        EXPECT_TRUE(venues.GetVenuesByIds(transaction, {}).empty());
    });
}

TEST(VenuesTest, GetVenuesByNeighborhoodFiltersAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetVenuesByNeighborhoodFiltersAndSortsByName", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Venues venues(databaseHelper);
        int64_t capitolHill = AddNeighborhood(transaction, databaseHelper, "capitol-hill");
        int64_t fremont = AddNeighborhood(transaction, databaseHelper, "fremont");

        auto addIn = [&](std::string_view name, int64_t neighborhoodId) {
            KeyValueTable values = MakeVenue(name);
            values[std::string(DbSchema::kVenuesNeighborhoodId)] = StringFromInt(neighborhoodId);
            return venues.AddVenue(transaction, values);
        };
        int64_t wildrose = addIn("Wildrose", capitolHill);
        int64_t neighbours = addIn("Neighbours", capitolHill);
        addIn("Fremont Abbey", fremont);
        venues.AddVenue(transaction, MakeVenue("Nowhere in particular"));

        KeyValueTableArray rows = venues.GetVenuesByNeighborhood(transaction, capitolHill);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kVenuesId)), StringFromInt(neighbours));
        EXPECT_EQ(rows[1].at(std::string(DbSchema::kVenuesId)), StringFromInt(wildrose));
    });
}

TEST(VenuesTest, UpdateBumpsUpdatedUsAndCanClearAColumn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearAColumn", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Venues venues(databaseHelper);
        int64_t neighborhoodId = AddNeighborhood(transaction, databaseHelper, "fremont");
        KeyValueTable values = MakeVenue("Fremont Abbey");
        values[std::string(DbSchema::kVenuesNeighborhoodId)] = StringFromInt(neighborhoodId);
        // Inserted with updated_us = 0 so the bump is observable: now_us() is
        // fixed for the whole transaction, so "after >= before" proves nothing.
        values[std::string(DbSchema::kVenuesUpdatedUs)] = "0";
        int64_t id = venues.AddVenue(transaction, values);

        venues.UpdateVenue(transaction, id, {
            { std::string(DbSchema::kVenuesStatus), "closed" },
            { std::string(DbSchema::kVenuesNeighborhoodId), "NULL" },
        });

        KeyValueTable row = venues.GetVenue(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesStatus)), "closed");
        EXPECT_TRUE(row.at(std::string(DbSchema::kVenuesNeighborhoodId)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kVenuesUpdatedUs))), 0);
        // Untouched columns stay put.
        EXPECT_EQ(row.at(std::string(DbSchema::kVenuesName)), "Fremont Abbey");
    });
}

// The behavior pin: deletes cascade, so deleting a venue deletes its events.
// This is why closing a venue is a status change, never a delete.
TEST(VenuesTest, DeleteRemovesTheVenueAndCascadesToItsEvents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "DeleteRemovesTheVenueAndCascadesToItsEvents", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Venues venues(databaseHelper);
        Events events(databaseHelper);
        int64_t venueId = venues.AddVenue(transaction, MakeVenue("Short-lived"));
        int64_t eventId = events.AddEvent(transaction, {
            { std::string(DbSchema::kEventsTitle), "Opening night" },
            { std::string(DbSchema::kEventsStartsAtUs), "1000" },
            { std::string(DbSchema::kEventsVenueId), StringFromInt(venueId) },
        });

        venues.DeleteVenue(transaction, venueId);
        EXPECT_TRUE(venues.GetVenue(transaction, venueId).empty());
        EXPECT_TRUE(events.GetEvent(transaction, eventId).empty());
    });
}

}  // namespace
}  // namespace TableHelpers
