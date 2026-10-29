#include "organizations.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/freshness.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organizations.h"
#include "neighborhoods.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

KeyValueTable MakeOrganization(std::string_view name, std::string_view slug,
                               std::string_view orgKind) {
    return {
        { std::string(DbSchema::kOrganizationsName), std::string(name) },
        { std::string(DbSchema::kOrganizationsSlug), std::string(slug) },
        { std::string(DbSchema::kOrganizationsOrgKind), std::string(orgKind) },
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
        slugs.push_back(row.at(std::string(DbSchema::kOrganizationsSlug)));
    }
    return slugs;
}

TEST(OrganizationsTest, AddAndGetByIdAndSlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetByIdAndSlug", [&](Transaction& transaction) {
        Organizations organizations(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeOrganization(
            "Seattle Frontrunners", "seattle-frontrunners",
            DbSchema::kOrganizationsOrgKindSportsLeague);
        values[std::string(DbSchema::kOrganizationsMeetingCadenceText)] =
            "Saturdays 9 AM, Green Lake";
        int64_t id = organizations.AddOrganization(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable byId = organizations.GetOrganization(transaction, id);
        ASSERT_FALSE(byId.empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kOrganizationsMeetingCadenceText)),
                  "Saturdays 9 AM, Green Lake");
        // The freshness defaults (GD1 §5) and a countywide neighborhood.
        EXPECT_EQ(byId.at(std::string(DbSchema::kOrganizationsStatus)),
                  DbSchema::kFreshnessStatusOpen);
        EXPECT_EQ(byId.at(std::string(DbSchema::kOrganizationsVolatilityTier)),
                  DbSchema::kVolatilityTierAnnual);
        EXPECT_TRUE(byId.at(std::string(DbSchema::kOrganizationsLastVerifiedAtUs)).empty());
        EXPECT_TRUE(byId.at(std::string(DbSchema::kOrganizationsNeighborhoodId)).empty());
        // Not 18+ unless a person says so.
        EXPECT_EQ(byId.at(std::string(DbSchema::kOrganizationsAdult)), "f");

        KeyValueTable bySlug = organizations.GetOrganizationBySlug(
            transaction, "seattle-frontrunners");
        ASSERT_FALSE(bySlug.empty());
        EXPECT_EQ(bySlug.at(std::string(DbSchema::kOrganizationsId)), StringFromInt(id));
    });
}

TEST(OrganizationsTest, MissesAreEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissesAreEmpty", [&](Transaction& transaction) {
        Organizations organizations(testDb.GetDatabaseHelper());
        EXPECT_TRUE(organizations.GetOrganization(transaction, 999999).empty());
        EXPECT_TRUE(organizations.GetOrganizationBySlug(transaction, "no-such-org").empty());
        EXPECT_TRUE(organizations.GetOrganizationsByKind(
            transaction, DbSchema::kOrganizationsOrgKindFaith).empty());
    });
}

TEST(OrganizationsTest, GetOrganizationsByKindFiltersAndSortsByName) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetOrganizationsByKindFiltersAndSortsByName", [&](Transaction& transaction) {
        Organizations organizations(testDb.GetDatabaseHelper());
        organizations.AddOrganization(transaction, MakeOrganization(
            "Seattle Men's Chorus", "smc", DbSchema::kOrganizationsOrgKindChorusPerformance));
        organizations.AddOrganization(transaction, MakeOrganization(
            "Emerald City Softball", "ecsa", DbSchema::kOrganizationsOrgKindSportsLeague));
        organizations.AddOrganization(transaction, MakeOrganization(
            "Cascade Volleyball", "cva", DbSchema::kOrganizationsOrgKindSportsLeague));

        EXPECT_EQ(Slugs(organizations.GetOrganizationsByKind(
                      transaction, DbSchema::kOrganizationsOrgKindSportsLeague)),
                  (std::vector<std::string>{ "cva", "ecsa" }));
    });
}

TEST(OrganizationsTest, GetOrganizationsByNeighborhoodLeavesOutCountywide) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetOrganizationsByNeighborhoodLeavesOutCountywide", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        Organizations organizations(databaseHelper);
        int64_t capitolHill = AddNeighborhood(transaction, databaseHelper, "capitol-hill");
        int64_t fremont = AddNeighborhood(transaction, databaseHelper, "fremont");
        auto addIn = [&](std::string_view slug, int64_t neighborhoodId) {
            KeyValueTable values = MakeOrganization(
                slug, slug, DbSchema::kOrganizationsOrgKindClubHobby);
            values[std::string(DbSchema::kOrganizationsNeighborhoodId)] =
                StringFromInt(neighborhoodId);
            organizations.AddOrganization(transaction, values);
        };
        addIn("knitting-circle", capitolHill);
        addIn("board-games", capitolHill);
        addIn("fremont-book-club", fremont);
        organizations.AddOrganization(transaction, MakeOrganization(
            "Countywide", "countywide", DbSchema::kOrganizationsOrgKindClubHobby));

        EXPECT_EQ(Slugs(organizations.GetOrganizationsByNeighborhood(transaction, capitolHill)),
                  (std::vector<std::string>{ "board-games", "knitting-circle" }));
    });
}

TEST(OrganizationsTest, GetOrganizationsLeavesOutClosedAndMovedUnlessAsked) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "GetOrganizationsLeavesOutClosedAndMovedUnlessAsked", [&](Transaction& transaction) {
        Organizations organizations(testDb.GetDatabaseHelper());
        auto addWithStatus = [&](std::string_view slug, std::string_view status) {
            KeyValueTable values = MakeOrganization(
                slug, slug, DbSchema::kOrganizationsOrgKindSocialMeetup);
            values[std::string(DbSchema::kOrganizationsStatus)] = std::string(status);
            organizations.AddOrganization(transaction, values);
        };
        addWithStatus("a-open", DbSchema::kFreshnessStatusOpen);
        addWithStatus("b-seasonal", DbSchema::kFreshnessStatusSeasonal);
        addWithStatus("c-unverified", DbSchema::kFreshnessStatusUnverified);
        addWithStatus("d-closed", DbSchema::kFreshnessStatusClosed);
        addWithStatus("e-moved", DbSchema::kFreshnessStatusMoved);

        EXPECT_EQ(Slugs(organizations.GetOrganizations(transaction, false)),
                  (std::vector<std::string>{ "a-open", "b-seasonal", "c-unverified" }));
        EXPECT_EQ(Slugs(organizations.GetOrganizations(transaction, true)),
                  (std::vector<std::string>{
                      "a-open", "b-seasonal", "c-unverified", "d-closed", "e-moved" }));
    });
}

TEST(OrganizationsTest, UpdateBumpsUpdatedUsAndCanClearAColumn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearAColumn", [&](Transaction& transaction) {
        Organizations organizations(testDb.GetDatabaseHelper());
        KeyValueTable values = MakeOrganization(
            "Dykes on Bikes", "dykes-on-bikes", DbSchema::kOrganizationsOrgKindClubHobby);
        values[std::string(DbSchema::kOrganizationsClosureNote)] = "A note to clear";
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kOrganizationsUpdatedUs)] = "0";
        int64_t id = organizations.AddOrganization(transaction, values);

        organizations.UpdateOrganization(transaction, id, {
            { std::string(DbSchema::kOrganizationsLastVerifiedAtUs), "1700000000000000" },
            { std::string(DbSchema::kOrganizationsClosureNote), "NULL" },
        });

        KeyValueTable row = organizations.GetOrganization(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kOrganizationsLastVerifiedAtUs)),
                  "1700000000000000");
        EXPECT_TRUE(row.at(std::string(DbSchema::kOrganizationsClosureNote)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kOrganizationsUpdatedUs))), 0);
        EXPECT_EQ(row.at(std::string(DbSchema::kOrganizationsName)), "Dykes on Bikes");
    });
}

TEST(OrganizationsTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        Organizations organizations(testDb.GetDatabaseHelper());
        organizations.AddOrganization(transaction, MakeOrganization(
            "One", "same-slug", DbSchema::kOrganizationsOrgKindFaith));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(organizations.AddOrganization(transaction, MakeOrganization(
                         "Two", "same-slug", DbSchema::kOrganizationsOrgKindFaith)),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
