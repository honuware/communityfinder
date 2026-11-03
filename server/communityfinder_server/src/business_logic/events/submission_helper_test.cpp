#include "business_logic/events/submission_helper.h"

#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/person.h"
#include "db_schema/admin_alerts.h"
#include "db_schema/categories.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "db_schema/people.h"
#include "db_schema/scene_tags.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/admin_alerts.h"
#include "test/src/util/database_test_helper.h"
#include "util/mail/mail_helper_test_util.h"
#include "util/types.h"

// EV3 §3.1 — SubmissionHelper: the submit door, "My submissions", withdrawal,
// and the curator's side of a submitted date.
namespace Events {
namespace {

constexpr int64_t kCuratorId = 77;  // reviewer ids aren't FKs (EV1 OQ8 (e))
constexpr int64_t kUsPerHour = 3'600LL * 1'000'000LL;
constexpr int64_t kUsPerWeek = 7 * kUsPerDay;

// Two venues and a small vocabulary, built at the top of each test (no
// fixtures). `retired` is a disabled category.
struct World {
    int64_t venueOne = 0;
    int64_t venueTwo = 0;
};

World MakeWorld(Transaction& transaction, DatabaseHelper databaseHelper) {
    World world;
    TableHelpers::Venues venues(databaseHelper);
    world.venueOne = venues.AddVenue(transaction, { { std::string(DbSchema::kVenuesName), "Venue One" } });
    world.venueTwo = venues.AddVenue(transaction, { { std::string(DbSchema::kVenuesName), "Venue Two" } });

    TableHelpers::Categories categories(databaseHelper);
    int64_t sortOrder = 10;
    for (std::string_view slug : { "dance", "music", "food", "art", "retired" }) {
        categories.AddCategory(transaction, {
            { std::string(DbSchema::kCategoriesSlug), std::string(slug) },
            { std::string(DbSchema::kCategoriesDisplayName), std::string(slug) },
            { std::string(DbSchema::kCategoriesSortOrder), StringFromInt(sortOrder += 10) },
            { std::string(DbSchema::kCategoriesEnabled), slug == "retired" ? "false" : "true" },
        });
    }
    TableHelpers::SceneTags sceneTags(databaseHelper);
    for (std::string_view slug : { "all-ages", "sober", "outdoors", "late-night" }) {
        sceneTags.AddSceneTag(transaction, {
            { std::string(DbSchema::kSceneTagsSlug), std::string(slug) },
            { std::string(DbSchema::kSceneTagsDisplayName), std::string(slug) },
            { std::string(DbSchema::kSceneTagsSortOrder), StringFromInt(sortOrder += 10) },
        });
    }
    return world;
}

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

int64_t AddVerifiedPerson(Transaction& transaction, DatabaseHelper databaseHelper,
                          std::string_view email) {
    Auth::PersonHelper(databaseHelper).CreateFullyValidatedUser(
        transaction, { std::string(email), "Sam", "Sample" }, "Password123!");
    return std::stoll(TableHelpers::People(databaseHelper)
                          .LookupPersonByEmail(transaction, email)
                          .at(std::string(DbSchema::kPeopleId)));
}

// `dateCount` weekly dates, the first two days out.
SubmissionRequest MakeRequest(int64_t nowUs, size_t dateCount = 1) {
    SubmissionRequest request;
    request.title = "Sample Dance Night";
    request.agreed = true;
    for (size_t index = 0; index < dateCount; ++index) {
        request.dates.push_back({ nowUs + 2 * kUsPerDay + static_cast<int64_t>(index) * kUsPerWeek,
                                  std::nullopt });
    }
    return request;
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

KeyValueTable EventRow(Transaction& transaction, DatabaseHelper databaseHelper, int64_t eventId) {
    return TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId);
}

KeyValueTable SubmissionRow(Transaction& transaction, DatabaseHelper databaseHelper, int64_t id) {
    return TableHelpers::EventSubmissions(databaseHelper).GetSubmission(transaction, id);
}

int64_t AddApprovedEvent(Transaction& transaction, DatabaseHelper databaseHelper,
                         std::string_view title, int64_t startsAtUs,
                         std::optional<int64_t> venueId,
                         std::string_view status = DbSchema::kEventsStatusApproved) {
    KeyValueTable values = {
        { std::string(DbSchema::kEventsTitle), std::string(title) },
        { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(startsAtUs) },
        { std::string(DbSchema::kEventsStatus), std::string(status) },
    };
    if (venueId) values[std::string(DbSchema::kEventsVenueId)] = StringFromInt(*venueId);
    return TableHelpers::Events(databaseHelper).AddEvent(transaction, values);
}

class FailingMailHelper : public ::Mail::MailHelper {
public:
    void SendMail(const ::Mail::MailMessage&) override {
        throw std::runtime_error("smtp unavailable");
    }
};

// --- FindDuplicateHints (no database) ---

TEST(SubmissionHelperTest, NormalizeTitleKeepsOnlyLettersAndDigits) {
    EXPECT_EQ(NormalizeTitleForMatching("Drag-Brunch! 2026"), "dragbrunch2026");
    EXPECT_EQ(NormalizeTitleForMatching("drag brunch 2026"), "dragbrunch2026");
    EXPECT_EQ(NormalizeTitleForMatching("Sam's  Party"), "samsparty");
    // Other scripts' bytes are kept as they are.
    EXPECT_EQ(NormalizeTitleForMatching("Caf\xC3\xA9 Night"), "caf\xC3\xA9night");
    EXPECT_EQ(NormalizeTitleForMatching("!!! ---"), "");
}

TEST(SubmissionHelperTest, DuplicateHintsByVenueOrTitleInsideThreeHours) {
    const int64_t startsAtUs = 1'000'000'000'000LL;
    std::vector<DuplicateCandidate> candidates = {
        { 1, "Something Else", startsAtUs + 3 * kUsPerHour, 5 },          // venue, at the edge
        { 2, "SAMPLE dance-night", startsAtUs - 2 * kUsPerHour, std::nullopt },  // title
        { 3, "Something Else", startsAtUs + 3 * kUsPerHour + 1, 5 },      // just outside
        { 4, "Sample Dance Night", startsAtUs + 4 * kUsPerHour, 5 },      // a 4-hour gap
        { 5, "Something Else", startsAtUs, 6 },                           // other venue and title
    };
    std::vector<DuplicateCandidate> matches =
        FindDuplicateHints("Sample Dance Night", startsAtUs, 5, candidates);
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].eventId, 1);
    EXPECT_EQ(matches[1].eventId, 2);

    // No venue on the new date: only the title can match.
    matches = FindDuplicateHints("Sample Dance Night", startsAtUs, std::nullopt, candidates);
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].eventId, 2);
}

TEST(SubmissionHelperTest, ATitleOfOnlyPunctuationMatchesNothing) {
    std::vector<DuplicateCandidate> candidates = { { 1, "???", 1000, std::nullopt } };
    EXPECT_TRUE(FindDuplicateHints("!!!", 1000, std::nullopt, candidates).empty());
}

// --- Submit ---

TEST(SubmissionHelperTest, SubmitWritesPendingSubmittedDates) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SubmitWritesPendingSubmittedDates", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");

        SubmissionRequest request = MakeRequest(NowUs(transaction), 3);
        request.title = "  Sample Dance Night  ";
        request.dates[1].endsAtUs = request.dates[1].startsAtUs + 3 * kUsPerHour;
        request.venueId = world.venueOne;
        request.description = "Bring water.";
        request.url = "https://example.org/sample";
        request.costText = "$5 at the door";
        request.locationText = "Upstairs";
        request.categorySlugs = { "dance", "music" };
        request.sceneTagSlugs = { "sober" };
        request.regularThing = true;
        request.relationship = std::string(DbSchema::kEventSubmissionsRelationshipOrganizer);
        request.submitterNote = "Second Saturdays.";

        SubmitResult result = SubmissionHelper(db).Submit(transaction, *secrets, request, personId);
        ASSERT_TRUE(result.ok) << result.errorCode;
        ASSERT_GT(result.submissionId, 0);
        ASSERT_EQ(result.eventIds.size(), 3u);
        EXPECT_TRUE(result.ignoredCategorySlugs.empty());

        KeyValueTable submission = SubmissionRow(transaction, db, result.submissionId);
        EXPECT_EQ(Column(submission, DbSchema::kEventSubmissionsSubmittedByPersonId), StringFromInt(personId));
        EXPECT_EQ(Column(submission, DbSchema::kEventSubmissionsRelationship), "organizer");
        EXPECT_EQ(Column(submission, DbSchema::kEventSubmissionsRegularThing), "t");
        EXPECT_EQ(Column(submission, DbSchema::kEventSubmissionsSubmitterNote), "Second Saturdays.");

        for (size_t index = 0; index < 3; ++index) {
            KeyValueTable event = EventRow(transaction, db, result.eventIds[index]);
            EXPECT_EQ(Column(event, DbSchema::kEventsTitle), "Sample Dance Night");
            EXPECT_EQ(Column(event, DbSchema::kEventsStartsAtUs), StringFromInt(request.dates[index].startsAtUs));
            EXPECT_EQ(Column(event, DbSchema::kEventsOrigin), DbSchema::kEventsOriginUserSubmitted);
            EXPECT_EQ(Column(event, DbSchema::kEventsStatus), DbSchema::kEventsStatusPending);
            EXPECT_EQ(Column(event, DbSchema::kEventsCreatedByPersonId), StringFromInt(personId));
            EXPECT_EQ(Column(event, DbSchema::kEventsSubmissionId), StringFromInt(result.submissionId));
            EXPECT_TRUE(Column(event, DbSchema::kEventsSourceId).empty());
            EXPECT_TRUE(Column(event, DbSchema::kEventsExternalKey).empty());
            EXPECT_EQ(Column(event, DbSchema::kEventsVenueId), StringFromInt(world.venueOne));
            EXPECT_EQ(Column(event, DbSchema::kEventsUrl), "https://example.org/sample");
            EXPECT_EQ(Column(event, DbSchema::kEventsCostText), "$5 at the door");
            EXPECT_EQ(Column(event, DbSchema::kEventsLocationText), "Upstairs");
            EXPECT_EQ(Column(event, DbSchema::kEventsDescription), "Bring water.");
            EXPECT_EQ(TableHelpers::EventCategoryAssignments(db).GetForEvent(transaction, result.eventIds[index]).size(), 2u);
            EXPECT_EQ(TableHelpers::EventSceneTagAssignments(db).GetForEvent(transaction, result.eventIds[index]).size(), 1u);
        }
        EXPECT_EQ(Column(EventRow(transaction, db, result.eventIds[1]), DbSchema::kEventsEndsAtUs),
                  StringFromInt(*request.dates[1].endsAtUs));
        EXPECT_TRUE(Column(EventRow(transaction, db, result.eventIds[0]), DbSchema::kEventsEndsAtUs).empty());
    });
}

// Each check, with the field (and date) it belongs to. None of them writes.
TEST(SubmissionHelperTest, EveryValidationCode) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EveryValidationCode", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);

        struct Case {
            std::string_view name;
            std::function<void(SubmissionRequest&)> change;
            std::string_view errorCode;
            std::string_view field;
            int64_t dateIndex = -1;
        };
        const std::vector<Case> cases = {
            { "not agreed", [](SubmissionRequest& r) { r.agreed = false; },
              kSubmissionErrorNotAgreed, "agreed" },
            { "title too short", [](SubmissionRequest& r) { r.title = "ab"; },
              kSubmissionErrorTitleLength, "title" },
            { "title only spaces", [](SubmissionRequest& r) { r.title = "  a   "; },
              kSubmissionErrorTitleLength, "title" },
            { "title too long", [](SubmissionRequest& r) { r.title = std::string(121, 'a'); },
              kSubmissionErrorTitleLength, "title" },
            { "no dates", [](SubmissionRequest& r) { r.dates.clear(); },
              kSubmissionErrorNoDates, "dates" },
            { "too soon", [&](SubmissionRequest& r) {
                  r.dates.push_back({ nowUs + 30 * 60'000'000LL, std::nullopt }); },
              kSubmissionErrorStartTooSoon, "dates", 1 },
            { "too far", [&](SubmissionRequest& r) { r.dates[0].startsAtUs = nowUs + 366 * kUsPerDay; },
              kSubmissionErrorStartTooFar, "dates", 0 },
            { "end at start", [](SubmissionRequest& r) { r.dates[0].endsAtUs = r.dates[0].startsAtUs; },
              kEventErrorEndBeforeStart, "dates", 0 },
            { "over four days", [](SubmissionRequest& r) {
                  r.dates[0].endsAtUs = r.dates[0].startsAtUs + 4 * kUsPerDay + 1; },
              kSubmissionErrorTooLong, "dates", 0 },
            { "same date twice", [](SubmissionRequest& r) { r.dates.push_back(r.dates[0]); },
              kSubmissionErrorDuplicateDate, "dates", 1 },
            { "unknown venue", [](SubmissionRequest& r) { r.venueId = 999999; },
              kEventErrorVenueNotFound, "venue_id" },
            { "venue zero", [](SubmissionRequest& r) { r.venueId = 0; },
              kEventErrorVenueNotFound, "venue_id" },
            { "long place", [](SubmissionRequest& r) { r.locationText = std::string(301, 'a'); },
              kSubmissionErrorFieldTooLong, "location_text" },
            { "long description", [](SubmissionRequest& r) { r.description = std::string(4001, 'a'); },
              kSubmissionErrorFieldTooLong, "description" },
            { "long cost", [](SubmissionRequest& r) { r.costText = std::string(101, 'a'); },
              kSubmissionErrorFieldTooLong, "cost_text" },
            { "long link", [](SubmissionRequest& r) { r.url = "https://example.org/" + std::string(481, 'a'); },
              kSubmissionErrorFieldTooLong, "url" },
            { "long note", [](SubmissionRequest& r) { r.submitterNote = std::string(1001, 'a'); },
              kSubmissionErrorFieldTooLong, "submitter_note" },
            { "ftp link", [](SubmissionRequest& r) { r.url = "ftp://example.org"; },
              kSubmissionErrorBadUrl, "url" },
            { "bare link", [](SubmissionRequest& r) { r.url = "example.org"; },
              kSubmissionErrorBadUrl, "url" },
            { "scheme only", [](SubmissionRequest& r) { r.url = "https://"; },
              kSubmissionErrorBadUrl, "url" },
            { "spaced link", [](SubmissionRequest& r) { r.url = "https://example.org/a b"; },
              kSubmissionErrorBadUrl, "url" },
            { "bad relationship", [](SubmissionRequest& r) { r.relationship = "fan"; },
              kSubmissionErrorBadRelationship, "relationship" },
            { "four categories", [](SubmissionRequest& r) {
                  r.categorySlugs = { "dance", "music", "food", "art" }; },
              kSubmissionErrorTooManyTags, "category_slugs" },
            { "four scenes", [](SubmissionRequest& r) {
                  r.sceneTagSlugs = { "all-ages", "sober", "outdoors", "late-night" }; },
              kSubmissionErrorTooManyTags, "scene_tag_slugs" },
        };

        SubmissionHelper helper(db);
        for (const Case& testCase : cases) {
            SubmissionRequest request = MakeRequest(nowUs);
            testCase.change(request);
            SubmitResult result = helper.Submit(transaction, *secrets, request, personId);
            EXPECT_FALSE(result.ok) << testCase.name;
            EXPECT_EQ(result.errorCode, testCase.errorCode) << testCase.name;
            EXPECT_EQ(result.errorField, testCase.field) << testCase.name;
            EXPECT_EQ(result.errorDateIndex, testCase.dateIndex) << testCase.name;
            EXPECT_FALSE(result.errorMessage.empty()) << testCase.name;
        }
        EXPECT_EQ(TableHelpers::EventSubmissions(db).CountSubmissionsSince(transaction, personId, 0), 0);
    });
}

// The edges of each range are allowed, and lengths count characters, not bytes.
TEST(SubmissionHelperTest, TheEdgesOfTheRangesAreAllowed) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheEdgesOfTheRangesAreAllowed", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);

        SubmissionRequest request = MakeRequest(nowUs);
        // 120 two-byte characters: 240 bytes, but 120 characters.
        std::string title;
        for (int i = 0; i < 120; ++i) title += "\xC3\xA9";
        request.title = title;
        request.dates[0].startsAtUs = nowUs + 2 * kUsPerHour;
        request.dates[0].endsAtUs = request.dates[0].startsAtUs + 4 * kUsPerDay;
        request.dates.push_back({ nowUs + 364 * kUsPerDay, std::nullopt });
        request.url = "http://example.org/" + std::string(481, 'a');  // 500
        request.locationText = std::string(300, 'a');
        request.costText = std::string(100, 'a');
        request.description = std::string(4000, 'a');
        request.submitterNote = std::string(1000, 'a');
        request.categorySlugs = { "dance", "music", "food" };
        request.sceneTagSlugs = { "all-ages", "sober", "outdoors" };
        SubmitResult result = SubmissionHelper(db).Submit(transaction, *secrets, request, personId);
        EXPECT_TRUE(result.ok) << result.errorCode << " " << result.errorField;

        SubmissionRequest shortest = MakeRequest(nowUs);
        shortest.title = "abc";
        shortest.dates[0].startsAtUs += kUsPerHour;  // not the same start as above
        EXPECT_TRUE(SubmissionHelper(db).Submit(transaction, *secrets, shortest, personId).ok);
    });
}

// Twelve dates fit (with the pending limit raised to hold them); a thirteenth doesn't.
TEST(SubmissionHelperTest, TheThirteenthDateIsRefused) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheThirteenthDateIsRefused", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        SubmissionSettingsUpdate update;
        update.maxPendingDates = 30;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, update).ok);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);
        SubmissionHelper helper(db);

        SubmitResult thirteen = helper.Submit(transaction, *secrets, MakeRequest(nowUs, 13), personId);
        EXPECT_EQ(thirteen.errorCode, kSubmissionErrorTooManyDates);
        EXPECT_EQ(thirteen.errorField, "dates");

        SubmitResult twelve = helper.Submit(transaction, *secrets, MakeRequest(nowUs, 12), personId);
        ASSERT_TRUE(twelve.ok) << twelve.errorCode;
        EXPECT_EQ(twelve.eventIds.size(), 12u);
    });
}

TEST(SubmissionHelperTest, AnUnverifiedAccountIsRefused) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnUnverifiedAccountIsRefused", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t unverified = TableHelpers::People(db).AddPerson(
            transaction, "unverified@example.com", "Sam", "Sample", "not-a-real-hash");
        SubmissionHelper helper(db);

        SubmitResult result = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction)), unverified);
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.errorCode, kSubmissionErrorEmailNotVerified);
        // No such person at all reads the same way.
        EXPECT_EQ(helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction)), 999999).errorCode,
                  kSubmissionErrorEmailNotVerified);
        EXPECT_EQ(TableHelpers::EventSubmissions(db).CountSubmissionsSince(transaction, unverified, 0), 0);
    });
}

// A bot fills in the hidden field: it hears success, and nothing is written.
TEST(SubmissionHelperTest, TheHoneypotWritesNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheHoneypotWritesNothing", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionRequest request = MakeRequest(NowUs(transaction), 2);
        request.website = "https://example.org";
        // Even an otherwise-bad form gets the quiet answer.
        request.title = "x";

        SubmissionHelper helper(db);
        SubmitResult result = helper.Submit(transaction, *secrets, request, personId);
        EXPECT_TRUE(result.ok);
        EXPECT_EQ(result.submissionId, 0);
        EXPECT_TRUE(result.eventIds.empty());
        EXPECT_EQ(TableHelpers::EventSubmissions(db).CountSubmissionsSince(transaction, personId, 0), 0);
        EXPECT_TRUE(helper.GetMySubmissions(transaction, personId).empty());
    });
}

TEST(SubmissionHelperTest, UnknownAndDisabledSlugsAreDroppedAndReported) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UnknownAndDisabledSlugsAreDroppedAndReported", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionRequest request = MakeRequest(NowUs(transaction));
        // Four slugs, but only two resolve (one of them given twice).
        request.categorySlugs = { "dance", "nope", "retired", "dance", "music" };
        request.sceneTagSlugs = { "gone" };

        SubmitResult result = SubmissionHelper(db).Submit(transaction, *secrets, request, personId);
        ASSERT_TRUE(result.ok) << result.errorCode;
        EXPECT_EQ(result.ignoredCategorySlugs, (std::vector<std::string>{ "nope", "retired" }));
        EXPECT_EQ(result.ignoredSceneTagSlugs, (std::vector<std::string>{ "gone" }));
        EXPECT_EQ(TableHelpers::EventCategoryAssignments(db).GetForEvent(transaction, result.eventIds[0]).size(), 2u);
        EXPECT_TRUE(TableHelpers::EventSceneTagAssignments(db).GetForEvent(transaction, result.eventIds[0]).empty());
    });
}

// --- Rate limits ---

// The default of 10 waiting dates: reaching it is fine, passing it isn't.
TEST(SubmissionHelperTest, ThePendingDateLimitAtTheDefault) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ThePendingDateLimitAtTheDefault", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);
        SubmissionHelper helper(db);

        auto submit = [&](size_t dates, int64_t offsetUs) {
            SubmissionRequest request = MakeRequest(nowUs, dates);
            for (SubmissionDate& date : request.dates) date.startsAtUs += offsetUs;
            return helper.Submit(transaction, *secrets, request, personId);
        };
        ASSERT_TRUE(submit(4, 0).ok);
        ASSERT_TRUE(submit(4, kUsPerHour).ok);
        ASSERT_TRUE(submit(2, 2 * kUsPerHour).ok);  // exactly 10

        SubmitResult over = submit(1, 3 * kUsPerHour);
        EXPECT_FALSE(over.ok);
        EXPECT_EQ(over.errorCode, kSubmissionErrorRateLimited);
        EXPECT_EQ(over.errorField, kSubmissionLimitMaxPendingDates);
        EXPECT_EQ(over.errorMessage,
                  "You can have up to 10 dates waiting for review at a time. You have 10 waiting, "
                  "and this would add 1. We'll get to them soon.");

        // Another person's limit is their own.
        int64_t otherId = AddVerifiedPerson(transaction, db, "other@example.com");
        EXPECT_TRUE(helper.Submit(transaction, *secrets, MakeRequest(nowUs), otherId).ok);
    });
}

// The default of 5 submissions in 24 hours.
TEST(SubmissionHelperTest, TheDailyLimitAtTheDefault) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheDailyLimitAtTheDefault", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);
        SubmissionHelper helper(db);

        for (int64_t index = 0; index < 5; ++index) {
            SubmissionRequest request = MakeRequest(nowUs);
            request.dates[0].startsAtUs += index * kUsPerHour;
            ASSERT_TRUE(helper.Submit(transaction, *secrets, request, personId).ok) << index;
        }
        SubmitResult over = helper.Submit(transaction, *secrets, MakeRequest(nowUs + kUsPerDay), personId);
        EXPECT_EQ(over.errorCode, kSubmissionErrorRateLimited);
        EXPECT_EQ(over.errorField, kSubmissionLimitMaxSubmissionsPerDay);
        EXPECT_EQ(over.errorMessage, "You can send up to 5 submissions in 24 hours. Please try again later.");
    });
}

// A lowered setting applies to the very next submission, and a decided date
// frees its place.
TEST(SubmissionHelperTest, LoweredLimitsApplyToTheNextSubmission) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("LoweredLimitsApplyToTheNextSubmission", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);
        SubmissionHelper helper(db);

        SubmitResult first = helper.Submit(transaction, *secrets, MakeRequest(nowUs, 2), personId);
        ASSERT_TRUE(first.ok);
        SubmissionSettingsUpdate update;
        update.maxPendingDates = 2;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, update).ok);

        SubmissionRequest next = MakeRequest(nowUs + kUsPerHour);
        SubmitResult refused = helper.Submit(transaction, *secrets, next, personId);
        EXPECT_EQ(refused.errorCode, kSubmissionErrorRateLimited);
        EXPECT_EQ(refused.errorMessage,
                  "You can have up to 2 dates waiting for review at a time. You have 2 waiting, "
                  "and this would add 1. We'll get to them soon.");

        // A curator decides one: there's room again.
        ASSERT_TRUE(helper.ReviewDate(transaction, first.eventIds[0], ReviewAction::Approve,
                                      kCuratorId, "", {}).review.ok);
        EXPECT_TRUE(helper.Submit(transaction, *secrets, next, personId).ok);

        update = {};
        update.maxSubmissionsPerDay = 2;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, update).ok);
        SubmitResult daily = helper.Submit(transaction, *secrets, MakeRequest(nowUs + 2 * kUsPerHour), personId);
        EXPECT_EQ(daily.errorField, kSubmissionLimitMaxSubmissionsPerDay);
        EXPECT_EQ(daily.errorMessage, "You can send up to 2 submissions in 24 hours. Please try again later.");
    });
}

// --- The pause switch ---

// While paused, even a good form is refused and nothing is written; the
// person's own submissions and withdrawing stay available. Resuming lets the
// same form through.
TEST(SubmissionHelperTest, APausedSubmitWritesNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("APausedSubmitWritesNothing", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        const int64_t nowUs = NowUs(transaction);
        SubmissionHelper helper(db);
        SubmitResult before = helper.Submit(transaction, *secrets, MakeRequest(nowUs), personId);
        ASSERT_TRUE(before.ok);

        SubmissionSettingsUpdate pause;
        pause.submissionsPaused = true;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, pause).ok);
        SubmissionRequest next = MakeRequest(nowUs + kUsPerHour, 2);
        SubmitResult paused = helper.Submit(transaction, *secrets, next, personId);
        EXPECT_FALSE(paused.ok);
        EXPECT_EQ(paused.errorCode, kSubmissionErrorPaused);
        EXPECT_TRUE(paused.errorField.empty());
        EXPECT_EQ(paused.errorMessage,
                  "We're not taking event submissions right now. Please check back soon.");
        EXPECT_TRUE(paused.eventIds.empty());
        EXPECT_EQ(TableHelpers::EventSubmissions(db).CountSubmissionsSince(transaction, personId, 0), 1);
        // The switch is checked before the form, so a bad form hears "paused" too.
        SubmissionRequest bad = next;
        bad.title = "x";
        EXPECT_EQ(helper.Submit(transaction, *secrets, bad, personId).errorCode, kSubmissionErrorPaused);

        EXPECT_EQ(helper.GetMySubmissions(transaction, personId).size(), 1u);
        EXPECT_TRUE(helper.Withdraw(transaction, before.submissionId, personId).ok);

        pause.submissionsPaused = false;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, pause).ok);
        SubmitResult resumed = helper.Submit(transaction, *secrets, next, personId);
        ASSERT_TRUE(resumed.ok) << resumed.errorCode;
        EXPECT_EQ(resumed.eventIds.size(), 2u);
    });
}

// --- Duplicate hints, against the table ---

TEST(SubmissionHelperTest, PossibleDuplicatesAreApprovedEventsNearby) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PossibleDuplicatesAreApprovedEventsNearby", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        World world = MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionRequest request = MakeRequest(NowUs(transaction), 2);
        request.venueId = world.venueOne;
        const int64_t startsAtUs = request.dates[0].startsAtUs;

        int64_t byVenue = AddApprovedEvent(transaction, db, "Something Else", startsAtUs + kUsPerHour, world.venueOne);
        int64_t byTitle = AddApprovedEvent(transaction, db, "SAMPLE dance-night", startsAtUs - 2 * kUsPerHour, world.venueTwo);
        // Not hinted: waiting for review (not the submitter's to see), and four hours off.
        AddApprovedEvent(transaction, db, "Sample Dance Night", startsAtUs, world.venueOne,
                         DbSchema::kEventsStatusPending);
        AddApprovedEvent(transaction, db, "Sample Dance Night", startsAtUs + 4 * kUsPerHour, world.venueOne);

        SubmitResult result = SubmissionHelper(db).Submit(transaction, *secrets, request, personId);
        ASSERT_TRUE(result.ok) << result.errorCode;
        ASSERT_EQ(result.possibleDuplicates.size(), 2u);
        std::set<int64_t> hinted;
        for (const DuplicateHint& hint : result.possibleDuplicates) {
            EXPECT_EQ(hint.dateIndex, 0u);
            hinted.insert(hint.eventId);
        }
        EXPECT_EQ(hinted, (std::set<int64_t>{ byVenue, byTitle }));
        // A hint never blocks: both dates were written.
        EXPECT_EQ(result.eventIds.size(), 2u);
    });
}

// --- My submissions and withdrawal ---

TEST(SubmissionHelperTest, MySubmissionsAreNewestFirstWithEachDatesState) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MySubmissionsAreNewestFirstWithEachDatesState", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        int64_t otherId = AddVerifiedPerson(transaction, db, "other@example.com");
        const int64_t nowUs = NowUs(transaction);
        SubmissionHelper helper(db);

        SubmissionRequest olderRequest = MakeRequest(nowUs, 3);
        olderRequest.submitterNote = "Note for the curators";
        SubmitResult older = helper.Submit(transaction, *secrets, olderRequest, personId);
        SubmitResult newer = helper.Submit(transaction, *secrets, MakeRequest(nowUs + kUsPerHour), personId);
        helper.Submit(transaction, *secrets, MakeRequest(nowUs + 2 * kUsPerHour), otherId);
        ASSERT_TRUE(older.ok && newer.ok);

        ASSERT_TRUE(helper.ReviewDate(transaction, older.eventIds[0], ReviewAction::Approve,
                                      kCuratorId, "looks right", {}).review.ok);
        SubmissionDecision decision{ std::string(DbSchema::kEventSubmissionsRejectReasonPast),
                                     std::string("That one's already happened.") };
        ASSERT_TRUE(helper.ReviewDate(transaction, older.eventIds[1], ReviewAction::Reject,
                                      kCuratorId, "curator-only note", decision).review.ok);
        ASSERT_TRUE(helper.Withdraw(transaction, newer.submissionId, personId).ok);

        std::vector<SubmissionView> mine = helper.GetMySubmissions(transaction, personId);
        ASSERT_EQ(mine.size(), 2u);
        EXPECT_EQ(mine[0].id, newer.submissionId);
        EXPECT_EQ(mine[1].id, older.submissionId);

        ASSERT_EQ(mine[0].dates.size(), 1u);
        EXPECT_EQ(mine[0].dates[0].state, kSubmissionDateWithdrawn);
        EXPECT_EQ(mine[0].rejectReason, DbSchema::kEventSubmissionsRejectReasonWithdrawn);

        const SubmissionView& view = mine[1];
        EXPECT_EQ(view.submitterNote, "Note for the curators");
        EXPECT_EQ(view.relationship, DbSchema::kEventSubmissionsRelationshipOther);
        EXPECT_FALSE(view.regularThing);
        EXPECT_EQ(view.rejectReason, DbSchema::kEventSubmissionsRejectReasonPast);
        EXPECT_EQ(view.messageToSubmitter, "That one's already happened.");
        ASSERT_EQ(view.dates.size(), 3u);
        EXPECT_EQ(view.dates[0].eventId, older.eventIds[0]);
        EXPECT_EQ(view.dates[0].state, kSubmissionDateListed);
        EXPECT_EQ(view.dates[1].state, kSubmissionDateNotListed);
        EXPECT_EQ(view.dates[2].state, kSubmissionDateWaiting);
        EXPECT_EQ(view.dates[2].title, "Sample Dance Night");
    });
}

TEST(SubmissionHelperTest, TheOwnerWithdrawsWhatIsStillPending) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheOwnerWithdrawsWhatIsStillPending", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction), 3), personId);
        ASSERT_TRUE(submitted.ok);
        ASSERT_TRUE(helper.ReviewDate(transaction, submitted.eventIds[0], ReviewAction::Approve,
                                      kCuratorId, "", {}).review.ok);

        WithdrawResult withdrawn = helper.Withdraw(transaction, submitted.submissionId, personId);
        ASSERT_TRUE(withdrawn.ok) << withdrawn.errorCode;
        EXPECT_EQ(withdrawn.withdrawnEventIds,
                  (std::vector<int64_t>{ submitted.eventIds[1], submitted.eventIds[2] }));

        // The approved date stays listed.
        EXPECT_EQ(Column(EventRow(transaction, db, submitted.eventIds[0]), DbSchema::kEventsStatus),
                  DbSchema::kEventsStatusApproved);
        for (size_t index : { 1u, 2u }) {
            KeyValueTable event = EventRow(transaction, db, submitted.eventIds[index]);
            EXPECT_EQ(Column(event, DbSchema::kEventsStatus), DbSchema::kEventsStatusRejected);
            EXPECT_EQ(Column(event, DbSchema::kEventsReviewedByPersonId), StringFromInt(personId));
            EXPECT_EQ(Column(event, DbSchema::kEventsReviewNotes), kReviewNoteWithdrawnBySubmitter);
        }
        KeyValueTable submission = SubmissionRow(transaction, db, submitted.submissionId);
        EXPECT_EQ(Column(submission, DbSchema::kEventSubmissionsRejectReason),
                  DbSchema::kEventSubmissionsRejectReasonWithdrawn);
        // Marked as told, so nothing emails about it later.
        EXPECT_FALSE(Column(submission, DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
        EXPECT_FALSE(helper.OnDateReviewed(transaction, submitted.eventIds[0]).has_value());

        EXPECT_EQ(helper.Withdraw(transaction, submitted.submissionId, personId).errorCode,
                  kSubmissionErrorNothingPending);
    });
}

TEST(SubmissionHelperTest, WithdrawKeepsACuratorsReason) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("WithdrawKeepsACuratorsReason", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction), 2), personId);
        ASSERT_TRUE(submitted.ok);
        SubmissionDecision decision{ std::string(DbSchema::kEventSubmissionsRejectReasonDuplicate), std::nullopt };
        ASSERT_TRUE(helper.ReviewDate(transaction, submitted.eventIds[0], ReviewAction::Reject,
                                      kCuratorId, "", decision).review.ok);

        ASSERT_TRUE(helper.Withdraw(transaction, submitted.submissionId, personId).ok);
        EXPECT_EQ(Column(SubmissionRow(transaction, db, submitted.submissionId),
                         DbSchema::kEventSubmissionsRejectReason),
                  DbSchema::kEventSubmissionsRejectReasonDuplicate);
        std::vector<SubmissionView> mine = helper.GetMySubmissions(transaction, personId);
        ASSERT_EQ(mine.size(), 1u);
        EXPECT_EQ(mine[0].dates[0].state, kSubmissionDateNotListed);
        EXPECT_EQ(mine[0].dates[1].state, kSubmissionDateWithdrawn);
    });
}

// Someone else's submission, or one that doesn't exist, reads the same.
TEST(SubmissionHelperTest, OnlyTheOwnerCanWithdraw) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OnlyTheOwnerCanWithdraw", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        int64_t otherId = AddVerifiedPerson(transaction, db, "other@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction)), personId);
        ASSERT_TRUE(submitted.ok);

        EXPECT_EQ(helper.Withdraw(transaction, submitted.submissionId, otherId).errorCode,
                  kSubmissionErrorNotYours);
        EXPECT_EQ(helper.Withdraw(transaction, 999999, otherId).errorCode, kSubmissionErrorNotYours);
        EXPECT_EQ(Column(EventRow(transaction, db, submitted.eventIds[0]), DbSchema::kEventsStatus),
                  DbSchema::kEventsStatusPending);
    });
}

// --- The curator's side and the decision email ---

TEST(SubmissionHelperTest, TheNoticeComesOnceAfterTheLastPendingDate) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheNoticeComesOnceAfterTheLastPendingDate", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction), 3), personId);
        ASSERT_TRUE(submitted.ok);

        DateReviewResult first = helper.ReviewDate(transaction, submitted.eventIds[0],
                                                   ReviewAction::Approve, kCuratorId, "", {});
        ASSERT_TRUE(first.review.ok);
        EXPECT_FALSE(first.notice.has_value());
        SubmissionDecision decision{ std::string(DbSchema::kEventSubmissionsRejectReasonMissingDetails),
                                     std::string("Which room is it in?") };
        DateReviewResult second = helper.ReviewDate(transaction, submitted.eventIds[1],
                                                    ReviewAction::Reject, kCuratorId, "", decision);
        ASSERT_TRUE(second.review.ok);
        EXPECT_FALSE(second.notice.has_value());
        EXPECT_FALSE(helper.OnDateReviewed(transaction, submitted.eventIds[1]).has_value());

        DateReviewResult last = helper.ReviewDate(transaction, submitted.eventIds[2],
                                                  ReviewAction::Approve, kCuratorId, "", {});
        ASSERT_TRUE(last.notice.has_value());
        const DecisionNotice& notice = *last.notice;
        EXPECT_EQ(notice.submissionId, submitted.submissionId);
        EXPECT_EQ(notice.recipientEmail, "submitter@example.com");
        EXPECT_EQ(notice.recipientFirstName, "Sam");
        EXPECT_EQ(notice.rejectReason, DbSchema::kEventSubmissionsRejectReasonMissingDetails);
        EXPECT_EQ(notice.messageToSubmitter, "Which room is it in?");
        ASSERT_EQ(notice.dates.size(), 3u);
        EXPECT_EQ(notice.dates[0].status, DbSchema::kEventsStatusApproved);
        EXPECT_EQ(notice.dates[1].status, DbSchema::kEventsStatusRejected);
        EXPECT_EQ(notice.dates[2].status, DbSchema::kEventsStatusApproved);
        EXPECT_FALSE(Column(SubmissionRow(transaction, db, submitted.submissionId),
                            DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());

        // Claimed: a later correction doesn't send a second email.
        DateReviewResult correction = helper.ReviewDate(transaction, submitted.eventIds[0],
                                                        ReviewAction::Reject, kCuratorId, "", {});
        ASSERT_TRUE(correction.review.ok);
        EXPECT_FALSE(correction.notice.has_value());
        EXPECT_FALSE(helper.OnDateReviewed(transaction, submitted.eventIds[2]).has_value());
    });
}

// A date from a feed or typed in by a curator has no submission: the reason and
// message have nowhere to go, and there is no one to email.
TEST(SubmissionHelperTest, EventsFromElsewhereHaveNoNotice) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EventsFromElsewhereHaveNoNotice", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        int64_t eventId = AddApprovedEvent(transaction, db, "Sample Event", NowUs(transaction) + kUsPerDay,
                                           std::nullopt, DbSchema::kEventsStatusPending);
        SubmissionHelper helper(db);
        SubmissionDecision decision{ std::string(DbSchema::kEventSubmissionsRejectReasonSpam),
                                     std::string("Not for us.") };
        DateReviewResult result = helper.ReviewDate(transaction, eventId, ReviewAction::Reject,
                                                    kCuratorId, "", decision);
        ASSERT_TRUE(result.review.ok) << result.review.errorCode;
        EXPECT_FALSE(result.notice.has_value());
        EXPECT_FALSE(helper.OnDateReviewed(transaction, eventId).has_value());
        EXPECT_EQ(helper.ReviewDate(transaction, 999999, ReviewAction::Approve, kCuratorId, "", {})
                      .review.errorCode,
                  kEventErrorNotFound);
    });
}

TEST(SubmissionHelperTest, TheDecisionIsValidated) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheDecisionIsValidated", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction)), personId);
        ASSERT_TRUE(submitted.ok);
        const int64_t eventId = submitted.eventIds[0];

        SubmissionDecision onApproval{ std::string(DbSchema::kEventSubmissionsRejectReasonPast), std::nullopt };
        EXPECT_EQ(helper.ReviewDate(transaction, eventId, ReviewAction::Approve, kCuratorId, "", onApproval)
                      .review.errorCode,
                  kSubmissionErrorBadReason);
        // `withdrawn` and `expired` are the system's, not a curator's.
        SubmissionDecision systemReason{ std::string(DbSchema::kEventSubmissionsRejectReasonWithdrawn), std::nullopt };
        EXPECT_EQ(helper.ReviewDate(transaction, eventId, ReviewAction::Reject, kCuratorId, "", systemReason)
                      .review.errorCode,
                  kSubmissionErrorBadReason);
        SubmissionDecision longMessage{ "", std::string(2001, 'a') };
        EXPECT_EQ(helper.ReviewSubmission(transaction, submitted.submissionId, ReviewAction::Reject,
                                          kCuratorId, "", longMessage).errorCode,
                  kSubmissionErrorFieldTooLong);
        EXPECT_EQ(Column(EventRow(transaction, db, eventId), DbSchema::kEventsStatus),
                  DbSchema::kEventsStatusPending);
    });
}

TEST(SubmissionHelperTest, ReviewSubmissionDecidesEveryPendingDate) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ReviewSubmissionDecidesEveryPendingDate", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction), 3), personId);
        ASSERT_TRUE(submitted.ok);
        ASSERT_TRUE(helper.ReviewDate(transaction, submitted.eventIds[0], ReviewAction::Approve,
                                      kCuratorId, "", {}).review.ok);

        SubmissionDecision decision{ std::string(DbSchema::kEventSubmissionsRejectReasonNotAFit),
                                     std::string("Thanks anyway!") };
        SubmissionReviewResult result = helper.ReviewSubmission(
            transaction, submitted.submissionId, ReviewAction::Reject, kCuratorId, "group note", decision);
        ASSERT_TRUE(result.ok) << result.errorCode;
        EXPECT_EQ(result.decidedEventIds,
                  (std::vector<int64_t>{ submitted.eventIds[1], submitted.eventIds[2] }));
        EXPECT_EQ(Column(EventRow(transaction, db, submitted.eventIds[0]), DbSchema::kEventsStatus),
                  DbSchema::kEventsStatusApproved);
        KeyValueTable rejected = EventRow(transaction, db, submitted.eventIds[2]);
        EXPECT_EQ(Column(rejected, DbSchema::kEventsStatus), DbSchema::kEventsStatusRejected);
        EXPECT_EQ(Column(rejected, DbSchema::kEventsReviewNotes), "group note");
        EXPECT_EQ(Column(rejected, DbSchema::kEventsReviewedByPersonId), StringFromInt(kCuratorId));
        ASSERT_TRUE(result.notice.has_value());
        EXPECT_EQ(result.notice->rejectReason, DbSchema::kEventSubmissionsRejectReasonNotAFit);
        EXPECT_EQ(result.notice->messageToSubmitter, "Thanks anyway!");

        EXPECT_EQ(helper.ReviewSubmission(transaction, submitted.submissionId, ReviewAction::Approve,
                                          kCuratorId, "", {}).errorCode,
                  kSubmissionErrorNothingPending);
        EXPECT_EQ(helper.ReviewSubmission(transaction, 999999, ReviewAction::Approve, kCuratorId, "", {})
                      .errorCode,
                  kSubmissionErrorNotFound);
    });
}

// The account is gone: the claim is taken, so nothing tries to email it again.
TEST(SubmissionHelperTest, ADeletedAccountGetsNoNotice) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("ADeletedAccountGetsNoNotice", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction)), personId);
        ASSERT_TRUE(submitted.ok);
        TableHelpers::People(db).DeletePerson(transaction, personId);

        SubmissionReviewResult result = helper.ReviewSubmission(
            transaction, submitted.submissionId, ReviewAction::Approve, kCuratorId, "", {});
        ASSERT_TRUE(result.ok);
        EXPECT_FALSE(result.notice.has_value());
        EXPECT_FALSE(Column(SubmissionRow(transaction, db, submitted.submissionId),
                            DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
    });
}

TEST(SubmissionHelperTest, SendDecisionNoticeSendsTheEmail) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SendDecisionNoticeSendsTheEmail", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction), 2), personId);
        ASSERT_TRUE(submitted.ok);
        SubmissionReviewResult result = helper.ReviewSubmission(
            transaction, submitted.submissionId, ReviewAction::Approve, kCuratorId, "", {});
        ASSERT_TRUE(result.notice.has_value());

        ::Mail::Test::TestMailHelperPtr mail = ::Mail::Test::MakeTestMailHelper();
        EXPECT_TRUE(helper.SendDecisionNotice(transaction, *secrets, mail, *result.notice));
        ASSERT_EQ(mail->GetMessages().size(), 1u);
        const ::Mail::MailMessage& message = mail->GetMessages()[0];
        ASSERT_EQ(message.GetTo().size(), 1u);
        EXPECT_EQ(message.GetTo()[0].address, "submitter@example.com");
        EXPECT_EQ(message.GetTo()[0].name, "Sam Sample");
        EXPECT_EQ(message.GetSubject(), "Your event is listed: Sample Dance Night");
        EXPECT_FALSE(Column(SubmissionRow(transaction, db, submitted.submissionId),
                            DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
    });
}

// A failed send never fails the review: it leaves an admin alert and releases
// the claim, so the next decision tries again.
TEST(SubmissionHelperTest, AFailedSendAlertsAndReleasesTheClaim) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AFailedSendAlertsAndReleasesTheClaim", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        MakeWorld(transaction, db);
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        int64_t personId = AddVerifiedPerson(transaction, db, "submitter@example.com");
        SubmissionHelper helper(db);
        SubmitResult submitted = helper.Submit(transaction, *secrets, MakeRequest(NowUs(transaction)), personId);
        ASSERT_TRUE(submitted.ok);
        SubmissionReviewResult result = helper.ReviewSubmission(
            transaction, submitted.submissionId, ReviewAction::Approve, kCuratorId, "", {});
        ASSERT_TRUE(result.notice.has_value());

        ::Mail::MailHelperPtr failing = std::make_shared<FailingMailHelper>();
        EXPECT_FALSE(helper.SendDecisionNotice(transaction, *secrets, failing, *result.notice));
        EXPECT_TRUE(Column(SubmissionRow(transaction, db, submitted.submissionId),
                           DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
        const std::string alertStart = "Event submission #" + StringFromInt(submitted.submissionId) + ":";
        size_t alerts = 0;
        for (const KeyValueTable& alert : TableHelpers::AdminAlerts(db).GetAdminAlertsSince(transaction, 0)) {
            const std::string description = Column(alert, DbSchema::kAdminAlertsDescription);
            if (description.rfind(alertStart, 0) == 0) {
                ++alerts;
                EXPECT_NE(description.find("smtp unavailable"), std::string::npos) << description;
            }
        }
        EXPECT_EQ(alerts, 1u);

        // Released: the next look claims it again. No mail helper at all is a
        // failure too.
        std::optional<DecisionNotice> again = helper.OnDateReviewed(transaction, submitted.eventIds[0]);
        ASSERT_TRUE(again.has_value());
        EXPECT_FALSE(helper.SendDecisionNotice(transaction, *secrets, nullptr, *again));
        EXPECT_TRUE(Column(SubmissionRow(transaction, db, submitted.submissionId),
                           DbSchema::kEventSubmissionsDecisionNotifiedUs).empty());
    });
}

}  // namespace
}  // namespace Events
