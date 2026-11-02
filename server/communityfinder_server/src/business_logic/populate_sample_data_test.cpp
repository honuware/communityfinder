#include "business_logic/populate_sample_data.h"

#include <cstdint>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "business_logic/create_app_tables.h"
#include "business_logic/create_framework_tables.h"
#include "business_logic/events/event_helper.h"
#include "business_logic/events/event_ical.h"
#include "business_logic/guide/guide_helper.h"
#include "db_schema/events.h"
#include "sql_util/database_access/transaction.h"
#include "test/src/util/database_test_helper.h"

// database_helper --populate_sample_data's body, run in a test transaction on
// top of the seed (it needs the vocabularies), as the live command runs it.
namespace {

constexpr int64_t kReviewerPersonId = 1;
// The four weeks the events land in, plus a day's slack: a clock change in
// between moves a wall-clock time by an hour against the UTC count.
constexpr int64_t kWindowUs = 29 * Events::kUsPerDay;

// Venues, organizations, places, services, guide pages and series.
constexpr int kSampleListingCount = 6 + 9 + 2 + 6 + 11 + 5;
// Fourteen one-offs (one left pending), four dates each for three weekly
// series, and two hikes.
constexpr int kSampleEventCount = 14 + 3 * 4 + 2;

void Seed(Transaction& transaction, TestDatabaseUtil& testDb) {
    PopulateFrameworkTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());
    PopulateAppTables(transaction, testDb.GetDatabaseHelper(), testDb.GetDatabaseInfo());
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

int CountRows(Transaction& transaction, const std::string& sql) {
    return std::stoi(transaction.RunSqlStatementReturningOneValue(sql));
}

}  // namespace

TEST(PopulateSampleDataTest, BuildsAGuideAndFourWeeksOfApprovedEvents) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BuildsSampleData", [&](Transaction& transaction) {
        Seed(transaction, testDb);
        const int64_t nowUs = NowUs(transaction);

        SampleDataReport report =
            PopulateSampleData(transaction, testDb.GetDatabaseHelper(), kReviewerPersonId);

        EXPECT_TRUE(report.missingVocabulary.empty());
        EXPECT_EQ(report.listingsCreated, kSampleListingCount);
        EXPECT_EQ(report.listingsAlreadyThere, 0);
        EXPECT_EQ(report.eventsCreated, kSampleEventCount);
        EXPECT_EQ(report.eventsApproved, kSampleEventCount - 1);

        // The guide reads it as the pages will: the open venues in the
        // directory, the closed ones in the graveyard, published pages only.
        Guide::GuideHelper guide(testDb.GetDatabaseHelper());
        EXPECT_EQ(guide.GetVenuesDirectory(transaction, {}).size(), 5u);
        Guide::Graveyard graveyard = guide.GetGraveyard(transaction);
        ASSERT_EQ(graveyard.venues.size(), 1u);
        EXPECT_EQ(graveyard.venues[0].slug, "the-wren");
        ASSERT_EQ(graveyard.organizations.size(), 1u);
        EXPECT_EQ(graveyard.organizations[0].slug, "night-owls-book-club");
        std::optional<Guide::GuidePage> leather = guide.GetPublishedGuidePage(transaction, "leather");
        ASSERT_TRUE(leather.has_value());
        EXPECT_NE(leather->bodyMarkdown.find("](/guide/venues/iron-and-oak)"), std::string::npos);
        EXPECT_FALSE(guide.GetPublishedGuidePage(transaction, "insurance-and-costs").has_value());
        EXPECT_FALSE(guide.GetPublishedGuidePage(transaction, "beat-the-freeze").has_value());

        // Every approved event is public and in the next four weeks; the
        // pending one waits in the review queue.
        Events::UpcomingQuery query;
        query.toUs = nowUs + kWindowUs;
        Events::UpcomingEvents upcoming =
            Events::EventHelper(testDb.GetDatabaseHelper()).GetUpcomingPublic(transaction, query);
        ASSERT_EQ(upcoming.events.size(), static_cast<size_t>(kSampleEventCount - 1));
        for (const Events::PublicEvent& event : upcoming.events) {
            EXPECT_GT(event.startsAtUs, nowUs) << event.title;
            EXPECT_LT(event.startsAtUs, nowUs + kWindowUs) << event.title;
        }
        EXPECT_EQ(CountRows(transaction,
                            "SELECT COUNT(*) FROM events WHERE status = 'pending' "
                            "AND title = 'Open Mic (awaiting review)'"),
                  1);
    });
}

TEST(PopulateSampleDataTest, PutsTheSeriesOnTheirSeattleDaysAndTimes) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SeriesTimes", [&](Transaction& transaction) {
        Seed(transaction, testDb);
        PopulateSampleData(transaction, testDb.GetDatabaseHelper(), kReviewerPersonId);

        // Read back in Seattle time: Thursday 6 PM, every one, on either side
        // of a clock change. Four distinct dates, a week apart.
        const std::string zone(Events::kEventsTimeZone);
        StringTable rows = transaction.RunSqlStatementReturningStringTable(
            "SELECT EXTRACT(DOW FROM to_timestamp(e.starts_at_us / 1000000.0) AT TIME ZONE $1)::int,"
            " to_char(to_timestamp(e.starts_at_us / 1000000.0) AT TIME ZONE $1, 'HH24:MI'),"
            " to_char(to_timestamp(e.starts_at_us / 1000000.0) AT TIME ZONE $1, 'YYYY-MM-DD')"
            " FROM events e JOIN series s ON s.id = e.series_id"
            " WHERE s.slug = 'bear-happy-hour' ORDER BY e.starts_at_us",
            zone);
        ASSERT_EQ(rows.size(), 4u);
        for (size_t index = 0; index < rows.size(); ++index) {
            EXPECT_EQ(rows[index][0], "4");
            EXPECT_EQ(rows[index][1], "18:00");
            if (index > 0) EXPECT_NE(rows[index][2], rows[index - 1][2]);
        }
    });
}

TEST(PopulateSampleDataTest, ARerunTheSameDayAddsNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("Rerun", [&](Transaction& transaction) {
        Seed(transaction, testDb);
        PopulateSampleData(transaction, testDb.GetDatabaseHelper(), kReviewerPersonId);
        const int venues = CountRows(transaction, "SELECT COUNT(*) FROM venues");
        const int links = CountRows(transaction, "SELECT COUNT(*) FROM guide_links");

        SampleDataReport again =
            PopulateSampleData(transaction, testDb.GetDatabaseHelper(), kReviewerPersonId);

        EXPECT_EQ(again.listingsCreated, 0);
        EXPECT_EQ(again.listingsAlreadyThere, kSampleListingCount);
        EXPECT_EQ(again.eventsCreated, 0);
        EXPECT_EQ(again.eventsUnchanged, kSampleEventCount);
        EXPECT_EQ(again.eventsApproved, 0);
        EXPECT_EQ(CountRows(transaction, "SELECT COUNT(*) FROM venues"), venues);
        EXPECT_EQ(CountRows(transaction, "SELECT COUNT(*) FROM guide_links"), links);
        EXPECT_EQ(CountRows(transaction, "SELECT COUNT(*) FROM events"), kSampleEventCount);
    });
}

TEST(PopulateSampleDataTest, ALaterRunMovesTheEventsAlongAndApprovesThemAgain) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("LaterRun", [&](Transaction& transaction) {
        Seed(transaction, testDb);
        PopulateSampleData(transaction, testDb.GetDatabaseHelper(), kReviewerPersonId);
        // As if the first run were a week ago: every sample event a week
        // earlier than this run will put it.
        transaction.RunSqlStatement(
            "UPDATE events SET starts_at_us = starts_at_us - $1::bigint,"
            " ends_at_us = ends_at_us - $1::bigint",
            std::to_string(7 * Events::kUsPerDay));

        SampleDataReport later =
            PopulateSampleData(transaction, testDb.GetDatabaseHelper(), kReviewerPersonId);

        EXPECT_EQ(later.eventsCreated, 0);
        EXPECT_EQ(later.eventsUpdated, kSampleEventCount);
        // Moving a start sends an approved event back to pending; the run
        // approves it again. The one meant to wait stays pending.
        EXPECT_EQ(later.eventsApproved, kSampleEventCount - 1);
        EXPECT_EQ(CountRows(transaction, "SELECT COUNT(*) FROM events"), kSampleEventCount);
        EXPECT_EQ(CountRows(transaction, "SELECT COUNT(*) FROM events WHERE status = 'pending'"), 1);
        EXPECT_EQ(CountRows(transaction,
                            "SELECT COUNT(*) FROM events WHERE starts_at_us <= now_us()"),
                  0);
    });
}
