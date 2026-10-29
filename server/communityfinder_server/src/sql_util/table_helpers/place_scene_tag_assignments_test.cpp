#include "place_scene_tag_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/place_scene_tag_assignments.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "places.h"
#include "scene_tags.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// One round trip through every method; the full set of junction cases lives in
// venue_profile_scene_tag_assignments_test.cpp, which shares this shape.
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

int64_t AddPlace(Transaction& transaction, DatabaseHelper databaseHelper,
                 std::string_view slug) {
    Places places(databaseHelper);
    return places.AddPlace(transaction, {
        { std::string(DbSchema::kPlacesName), std::string(slug) },
        { std::string(DbSchema::kPlacesSlug), std::string(slug) },
        { std::string(DbSchema::kPlacesPlaceKind),
          std::string(DbSchema::kPlacesPlaceKindParkBeach) },
    });
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(kPlaceSceneTagAssignmentsSceneTagSlug)));
    }
    return slugs;
}

TEST(PlaceSceneTagAssignmentsTest, AssignReadBatchAndRemove) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignReadBatchAndRemove", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        PlaceSceneTagAssignments assignments(databaseHelper);
        // beach was added first, so its id is the lower one.
        int64_t beach = AddPlace(transaction, databaseHelper, "beach");
        int64_t park = AddPlace(transaction, databaseHelper, "park");
        int64_t outdoors = AddSceneTag(transaction, databaseHelper, "outdoors", 20);
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);

        EXPECT_GT(assignments.Assign(transaction, beach, outdoors), 0);
        assignments.Assign(transaction, beach, queer);
        assignments.Assign(transaction, park, queer);

        EXPECT_EQ(Slugs(assignments.GetForPlace(transaction, beach)),
                  (std::vector<std::string>{ "queer", "outdoors" }));
        KeyValueTableArray batch = assignments.GetForPlaces(transaction, { park, beach });
        ASSERT_EQ(batch.size(), 3u);
        EXPECT_EQ(batch[0].at(std::string(DbSchema::kPlaceSceneTagAssignmentsPlaceId)),
                  StringFromInt(beach));
        EXPECT_EQ(batch[2].at(std::string(DbSchema::kPlaceSceneTagAssignmentsPlaceId)),
                  StringFromInt(park));
        EXPECT_TRUE(assignments.GetForPlaces(transaction, {}).empty());
        EXPECT_EQ(assignments.GetPlaceIdsForSceneTag(transaction, queer),
                  (std::vector<int64_t>{ beach, park }));

        assignments.Remove(transaction, beach, queer);
        EXPECT_EQ(Slugs(assignments.GetForPlace(transaction, beach)),
                  (std::vector<std::string>{ "outdoors" }));
        EXPECT_EQ(assignments.GetPlaceIdsForSceneTag(transaction, queer),
                  (std::vector<int64_t>{ park }));
    });
}

TEST(PlaceSceneTagAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        PlaceSceneTagAssignments assignments(databaseHelper);
        int64_t beach = AddPlace(transaction, databaseHelper, "beach");
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);
        assignments.Assign(transaction, beach, queer);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, beach, queer), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
