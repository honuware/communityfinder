#include "venue_profile_scene_tag_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/scene_tags.h"
#include "db_schema/venue_profile_scene_tag_assignments.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "scene_tags.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"
#include "venue_profiles.h"
#include "venues.h"

// The representative junction test (GD2 §2.5): the other three guide
// assignment helpers share this shape and get one round-trip test each.
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

int64_t AddProfile(Transaction& transaction, DatabaseHelper databaseHelper,
                   std::string_view slug) {
    Venues venues(databaseHelper);
    int64_t venueId = venues.AddVenue(transaction, {
        { std::string(DbSchema::kVenuesName), std::string(slug) } });
    VenueProfiles profiles(databaseHelper);
    return profiles.AddProfile(transaction, {
        { std::string(DbSchema::kVenueProfilesVenueId), StringFromInt(venueId) },
        { std::string(DbSchema::kVenueProfilesSlug), std::string(slug) },
    });
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(kVenueProfileSceneTagAssignmentsSceneTagSlug)));
    }
    return slugs;
}

TEST(VenueProfileSceneTagAssignmentsTest, AssignAndGetForVenueProfileInDisplayOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "AssignAndGetForVenueProfileInDisplayOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfileSceneTagAssignments assignments(databaseHelper);
        int64_t profileId = AddProfile(transaction, databaseHelper, "the-cuff");
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 30);
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);

        EXPECT_GT(assignments.Assign(transaction, profileId, leather), 0);
        assignments.Assign(transaction, profileId, bears);

        KeyValueTableArray rows = assignments.GetForVenueProfile(transaction, profileId);
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "bears", "leather" }));
        EXPECT_EQ(rows[0].at(std::string(DbSchema::kVenueProfileSceneTagAssignmentsSceneTagId)),
                  StringFromInt(bears));
    });
}

TEST(VenueProfileSceneTagAssignmentsTest, GetForVenueProfilesBatchesByProfile) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetForVenueProfilesBatchesByProfile", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfileSceneTagAssignments assignments(databaseHelper);
        int64_t first = AddProfile(transaction, databaseHelper, "first");
        int64_t second = AddProfile(transaction, databaseHelper, "second");
        int64_t unasked = AddProfile(transaction, databaseHelper, "unasked");
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        assignments.Assign(transaction, second, leather);
        assignments.Assign(transaction, second, bears);
        assignments.Assign(transaction, first, leather);
        assignments.Assign(transaction, unasked, bears);

        KeyValueTableArray rows = assignments.GetForVenueProfiles(transaction, { second, first });
        ASSERT_EQ(rows.size(), 3u);
        const std::string profileIdColumn(
            DbSchema::kVenueProfileSceneTagAssignmentsVenueProfileId);
        EXPECT_EQ(rows[0].at(profileIdColumn), StringFromInt(first));
        EXPECT_EQ(rows[1].at(profileIdColumn), StringFromInt(second));
        EXPECT_EQ(rows[2].at(profileIdColumn), StringFromInt(second));
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "leather", "bears", "leather" }));

        EXPECT_TRUE(assignments.GetForVenueProfiles(transaction, {}).empty());
    });
}

TEST(VenueProfileSceneTagAssignmentsTest, GetVenueProfileIdsForSceneTagAscends) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetVenueProfileIdsForSceneTagAscends", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfileSceneTagAssignments assignments(databaseHelper);
        int64_t first = AddProfile(transaction, databaseHelper, "first");
        int64_t second = AddProfile(transaction, databaseHelper, "second");
        int64_t untagged = AddProfile(transaction, databaseHelper, "untagged");
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        assignments.Assign(transaction, second, bears);
        assignments.Assign(transaction, first, bears);
        assignments.Assign(transaction, untagged, leather);

        EXPECT_EQ(assignments.GetVenueProfileIdsForSceneTag(transaction, bears),
                  (std::vector<int64_t>{ first, second }));
        EXPECT_TRUE(assignments.GetVenueProfileIdsForSceneTag(
            transaction, AddSceneTag(transaction, databaseHelper, "unused", 30)).empty());
    });
}

TEST(VenueProfileSceneTagAssignmentsTest, RemoveDeletesOnlyThatPair) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("RemoveDeletesOnlyThatPair", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfileSceneTagAssignments assignments(databaseHelper);
        int64_t profileId = AddProfile(transaction, databaseHelper, "the-cuff");
        int64_t otherProfile = AddProfile(transaction, databaseHelper, "other");
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);
        int64_t leather = AddSceneTag(transaction, databaseHelper, "leather", 20);
        assignments.Assign(transaction, profileId, bears);
        assignments.Assign(transaction, profileId, leather);
        assignments.Assign(transaction, otherProfile, leather);

        assignments.Remove(transaction, profileId, leather);
        // Removing a pair that isn't there is a no-op.
        assignments.Remove(transaction, profileId, leather);

        EXPECT_EQ(Slugs(assignments.GetForVenueProfile(transaction, profileId)),
                  (std::vector<std::string>{ "bears" }));
        EXPECT_EQ(Slugs(assignments.GetForVenueProfile(transaction, otherProfile)),
                  (std::vector<std::string>{ "leather" }));
    });
}

TEST(VenueProfileSceneTagAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        VenueProfileSceneTagAssignments assignments(databaseHelper);
        int64_t profileId = AddProfile(transaction, databaseHelper, "the-cuff");
        int64_t bears = AddSceneTag(transaction, databaseHelper, "bears", 10);
        assignments.Assign(transaction, profileId, bears);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, profileId, bears), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
