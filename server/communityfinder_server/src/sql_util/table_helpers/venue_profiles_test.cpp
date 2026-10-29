#include "venue_profiles.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"
#include "venues.h"

namespace TableHelpers {
namespace {

int64_t AddVenue(Transaction& transaction, DatabaseHelper databaseHelper,
                 std::string_view name) {
    Venues venues(databaseHelper);
    return venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), std::string(name) } });
}

KeyValueTable MakeProfile(int64_t venueId, std::string_view slug) {
    return {
        { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(venueId) },
        { std::string(DbSchema::kVenueProfilesSlug), std::string(slug) },
    };
}

TEST(VenueProfilesTest, AddAndGetByIdVenueAndSlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddAndGetByIdVenueAndSlug", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfiles profiles(databaseHelper);
        int64_t venueId = AddVenue(transaction, databaseHelper, "The Cuff");

        KeyValueTable values = MakeProfile(venueId, "the-cuff");
        values[std::string(DbSchema::kVenueProfilesCrowdVibe)] = "Leather and bears";
        int64_t id = profiles.AddProfile(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable byId = profiles.GetProfile(transaction, id);
        ASSERT_FALSE(byId.empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kVenueProfilesCrowdVibe)), "Leather and bears");
        // The defaults: not adult, quarterly, never verified, nothing to say yet.
        EXPECT_EQ(byId.at(std::string(DbSchema::kVenueProfilesAdult)), "f");
        EXPECT_EQ(byId.at(std::string(DbSchema::kVenueProfilesVolatilityTier)), "quarterly");
        EXPECT_TRUE(byId.at(std::string(DbSchema::kVenueProfilesLastVerifiedAtUs)).empty());
        EXPECT_TRUE(byId.at(std::string(DbSchema::kVenueProfilesPriceBand)).empty());
        EXPECT_TRUE(byId.at(std::string(DbSchema::kVenueProfilesClosureNote)).empty());

        KeyValueTable byVenue = profiles.GetProfileByVenueId(transaction, venueId);
        ASSERT_FALSE(byVenue.empty());
        EXPECT_EQ(byVenue.at(std::string(DbSchema::kVenueProfilesId)), StringFromInt(id));

        KeyValueTable bySlug = profiles.GetProfileBySlug(transaction, "the-cuff");
        ASSERT_FALSE(bySlug.empty());
        EXPECT_EQ(bySlug.at(std::string(DbSchema::kVenueProfilesId)), StringFromInt(id));
    });
}

TEST(VenueProfilesTest, MissesAreEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissesAreEmpty", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfiles profiles(databaseHelper);
        // A venue with no profile yet.
        int64_t venueId = AddVenue(transaction, databaseHelper, "Unprofiled");

        EXPECT_TRUE(profiles.GetProfile(transaction, 999999).empty());
        EXPECT_TRUE(profiles.GetProfileByVenueId(transaction, venueId).empty());
        EXPECT_TRUE(profiles.GetProfileBySlug(transaction, "no-such-profile").empty());
    });
}

TEST(VenueProfilesTest, GetProfilesIsOrderedBySlug) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetProfilesIsOrderedBySlug", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfiles profiles(databaseHelper);
        profiles.AddProfile(transaction,
            MakeProfile(AddVenue(transaction, databaseHelper, "Wildrose"), "wildrose"));
        profiles.AddProfile(transaction,
            MakeProfile(AddVenue(transaction, databaseHelper, "Kremwerk"), "kremwerk"));

        std::vector<std::string> slugs;
        for (const KeyValueTable& row : profiles.GetProfiles(transaction)) {
            slugs.push_back(row.at(std::string(DbSchema::kVenueProfilesSlug)));
        }
        EXPECT_EQ(slugs, (std::vector<std::string>{ "kremwerk", "wildrose" }));
    });
}

TEST(VenueProfilesTest, UpdateBumpsUpdatedUsAndCanClearAColumn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "UpdateBumpsUpdatedUsAndCanClearAColumn", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfiles profiles(databaseHelper);
        KeyValueTable values =
            MakeProfile(AddVenue(transaction, databaseHelper, "R Place"), "r-place");
        values[std::string(DbSchema::kVenueProfilesPriceBand)] = "$$";
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kVenueProfilesUpdatedUs)] = "0";
        int64_t id = profiles.AddProfile(transaction, values);

        profiles.UpdateProfile(transaction, id, {
            { std::string(DbSchema::kVenueProfilesClosureNote), "Closed 2023; the lease ran out" },
            { std::string(DbSchema::kVenueProfilesPriceBand), "NULL" },
        });

        KeyValueTable row = profiles.GetProfile(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kVenueProfilesClosureNote)),
                  "Closed 2023; the lease ran out");
        EXPECT_TRUE(row.at(std::string(DbSchema::kVenueProfilesPriceBand)).empty());
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kVenueProfilesUpdatedUs))), 0);
        EXPECT_EQ(row.at(std::string(DbSchema::kVenueProfilesSlug)), "r-place");
    });
}

// Interface Contract 2: a venue has at most one profile.
TEST(VenueProfilesTest, SecondProfileForAVenueThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SecondProfileForAVenueThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfiles profiles(databaseHelper);
        int64_t venueId = AddVenue(transaction, databaseHelper, "The Cuff");
        profiles.AddProfile(transaction, MakeProfile(venueId, "the-cuff"));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(profiles.AddProfile(transaction, MakeProfile(venueId, "the-cuff-2")),
                     std::exception);
    });
}

TEST(VenueProfilesTest, DuplicateSlugThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfiles profiles(databaseHelper);
        profiles.AddProfile(transaction,
            MakeProfile(AddVenue(transaction, databaseHelper, "One"), "same-slug"));
        int64_t otherVenue = AddVenue(transaction, databaseHelper, "Two");
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(profiles.AddProfile(transaction, MakeProfile(otherVenue, "same-slug")),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
