#include "service_listings.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/freshness.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/service_listings.h"
#include "neighborhoods.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeListing(std::string_view name, std::string_view slug,
                          std::string_view serviceKind) {
    return {
        { std::string(DbSchema::kServiceListingsName), std::string(name) },
        { std::string(DbSchema::kServiceListingsSlug), std::string(slug) },
        { std::string(DbSchema::kServiceListingsServiceKind), std::string(serviceKind) },
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
        slugs.push_back(row.at(std::string(DbSchema::kServiceListingsSlug)));
    }
    return slugs;
}

TEST(ServiceListingsTest, AddAndGetByIdAndSlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetByIdAndSlug", [&](Transaction& transaction) {
        ServiceListings listings(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeListing(
            "Example Clinic", "example-clinic", DbSchema::kServiceListingsServiceKindStiTesting);
        values[std::string(DbSchema::kServiceListingsPhone)] = "206-555-0100";
        int64_t id = listings.AddServiceListing(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable byId = listings.GetServiceListing(transaction, id);
        ASSERT_FALSE(byId.empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kServiceListingsPhone)), "206-555-0100");
        EXPECT_EQ(byId.at(std::string(DbSchema::kServiceListingsStatus)),
                  DbSchema::kFreshnessStatusOpen);
        EXPECT_TRUE(byId.at(std::string(DbSchema::kServiceListingsLastVerifiedAtUs)).empty());

        KeyValueTable bySlug = listings.GetServiceListingBySlug(transaction, "example-clinic");
        ASSERT_FALSE(bySlug.empty());
        EXPECT_EQ(bySlug.at(std::string(DbSchema::kServiceListingsId)), StringFromInt(id));
    });
}

// GD2 OQ9 (a) and (b): health care unless a person says otherwise, re-checked
// semiannually; an everyday service re-checks annually; an explicit cadence wins.
TEST(ServiceListingsTest, CadenceFollowsTheHealthFlag) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CadenceFollowsTheHealthFlag", [&](Transaction& transaction) {
        ServiceListings listings(testDb.GetDatabaseHelper());
        const std::string isHealth(DbSchema::kServiceListingsIsHealth);
        const std::string tier(DbSchema::kServiceListingsVolatilityTier);

        int64_t unsaid = listings.AddServiceListing(transaction, MakeListing(
            "Clinic", "clinic", DbSchema::kServiceListingsServiceKindPrep));

        KeyValueTable everydayValues = MakeListing(
            "Barber", "barber", DbSchema::kServiceListingsServiceKindEverydayService);
        everydayValues[isHealth] = "false";
        int64_t everyday = listings.AddServiceListing(transaction, everydayValues);

        KeyValueTable explicitValues = MakeListing(
            "Dry Cleaner", "dry-cleaner", DbSchema::kServiceListingsServiceKindEverydayService);
        explicitValues[isHealth] = "f";
        explicitValues[tier] = std::string(DbSchema::kVolatilityTierQuarterly);
        int64_t explicitTier = listings.AddServiceListing(transaction, explicitValues);

        KeyValueTable unsaidRow = listings.GetServiceListing(transaction, unsaid);
        EXPECT_EQ(unsaidRow.at(isHealth), "t");
        EXPECT_EQ(unsaidRow.at(tier), DbSchema::kVolatilityTierSemiannual);

        KeyValueTable everydayRow = listings.GetServiceListing(transaction, everyday);
        EXPECT_EQ(everydayRow.at(isHealth), "f");
        EXPECT_EQ(everydayRow.at(tier), DbSchema::kVolatilityTierAnnual);

        EXPECT_EQ(listings.GetServiceListing(transaction, explicitTier).at(tier),
                  DbSchema::kVolatilityTierQuarterly);
    });
}

TEST(ServiceListingsTest, MissesAreEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissesAreEmpty", [&](Transaction& transaction) {
        ServiceListings listings(testDb.GetDatabaseHelper());
        EXPECT_TRUE(listings.GetServiceListing(transaction, 999999).empty());
        EXPECT_TRUE(listings.GetServiceListingBySlug(transaction, "no-such-listing").empty());
    });
}

TEST(ServiceListingsTest, GetServiceListingsByKindFiltersAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetServiceListingsByKindFiltersAndSortsByName", [&](Transaction& transaction) {
        ServiceListings listings(testDb.GetDatabaseHelper());
        listings.AddServiceListing(transaction, MakeListing(
            "Zeta Dental", "zeta-dental", DbSchema::kServiceListingsServiceKindDental));
        listings.AddServiceListing(transaction, MakeListing(
            "Beta PrEP", "beta-prep", DbSchema::kServiceListingsServiceKindPrep));
        listings.AddServiceListing(transaction, MakeListing(
            "Alpha Dental", "alpha-dental", DbSchema::kServiceListingsServiceKindDental));

        EXPECT_EQ(Slugs(listings.GetServiceListingsByKind(
                      transaction, DbSchema::kServiceListingsServiceKindDental)),
                  (std::vector<std::string>{ "alpha-dental", "zeta-dental" }));
    });
}

TEST(ServiceListingsTest, GetServiceListingsByNeighborhoodFiltersAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetServiceListingsByNeighborhoodFiltersAndSortsByName", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        ServiceListings listings(databaseHelper);
        int64_t capitolHill = AddNeighborhood(transaction, databaseHelper, "capitol-hill");
        int64_t ballard = AddNeighborhood(transaction, databaseHelper, "ballard");
        auto addIn = [&](std::string_view slug, int64_t neighborhoodId) {
            KeyValueTable values = MakeListing(
                slug, slug, DbSchema::kServiceListingsServiceKindPrimaryCare);
            values[std::string(DbSchema::kServiceListingsNeighborhoodId)] =
                StringFromInt(neighborhoodId);
            listings.AddServiceListing(transaction, values);
        };
        addIn("second-clinic", capitolHill);
        addIn("first-clinic", capitolHill);
        addIn("ballard-clinic", ballard);

        EXPECT_EQ(Slugs(listings.GetServiceListingsByNeighborhood(transaction, capitolHill)),
                  (std::vector<std::string>{ "first-clinic", "second-clinic" }));
    });
}

TEST(ServiceListingsTest, GetServiceListingsLeavesOutClosedAndMovedUnlessAsked) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetServiceListingsLeavesOutClosedAndMovedUnlessAsked", [&](Transaction& transaction) {
        ServiceListings listings(testDb.GetDatabaseHelper());
        auto addWithStatus = [&](std::string_view slug, std::string_view status) {
            KeyValueTable values = MakeListing(
                slug, slug, DbSchema::kServiceListingsServiceKindMentalHealth);
            values[std::string(DbSchema::kServiceListingsStatus)] = std::string(status);
            listings.AddServiceListing(transaction, values);
        };
        addWithStatus("a-open", DbSchema::kFreshnessStatusOpen);
        addWithStatus("b-unverified", DbSchema::kFreshnessStatusUnverified);
        addWithStatus("c-closed", DbSchema::kFreshnessStatusClosed);
        addWithStatus("d-moved", DbSchema::kFreshnessStatusMoved);

        EXPECT_EQ(Slugs(listings.GetServiceListings(transaction, false)),
                  (std::vector<std::string>{ "a-open", "b-unverified" }));
        EXPECT_EQ(Slugs(listings.GetServiceListings(transaction, true)),
                  (std::vector<std::string>{ "a-open", "b-unverified", "c-closed", "d-moved" }));
    });
}

TEST(ServiceListingsTest, UpdateBumpsUpdatedUsAndLeavesTheCadenceAlone) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndLeavesTheCadenceAlone", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        ServiceListings listings(databaseHelper);
        KeyValueTable values = MakeListing(
            "Example Clinic", "example-clinic", DbSchema::kServiceListingsServiceKindHivCare);
        values[std::string(DbSchema::kServiceListingsNeighborhoodId)] =
            StringFromInt(AddNeighborhood(transaction, databaseHelper, "first-hill"));
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kServiceListingsUpdatedUs)] = "0";
        int64_t id = listings.AddServiceListing(transaction, values);

        listings.UpdateServiceListing(transaction, id, {
            { std::string(DbSchema::kServiceListingsIsHealth), "false" },
            { std::string(DbSchema::kServiceListingsNeighborhoodId), "NULL" },
        });

        KeyValueTable row = listings.GetServiceListing(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kServiceListingsIsHealth)), "f");
        EXPECT_EQ(row.at(std::string(DbSchema::kServiceListingsVolatilityTier)),
                  DbSchema::kVolatilityTierSemiannual);
        EXPECT_TRUE(row.at(std::string(DbSchema::kServiceListingsNeighborhoodId)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kServiceListingsUpdatedUs))), 0);
    });
}

TEST(ServiceListingsTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        ServiceListings listings(testDb.GetDatabaseHelper());
        listings.AddServiceListing(transaction, MakeListing(
            "One", "same-slug", DbSchema::kServiceListingsServiceKindDental));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(listings.AddServiceListing(transaction, MakeListing(
                         "Two", "same-slug", DbSchema::kServiceListingsServiceKindDental)),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
