#include "places.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/freshness.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/places.h"
#include "neighborhoods.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakePlace(std::string_view name, std::string_view slug,
                        std::string_view placeKind) {
    return {
        { std::string(DbSchema::kPlacesName), std::string(name) },
        { std::string(DbSchema::kPlacesSlug), std::string(slug) },
        { std::string(DbSchema::kPlacesPlaceKind), std::string(placeKind) },
    };
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

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(DbSchema::kPlacesSlug)));
    }
    return slugs;
}

TEST(PlacesTest, AddAndGetByIdAndSlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetByIdAndSlug", [&](Transaction& transaction) {
        Places places(testDb.GetDatabaseHelper());
        KeyValueTable values = MakePlace(
            "Denny Blaine Park", "denny-blaine", DbSchema::kPlacesPlaceKindParkBeach);
        values[std::string(DbSchema::kPlacesAdult)] = "true";
        // A place in the news runs monthly while it lasts: data, not schema.
        values[std::string(DbSchema::kPlacesVolatilityTier)] =
            std::string(DbSchema::kVolatilityTierMonthly);
        int64_t id = places.AddPlace(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable byId = places.GetPlace(transaction, id);
        ASSERT_FALSE(byId.empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kPlacesAdult)), "t");
        EXPECT_EQ(byId.at(std::string(DbSchema::kPlacesVolatilityTier)),
                  DbSchema::kVolatilityTierMonthly);
        EXPECT_EQ(byId.at(std::string(DbSchema::kPlacesStatus)), DbSchema::kFreshnessStatusOpen);
        EXPECT_TRUE(byId.at(std::string(DbSchema::kPlacesLastVerifiedAtUs)).empty());

        KeyValueTable bySlug = places.GetPlaceBySlug(transaction, "denny-blaine");
        ASSERT_FALSE(bySlug.empty());
        EXPECT_EQ(bySlug.at(std::string(DbSchema::kPlacesId)), StringFromInt(id));
    });
}

TEST(PlacesTest, DefaultsToQuarterlyAndNotAdult) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DefaultsToQuarterlyAndNotAdult", [&](Transaction& transaction) {
        Places places(testDb.GetDatabaseHelper());
        int64_t id = places.AddPlace(transaction, MakePlace(
            "Cal Anderson Park", "cal-anderson", DbSchema::kPlacesPlaceKindParkBeach));

        KeyValueTable row = places.GetPlace(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kPlacesAdult)), "f");
        EXPECT_EQ(row.at(std::string(DbSchema::kPlacesVolatilityTier)),
                  DbSchema::kVolatilityTierQuarterly);
    });
}

TEST(PlacesTest, MissesAreEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissesAreEmpty", [&](Transaction& transaction) {
        Places places(testDb.GetDatabaseHelper());
        EXPECT_TRUE(places.GetPlace(transaction, 999999).empty());
        EXPECT_TRUE(places.GetPlaceBySlug(transaction, "no-such-place").empty());
    });
}

TEST(PlacesTest, GetPlacesByKindFiltersAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetPlacesByKindFiltersAndSortsByName", [&](Transaction& transaction) {
        Places places(testDb.GetDatabaseHelper());
        places.AddPlace(transaction, MakePlace(
            "Volunteer Park", "volunteer-park", DbSchema::kPlacesPlaceKindParkBeach));
        places.AddPlace(transaction, MakePlace(
            "The Pink Triangle", "pink-triangle", DbSchema::kPlacesPlaceKindHistorical));
        places.AddPlace(transaction, MakePlace(
            "Alki Beach", "alki-beach", DbSchema::kPlacesPlaceKindParkBeach));

        EXPECT_EQ(Slugs(places.GetPlacesByKind(transaction, DbSchema::kPlacesPlaceKindParkBeach)),
                  (std::vector<std::string>{ "alki-beach", "volunteer-park" }));
    });
}

TEST(PlacesTest, GetPlacesByNeighborhoodFiltersAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetPlacesByNeighborhoodFiltersAndSortsByName", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Places places(databaseHelper);
        int64_t capitolHill = AddNeighborhood(transaction, databaseHelper, "capitol-hill");
        int64_t westSeattle = AddNeighborhood(transaction, databaseHelper, "west-seattle");
        auto addIn = [&](std::string_view slug, int64_t neighborhoodId) {
            KeyValueTable values = MakePlace(slug, slug, DbSchema::kPlacesPlaceKindLandmark);
            values[std::string(DbSchema::kPlacesNeighborhoodId)] = StringFromInt(neighborhoodId);
            places.AddPlace(transaction, values);
        };
        addIn("rainbow-crosswalks", capitolHill);
        addIn("cal-anderson", capitolHill);
        addIn("alki-beach", westSeattle);

        EXPECT_EQ(Slugs(places.GetPlacesByNeighborhood(transaction, capitolHill)),
                  (std::vector<std::string>{ "cal-anderson", "rainbow-crosswalks" }));
    });
}

TEST(PlacesTest, GetPlacesLeavesOutClosedAndMovedUnlessAsked) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetPlacesLeavesOutClosedAndMovedUnlessAsked", [&](Transaction& transaction) {
        Places places(testDb.GetDatabaseHelper());
        auto addWithStatus = [&](std::string_view slug, std::string_view status) {
            KeyValueTable values = MakePlace(slug, slug, DbSchema::kPlacesPlaceKindLandmark);
            values[std::string(DbSchema::kPlacesStatus)] = std::string(status);
            places.AddPlace(transaction, values);
        };
        addWithStatus("a-open", DbSchema::kFreshnessStatusOpen);
        addWithStatus("b-seasonal", DbSchema::kFreshnessStatusSeasonal);
        addWithStatus("c-closed", DbSchema::kFreshnessStatusClosed);
        addWithStatus("d-moved", DbSchema::kFreshnessStatusMoved);

        EXPECT_EQ(Slugs(places.GetPlaces(transaction, false)),
                  (std::vector<std::string>{ "a-open", "b-seasonal" }));
        EXPECT_EQ(Slugs(places.GetPlaces(transaction, true)),
                  (std::vector<std::string>{ "a-open", "b-seasonal", "c-closed", "d-moved" }));
    });
}

TEST(PlacesTest, UpdateBumpsUpdatedUsAndCanClearAColumn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearAColumn", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Places places(databaseHelper);
        KeyValueTable values = MakePlace(
            "Howell Park", "howell-park", DbSchema::kPlacesPlaceKindParkBeach);
        values[std::string(DbSchema::kPlacesNeighborhoodId)] =
            StringFromInt(AddNeighborhood(transaction, databaseHelper, "madison-park"));
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kPlacesUpdatedUs)] = "0";
        int64_t id = places.AddPlace(transaction, values);

        places.UpdatePlace(transaction, id, {
            { std::string(DbSchema::kPlacesDescription), "A small wooded park" },
            { std::string(DbSchema::kPlacesNeighborhoodId), "NULL" },
        });

        KeyValueTable row = places.GetPlace(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kPlacesDescription)), "A small wooded park");
        EXPECT_TRUE(row.at(std::string(DbSchema::kPlacesNeighborhoodId)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kPlacesUpdatedUs))), 0);
    });
}

TEST(PlacesTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        Places places(testDb.GetDatabaseHelper());
        places.AddPlace(transaction, MakePlace(
            "One", "same-slug", DbSchema::kPlacesPlaceKindLandmark));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(places.AddPlace(transaction, MakePlace(
                         "Two", "same-slug", DbSchema::kPlacesPlaceKindLandmark)),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
