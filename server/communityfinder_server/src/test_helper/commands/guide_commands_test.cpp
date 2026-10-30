#include "commands/guide_commands.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/guide/guide_helper.h"
#include "db_schema/freshness.h"
#include "db_schema/guide_links.h"
#include "db_schema/guide_pages.h"
#include "db_schema/neighborhoods.h"
#include "db_schema/organizations.h"
#include "db_schema/places.h"
#include "db_schema/scene_tags.h"
#include "db_schema/service_listings.h"
#include "db_schema/venue_profiles.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/neighborhoods.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/service_listings.h"
#include "sql_util/table_helpers/venue_profiles.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

#include "command_runner.h"

// GD2 §5 — the guide simulation commands, run through the registry against the
// test database. The harness doesn't seed vocabulary, so the demo runs bare
// unless a test adds the slugs it looks for.
namespace TestHelper {
namespace {

constexpr int64_t kUsPerDay = 86'400LL * 1'000'000LL;

// Every table create-demo-guide writes to.
const std::vector<std::string_view> kDemoTables{
    DbSchema::kVenuesTable, DbSchema::kVenueProfilesTable, DbSchema::kOrganizationsTable,
    DbSchema::kPlacesTable, DbSchema::kGuidePagesTable, DbSchema::kGuideLinksTable,
};

int RunCommand(TestHelperContext& ctx, Transaction& transaction, const std::string& command,
        const Args& args = {}) {
    const CommandInfo* info = ctx.registry.Find(command);
    EXPECT_NE(info, nullptr) << command;
    return info != nullptr ? info->execute(ctx, transaction, args) : -1;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

int64_t RowCount(Transaction& transaction, std::string_view table) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        "SELECT count(*) FROM " + std::string(table)));
}

// 0 = never verified.
int64_t LastVerifiedAtUs(Transaction& transaction, std::string_view table, int64_t id) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        "SELECT COALESCE(last_verified_at_us, 0) FROM " + std::string(table)
        + " WHERE id = " + StringFromInt(id)));
}

int64_t VerifiedRowCount(Transaction& transaction, std::string_view table) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue(
        "SELECT count(*) FROM " + std::string(table) + " WHERE last_verified_at_us IS NOT NULL"));
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it != row.end() ? it->second : std::string();
}

int64_t IdColumn(const KeyValueTable& row, std::string_view column) {
    return std::stoll(Column(row, column));
}

template <typename Item>
const Item* FindBySlug(const std::vector<Item>& items, std::string_view slug) {
    for (const Item& item : items) {
        if (item.slug == slug) return &item;
    }
    return nullptr;
}

TEST(GuideCommandsTest, EachCommandIsRegisteredUnderGuideWithItsAlias) {
    CommandRegistry registry;
    RegisterGuideCommands(registry);
    for (const auto& [name, alias] : std::map<std::string, std::string>{
             { "create-demo-guide", "dg" }, { "publish-page", "pp" },
             { "verify-listing", "vl" }, { "list-graveyard", "lg" } }) {
        const CommandInfo* byName = registry.Find(name);
        ASSERT_NE(byName, nullptr) << name;
        EXPECT_EQ(byName->category, "Guide");
        EXPECT_EQ(registry.Find(alias), byName) << alias;
    }
}

TEST(GuideCommandsTest, CreateDemoGuideBuildsEachKindOfListingAndIsIdempotent) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CreateDemoGuideBuildsEachKindOfListingAndIsIdempotent",
                            [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterGuideCommands(ctx.registry);

        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        Guide::GuideHelper helper(databaseHelper);

        // The open venue is in the directory with its link; the closed one is in
        // the graveyard with its note, and keeps its own page.
        std::vector<Guide::GuideVenue> directory = helper.GetVenuesDirectory(transaction, {});
        ASSERT_EQ(directory.size(), 1u);
        EXPECT_EQ(directory[0].slug, "demo-bar");
        EXPECT_EQ(directory[0].priceBand, "$$");
        std::optional<Guide::VenueDetail> bar = helper.GetVenueProfileBySlug(transaction, "demo-bar");
        ASSERT_TRUE(bar.has_value());
        ASSERT_EQ(bar->links.size(), 1u);
        EXPECT_EQ(bar->links[0].url, "https://example.org/demo-bar");
        EXPECT_EQ(bar->links[0].platform, DbSchema::kGuideLinksPlatformWebsite);

        Guide::Graveyard graveyard = helper.GetGraveyard(transaction);
        ASSERT_EQ(graveyard.venues.size(), 1u);
        EXPECT_EQ(graveyard.venues[0].slug, "demo-closed-bar");
        EXPECT_FALSE(graveyard.venues[0].closureNote.empty());
        EXPECT_TRUE(graveyard.organizations.empty());
        EXPECT_TRUE(helper.GetVenueProfileBySlug(transaction, "demo-closed-bar").has_value());

        // Two organizations of different kinds.
        std::vector<Guide::GuideOrganization> organizations =
            helper.GetOrganizationsDirectory(transaction, {});
        ASSERT_EQ(organizations.size(), 2u);
        const Guide::GuideOrganization* league = FindBySlug(organizations, "demo-softball-league");
        const Guide::GuideOrganization* chorus = FindBySlug(organizations, "demo-chorus");
        ASSERT_NE(league, nullptr);
        ASSERT_NE(chorus, nullptr);
        EXPECT_EQ(league->orgKind, DbSchema::kOrganizationsOrgKindSportsLeague);
        EXPECT_EQ(chorus->orgKind, DbSchema::kOrganizationsOrgKindChorusPerformance);

        // The 18+ place, re-checked monthly.
        std::optional<Guide::PlaceDetail> place = helper.GetPlaceBySlug(transaction, "demo-denny-blaine");
        ASSERT_TRUE(place.has_value());
        EXPECT_TRUE(place->place.adult);
        EXPECT_EQ(place->place.placeKind, DbSchema::kPlacesPlaceKindParkBeach);
        KeyValueTable placeRow =
            TableHelpers::Places(databaseHelper).GetPlaceBySlug(transaction, "demo-denny-blaine");
        EXPECT_EQ(Column(placeRow, DbSchema::kPlacesVolatilityTier), DbSchema::kVolatilityTierMonthly);

        // One page public, one draft only the preview path sees.
        EXPECT_TRUE(helper.GetPublishedGuidePage(transaction, "demo-published-page").has_value());
        EXPECT_FALSE(helper.GetPublishedGuidePage(transaction, "demo-draft-page").has_value());
        std::optional<Guide::GuidePage> draft = helper.GetGuidePageAnyState(transaction, "demo-draft-page");
        ASSERT_TRUE(draft.has_value());
        EXPECT_TRUE(draft->draft);

        // Nothing is tagged: the test database has no vocabulary.
        EXPECT_TRUE(directory[0].sceneTagSlugs.empty());
        EXPECT_TRUE(directory[0].neighborhoodSlug.empty());

        // A re-run finds every row by slug and writes nothing.
        std::map<std::string_view, int64_t> counts;
        for (std::string_view table : kDemoTables) counts[table] = RowCount(transaction, table);
        EXPECT_EQ(counts[DbSchema::kVenuesTable], 2);
        EXPECT_EQ(counts[DbSchema::kGuidePagesTable], 2);
        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        for (std::string_view table : kDemoTables) {
            EXPECT_EQ(RowCount(transaction, table), counts[table]) << table;
        }
    });
}

TEST(GuideCommandsTest, CreateDemoGuideTagsWithTheSeededVocabularyWhenItIsThere) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CreateDemoGuideTagsWithTheSeededVocabularyWhenItIsThere",
                            [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterGuideCommands(ctx.registry);
        TableHelpers::Neighborhoods(databaseHelper).AddNeighborhood(transaction, {
            { std::string(DbSchema::kNeighborhoodsSlug), "capitol-hill" },
            { std::string(DbSchema::kNeighborhoodsDisplayName), "Capitol Hill" },
            { std::string(DbSchema::kNeighborhoodsRegion),
              std::string(DbSchema::kNeighborhoodsRegionSeattleCore) },
        });
        TableHelpers::SceneTags sceneTags(databaseHelper);
        int sortOrder = 0;
        for (const char* slug : { "leather", "bear", "jock" }) {
            sceneTags.AddSceneTag(transaction, {
                { std::string(DbSchema::kSceneTagsSlug), slug },
                { std::string(DbSchema::kSceneTagsDisplayName), "Name of " + std::string(slug) },
                { std::string(DbSchema::kSceneTagsDescription), "About " + std::string(slug) },
                { std::string(DbSchema::kSceneTagsCoverage), std::string(DbSchema::kSceneTagsCoverageFull) },
                { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder += 10) },
            });
        }

        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        Guide::GuideHelper helper(databaseHelper);

        std::optional<Guide::VenueDetail> bar = helper.GetVenueProfileBySlug(transaction, "demo-bar");
        ASSERT_TRUE(bar.has_value());
        EXPECT_EQ(bar->venue.sceneTagSlugs, std::vector<std::string>{ "leather" });
        EXPECT_EQ(bar->venue.neighborhoodSlug, "capitol-hill");
        std::optional<Guide::VenueDetail> closedBar =
            helper.GetVenueProfileBySlug(transaction, "demo-closed-bar");
        ASSERT_TRUE(closedBar.has_value());
        EXPECT_EQ(closedBar->venue.sceneTagSlugs, std::vector<std::string>{ "bear" });

        std::optional<Guide::OrganizationDetail> league =
            helper.GetOrganizationBySlug(transaction, "demo-softball-league");
        ASSERT_TRUE(league.has_value());
        EXPECT_EQ(league->organization.sceneTagSlugs, std::vector<std::string>{ "jock" });
        EXPECT_EQ(league->organization.neighborhoodSlug, "capitol-hill");
        std::optional<Guide::OrganizationDetail> chorus =
            helper.GetOrganizationBySlug(transaction, "demo-chorus");
        ASSERT_TRUE(chorus.has_value());
        EXPECT_TRUE(chorus->organization.sceneTagSlugs.empty());
        EXPECT_TRUE(chorus->organization.neighborhoodSlug.empty());
    });
}

TEST(GuideCommandsTest, PublishPageMakesAPagePublicFromNow) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PublishPageMakesAPagePublicFromNow", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterGuideCommands(ctx.registry);
        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        Guide::GuideHelper helper(databaseHelper);

        // The draft.
        int64_t beforeUs = NowUs(transaction);
        ASSERT_EQ(RunCommand(ctx, transaction, "publish-page", { { "slug", "demo-draft-page" } }), 0);
        std::optional<Guide::GuidePage> published =
            helper.GetPublishedGuidePage(transaction, "demo-draft-page");
        ASSERT_TRUE(published.has_value());
        EXPECT_FALSE(published->draft);
        ASSERT_TRUE(published->publishAtUs.has_value());
        EXPECT_GE(*published->publishAtUs, beforeUs);

        // Publishing it again changes nothing.
        ASSERT_EQ(RunCommand(ctx, transaction, "publish-page", { { "slug", "demo-draft-page" } }), 0);
        std::optional<Guide::GuidePage> again =
            helper.GetPublishedGuidePage(transaction, "demo-draft-page");
        ASSERT_TRUE(again.has_value() && again->publishAtUs.has_value());
        EXPECT_EQ(*again->publishAtUs, *published->publishAtUs);

        // A page scheduled for next week goes public now.
        TableHelpers::GuidePages(databaseHelper).AddPage(transaction, {
            { std::string(DbSchema::kGuidePagesTitle), "Scheduled" },
            { std::string(DbSchema::kGuidePagesSlug), "scheduled-page" },
            { std::string(DbSchema::kGuidePagesSection), std::string(DbSchema::kGuidePagesSectionHealth) },
            { std::string(DbSchema::kGuidePagesBodyMarkdown), "Body" },
            { std::string(DbSchema::kGuidePagesDraft), "false" },
            { std::string(DbSchema::kGuidePagesPublishAtUs), StringFromInt(NowUs(transaction) + 7 * kUsPerDay) },
        });
        EXPECT_FALSE(helper.GetPublishedGuidePage(transaction, "scheduled-page").has_value());
        ASSERT_EQ(RunCommand(ctx, transaction, "publish-page", { { "slug", "scheduled-page" } }), 0);
        EXPECT_TRUE(helper.GetPublishedGuidePage(transaction, "scheduled-page").has_value());

        EXPECT_EQ(RunCommand(ctx, transaction, "publish-page", { { "slug", "no-such-page" } }), 1);
        EXPECT_EQ(RunCommand(ctx, transaction, "publish-page"), 1);
    });
}

TEST(GuideCommandsTest, VerifyListingStampsEachListingTable) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("VerifyListingStampsEachListingTable", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterGuideCommands(ctx.registry);
        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        Guide::GuideHelper helper(databaseHelper);

        // The demo has no service; verify-listing covers them too.
        int64_t serviceId = TableHelpers::ServiceListings(databaseHelper).AddServiceListing(transaction, {
            { std::string(DbSchema::kServiceListingsName), "Example Clinic" },
            { std::string(DbSchema::kServiceListingsSlug), "example-clinic" },
            { std::string(DbSchema::kServiceListingsServiceKind),
              std::string(DbSchema::kServiceListingsServiceKindStiTesting) },
        });
        KeyValueTable profile =
            TableHelpers::VenueProfiles(databaseHelper).GetProfileBySlug(transaction, "demo-bar");
        int64_t venueId = IdColumn(profile, DbSchema::kVenueProfilesVenueId);
        int64_t profileId = IdColumn(profile, DbSchema::kVenueProfilesId);
        int64_t organizationId = IdColumn(
            TableHelpers::Organizations(databaseHelper).GetOrganizationBySlug(transaction, "demo-chorus"),
            DbSchema::kOrganizationsId);
        int64_t placeId = IdColumn(
            TableHelpers::Places(databaseHelper).GetPlaceBySlug(transaction, "demo-denny-blaine"),
            DbSchema::kPlacesId);
        int64_t pageId = IdColumn(
            TableHelpers::GuidePages(databaseHelper).GetPageBySlug(transaction, "demo-published-page"),
            DbSchema::kGuidePagesId);

        auto verify = [&](std::string_view table, int64_t id) {
            int64_t beforeUs = NowUs(transaction);
            ASSERT_EQ(LastVerifiedAtUs(transaction, table, id), 0) << table;
            ASSERT_EQ(RunCommand(ctx, transaction, "verify-listing",
                          { { "table", std::string(table) }, { "id", StringFromInt(id) } }), 0) << table;
            EXPECT_GE(LastVerifiedAtUs(transaction, table, id), beforeUs) << table;
        };

        // A guide venue is only as verified as the older of its two rows: the
        // venue alone doesn't stamp it.
        verify(DbSchema::kVenuesTable, venueId);
        EXPECT_FALSE(helper.GetVenueProfileBySlug(transaction, "demo-bar")
                         ->venue.freshness.lastVerifiedAtUs.has_value());
        verify(DbSchema::kVenueProfilesTable, profileId);
        EXPECT_TRUE(helper.GetVenueProfileBySlug(transaction, "demo-bar")
                        ->venue.freshness.lastVerifiedAtUs.has_value());

        verify(DbSchema::kOrganizationsTable, organizationId);
        verify(DbSchema::kPlacesTable, placeId);
        verify(DbSchema::kServiceListingsTable, serviceId);
        verify(DbSchema::kGuidePagesTable, pageId);
        EXPECT_TRUE(helper.GetPlaceBySlug(transaction, "demo-denny-blaine")
                        ->place.freshness.lastVerifiedAtUs.has_value());

        // Only the rows named were stamped.
        EXPECT_EQ(VerifiedRowCount(transaction, DbSchema::kVenuesTable), 1);
        EXPECT_EQ(VerifiedRowCount(transaction, DbSchema::kOrganizationsTable), 1);
        EXPECT_EQ(VerifiedRowCount(transaction, DbSchema::kGuidePagesTable), 1);
    });
}

TEST(GuideCommandsTest, VerifyListingRefusesBadInputWithoutStamping) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("VerifyListingRefusesBadInputWithoutStamping", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterGuideCommands(ctx.registry);
        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        std::string placeId = Column(
            TableHelpers::Places(databaseHelper).GetPlaceBySlug(transaction, "demo-denny-blaine"),
            DbSchema::kPlacesId);

        EXPECT_EQ(RunCommand(ctx, transaction, "verify-listing", { { "id", placeId } }), 1);
        for (const char* table : { "events", "scene_tags", "place" }) {
            EXPECT_EQ(RunCommand(ctx, transaction, "verify-listing",
                          { { "table", table }, { "id", placeId } }), 1) << table;
        }
        for (const char* id : { "", "abc", "12x" }) {
            EXPECT_EQ(RunCommand(ctx, transaction, "verify-listing",
                          { { "table", "places" }, { "id", id } }), 1) << id;
        }
        EXPECT_EQ(RunCommand(ctx, transaction, "verify-listing",
                      { { "table", "places" }, { "id", StringFromInt(std::stoll(placeId) + 1000) } }), 1);

        for (std::string_view table : { DbSchema::kVenuesTable, DbSchema::kVenueProfilesTable,
                                        DbSchema::kOrganizationsTable, DbSchema::kPlacesTable,
                                        DbSchema::kGuidePagesTable }) {
            EXPECT_EQ(VerifiedRowCount(transaction, table), 0) << table;
        }
    });
}

TEST(GuideCommandsTest, ListGraveyardRunsEmptyAndWithTheDemo) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ListGraveyardRunsEmptyAndWithTheDemo", [&](Transaction& transaction) {
        TestHelperContext ctx(testDb.GetDatabaseHelper());
        RegisterGuideCommands(ctx.registry);
        EXPECT_EQ(RunCommand(ctx, transaction, "list-graveyard"), 0);
        ASSERT_EQ(RunCommand(ctx, transaction, "create-demo-guide"), 0);
        EXPECT_EQ(RunCommand(ctx, transaction, "list-graveyard"), 0);
    });
}

}  // namespace
}  // namespace TestHelper
