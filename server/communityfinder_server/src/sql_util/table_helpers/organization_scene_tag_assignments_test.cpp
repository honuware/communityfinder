#include "organization_scene_tag_assignments.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/organization_scene_tag_assignments.h"
#include "db_schema/organizations.h"
#include "db_schema/scene_tags.h"
#include "organizations.h"
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

int64_t AddOrganization(Transaction& transaction, DatabaseHelper databaseHelper,
                        std::string_view slug) {
    Organizations organizations(databaseHelper);
    return organizations.AddOrganization(transaction, {
        { std::string(DbSchema::kOrganizationsName), std::string(slug) },
        { std::string(DbSchema::kOrganizationsSlug), std::string(slug) },
        { std::string(DbSchema::kOrganizationsOrgKind),
          std::string(DbSchema::kOrganizationsOrgKindSportsLeague) },
    });
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(kOrganizationSceneTagAssignmentsSceneTagSlug)));
    }
    return slugs;
}

TEST(OrganizationSceneTagAssignmentsTest, AssignReadBatchAndRemove) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AssignReadBatchAndRemove", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        OrganizationSceneTagAssignments assignments(databaseHelper);
        int64_t league = AddOrganization(transaction, databaseHelper, "league");
        int64_t chorus = AddOrganization(transaction, databaseHelper, "chorus");
        int64_t sports = AddSceneTag(transaction, databaseHelper, "sports", 20);
        int64_t queer = AddSceneTag(transaction, databaseHelper, "queer", 10);

        EXPECT_GT(assignments.Assign(transaction, league, sports), 0);
        assignments.Assign(transaction, league, queer);
        assignments.Assign(transaction, chorus, queer);

        EXPECT_EQ(Slugs(assignments.GetForOrganization(transaction, league)),
                  (std::vector<std::string>{ "queer", "sports" }));
        // league was added first, so its id is the lower one.
        KeyValueTableArray batch = assignments.GetForOrganizations(transaction, { chorus, league });
        ASSERT_EQ(batch.size(), 3u);
        EXPECT_EQ(batch[0].at(std::string(DbSchema::kOrganizationSceneTagAssignmentsOrganizationId)),
                  StringFromInt(league));
        EXPECT_EQ(batch[2].at(std::string(DbSchema::kOrganizationSceneTagAssignmentsOrganizationId)),
                  StringFromInt(chorus));
        EXPECT_TRUE(assignments.GetForOrganizations(transaction, {}).empty());
        EXPECT_EQ(assignments.GetOrganizationIdsForSceneTag(transaction, queer),
                  (std::vector<int64_t>{ league, chorus }));

        assignments.Remove(transaction, league, queer);
        EXPECT_EQ(Slugs(assignments.GetForOrganization(transaction, league)),
                  (std::vector<std::string>{ "sports" }));
        EXPECT_EQ(assignments.GetOrganizationIdsForSceneTag(transaction, queer),
                  (std::vector<int64_t>{ chorus }));
    });
}

TEST(OrganizationSceneTagAssignmentsTest, DuplicatePairThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicatePairThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        OrganizationSceneTagAssignments assignments(databaseHelper);
        int64_t league = AddOrganization(transaction, databaseHelper, "league");
        int64_t sports = AddSceneTag(transaction, databaseHelper, "sports", 10);
        assignments.Assign(transaction, league, sports);
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(assignments.Assign(transaction, league, sports), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
