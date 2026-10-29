#include "guide_pages.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/freshness.h"
#include "db_schema/guide_pages.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

constexpr int64_t kUsPerDay = 86'400LL * 1'000'000LL;

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

KeyValueTable MakePage(std::string_view title, std::string_view slug,
                       std::string_view section) {
    return {
        { std::string(DbSchema::kGuidePagesTitle), std::string(title) },
        { std::string(DbSchema::kGuidePagesSlug), std::string(slug) },
        { std::string(DbSchema::kGuidePagesSection), std::string(section) },
    };
}

// A page in `section`, with `draft` and `publishAtUs` set ("" leaves
// publish_at_us NULL).
int64_t AddPageInState(Transaction& transaction, GuidePages& pages, std::string_view slug,
                       std::string_view section, bool draft, const std::string& publishAtUs) {
    KeyValueTable values = MakePage(slug, slug, section);
    values[std::string(DbSchema::kGuidePagesDraft)] = draft ? "true" : "false";
    if (!publishAtUs.empty()) {
        values[std::string(DbSchema::kGuidePagesPublishAtUs)] = publishAtUs;
    }
    return pages.AddPage(transaction, values);
}

std::vector<std::string> Slugs(const KeyValueTableArray& rows) {
    std::vector<std::string> slugs;
    for (const KeyValueTable& row : rows) {
        slugs.push_back(row.at(std::string(DbSchema::kGuidePagesSlug)));
    }
    return slugs;
}

TEST(GuidePagesTest, AddAndGetByIdAndSlugStartsAsAnUnpublishedDraft) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "AddAndGetByIdAndSlugStartsAsAnUnpublishedDraft", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        KeyValueTable values = MakePage("Bears", "bears", DbSchema::kGuidePagesSectionScenes);
        values[std::string(DbSchema::kGuidePagesBodyMarkdown)] = "# Bears\n\nWhere to go.";
        int64_t id = pages.AddPage(transaction, values);
        EXPECT_GT(id, 0);

        KeyValueTable byId = pages.GetPage(transaction, id);
        ASSERT_FALSE(byId.empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kGuidePagesBodyMarkdown)),
                  "# Bears\n\nWhere to go.");
        EXPECT_EQ(byId.at(std::string(DbSchema::kGuidePagesDraft)), "t");
        EXPECT_TRUE(byId.at(std::string(DbSchema::kGuidePagesPublishAtUs)).empty());
        EXPECT_EQ(byId.at(std::string(DbSchema::kGuidePagesStatus)),
                  DbSchema::kFreshnessStatusOpen);

        // The author's view finds it; the public one doesn't.
        EXPECT_FALSE(pages.GetPageBySlug(transaction, "bears").empty());
        EXPECT_TRUE(pages.GetPublishedPageBySlug(transaction, "bears").empty());
    });
}

TEST(GuidePagesTest, MissesAreEmpty) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissesAreEmpty", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        EXPECT_TRUE(pages.GetPage(transaction, 999999).empty());
        EXPECT_TRUE(pages.GetPageBySlug(transaction, "no-such-page").empty());
        EXPECT_TRUE(pages.GetPublishedPageBySlug(transaction, "no-such-page").empty());
        EXPECT_TRUE(pages.GetPublishedPagesBySection(
            transaction, DbSchema::kGuidePagesSectionFreeze).empty());
    });
}

// The publish rule needs all three: not a draft, a publish time, and that time
// arrived. Each other combination stays private.
TEST(GuidePagesTest, OnlyPublishedPagesAreVisible) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OnlyPublishedPagesAreVisible", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        const std::string past = StringFromInt(NowUs(transaction) - kUsPerDay);
        const std::string future = StringFromInt(NowUs(transaction) + 30 * kUsPerDay);
        const std::string_view section = DbSchema::kGuidePagesSectionHealth;

        AddPageInState(transaction, pages, "published", section, false, past);
        // Dated in the past but still a draft.
        AddPageInState(transaction, pages, "still-a-draft", section, true, past);
        // Not a draft, but never given a publish time.
        AddPageInState(transaction, pages, "unscheduled", section, false, "");
        // Not a draft and dated, but the date hasn't arrived.
        AddPageInState(transaction, pages, "scheduled", section, false, future);

        EXPECT_FALSE(pages.GetPublishedPageBySlug(transaction, "published").empty());
        for (std::string_view slug : { "still-a-draft", "unscheduled", "scheduled" }) {
            EXPECT_TRUE(pages.GetPublishedPageBySlug(transaction, slug).empty()) << slug;
            // The preview path still finds every one.
            EXPECT_FALSE(pages.GetPageBySlug(transaction, slug).empty()) << slug;
        }
        EXPECT_EQ(Slugs(pages.GetPublishedPagesBySection(transaction, section)),
                  (std::vector<std::string>{ "published" }));
    });
}

TEST(GuidePagesTest, PublishedPagesBySectionAreTheListShapeInTitleOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction(
        "PublishedPagesBySectionAreTheListShapeInTitleOrder", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        const std::string past = StringFromInt(NowUs(transaction) - kUsPerDay);
        AddPageInState(transaction, pages, "vashon", DbSchema::kGuidePagesSectionNewTo,
                       false, past);
        AddPageInState(transaction, pages, "bellevue", DbSchema::kGuidePagesSectionNewTo,
                       false, past);
        AddPageInState(transaction, pages, "elsewhere", DbSchema::kGuidePagesSectionFreeze,
                       false, past);

        KeyValueTableArray rows =
            pages.GetPublishedPagesBySection(transaction, DbSchema::kGuidePagesSectionNewTo);
        EXPECT_EQ(Slugs(rows), (std::vector<std::string>{ "bellevue", "vashon" }));

        ASSERT_FALSE(rows.empty());
        const KeyValueTable& row = rows.front();
        // The list carries the freshness fields (Contract 3) but not the body.
        EXPECT_EQ(row.count(std::string(DbSchema::kGuidePagesStatus)), 1u);
        EXPECT_EQ(row.count(std::string(DbSchema::kGuidePagesLastVerifiedAtUs)), 1u);
        EXPECT_EQ(row.count(std::string(DbSchema::kGuidePagesAdult)), 1u);
        EXPECT_EQ(row.count(std::string(DbSchema::kGuidePagesTitle)), 1u);
        EXPECT_EQ(row.count(std::string(DbSchema::kGuidePagesPublishAtUs)), 1u);
        EXPECT_EQ(row.count(std::string(DbSchema::kGuidePagesBodyMarkdown)), 0u);
    });
}

// GD2 OQ9 (a): with no cadence of its own, an adult page re-checks monthly, a
// health page semiannually, and any other page annually.
TEST(GuidePagesTest, CadenceFollowsTheSection) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("CadenceFollowsTheSection", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        const std::string tier(DbSchema::kGuidePagesVolatilityTier);

        KeyValueTable adultValues = MakePage("Bathhouses", "bathhouses",
                                             DbSchema::kGuidePagesSectionAdult);
        adultValues[std::string(DbSchema::kGuidePagesAdult)] = "true";
        int64_t adult = pages.AddPage(transaction, adultValues);
        int64_t health = pages.AddPage(transaction, MakePage(
            "Getting PrEP", "getting-prep", DbSchema::kGuidePagesSectionHealth));
        int64_t freeze = pages.AddPage(transaction, MakePage(
            "The Seattle Freeze", "the-freeze", DbSchema::kGuidePagesSectionFreeze));
        KeyValueTable explicitValues = MakePage("Testing", "testing",
                                                DbSchema::kGuidePagesSectionHealth);
        explicitValues[tier] = std::string(DbSchema::kVolatilityTierQuarterly);
        int64_t explicitTier = pages.AddPage(transaction, explicitValues);

        EXPECT_EQ(pages.GetPage(transaction, adult).at(tier), DbSchema::kVolatilityTierMonthly);
        EXPECT_EQ(pages.GetPage(transaction, health).at(tier),
                  DbSchema::kVolatilityTierSemiannual);
        EXPECT_EQ(pages.GetPage(transaction, freeze).at(tier), DbSchema::kVolatilityTierAnnual);
        EXPECT_EQ(pages.GetPage(transaction, explicitTier).at(tier),
                  DbSchema::kVolatilityTierQuarterly);
    });
}

// Publishing is an update: clear draft and set the time.
TEST(GuidePagesTest, UpdatePublishesAndBumpsUpdatedUs) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UpdatePublishesAndBumpsUpdatedUs", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        KeyValueTable values = MakePage("Capitol Hill", "capitol-hill",
                                        DbSchema::kGuidePagesSectionNewToSeattle);
        // updated_us = 0 makes the bump observable (insert and update can share a microsecond).
        values[std::string(DbSchema::kGuidePagesUpdatedUs)] = "0";
        int64_t id = pages.AddPage(transaction, values);
        ASSERT_TRUE(pages.GetPublishedPageBySlug(transaction, "capitol-hill").empty());

        pages.UpdatePage(transaction, id, {
            { std::string(DbSchema::kGuidePagesDraft), "false" },
            { std::string(DbSchema::kGuidePagesPublishAtUs),
              StringFromInt(NowUs(transaction) - kUsPerDay) },
        });

        KeyValueTable row = pages.GetPublishedPageBySlug(transaction, "capitol-hill");
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kGuidePagesId)), StringFromInt(id));
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kGuidePagesUpdatedUs))), 0);
    });
}

TEST(GuidePagesTest, DuplicateSlugThrowsAcrossSections) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DuplicateSlugThrowsAcrossSections", [&](Transaction& transaction) {
        GuidePages pages(testDb.GetDatabaseHelper());
        pages.AddPage(transaction, MakePage("Bears", "bears", DbSchema::kGuidePagesSectionScenes));
        // The unique violation aborts the transaction — last statement.
        EXPECT_THROW(pages.AddPage(transaction, MakePage(
                         "Bears", "bears", DbSchema::kGuidePagesSectionFreeze)),
                     std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
