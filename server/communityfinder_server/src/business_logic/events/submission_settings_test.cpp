#include "business_logic/events/submission_settings.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "business_logic/app_secret_values.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// EV3 §3.3 / §3.4 — the submission limits and the pause switch, through the
// real config_secrets store (the test database starts with no rows in it).
namespace Events {
namespace {

TEST(SubmissionSettingsTest, NothingStoredReadsTheDefaults) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NothingStoredReadsTheDefaults", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        SubmissionSettings settings = LoadSubmissionSettings(*secrets, transaction);
        EXPECT_EQ(settings.maxPendingDates, 10);
        EXPECT_EQ(settings.maxSubmissionsPerDay, 5);
        EXPECT_FALSE(settings.submissionsPaused);
    });
}

TEST(SubmissionSettingsTest, SaveThenLoad) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SaveThenLoad", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        SubmissionSettingsUpdate update;
        update.maxPendingDates = 25;
        update.maxSubmissionsPerDay = 2;
        update.submissionsPaused = true;
        SaveSettingsResult saved = SaveSubmissionSettings(*secrets, transaction, update);
        ASSERT_TRUE(saved.ok) << saved.errorCode;
        EXPECT_EQ(saved.settings.maxPendingDates, 25);
        EXPECT_EQ(saved.settings.maxSubmissionsPerDay, 2);
        EXPECT_TRUE(saved.settings.submissionsPaused);

        SubmissionSettings loaded = LoadSubmissionSettings(*secrets, transaction);
        EXPECT_EQ(loaded.maxPendingDates, 25);
        EXPECT_EQ(loaded.maxSubmissionsPerDay, 2);
        EXPECT_TRUE(loaded.submissionsPaused);
        EXPECT_EQ(secrets->LookupSecret(transaction, Secrets::kEventSubmissionsPaused), "true");

        SubmissionSettingsUpdate resume;
        resume.submissionsPaused = false;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, resume).ok);
        EXPECT_FALSE(LoadSubmissionSettings(*secrets, transaction).submissionsPaused);
        EXPECT_EQ(secrets->LookupSecret(transaction, Secrets::kEventSubmissionsPaused), "false");
    });
}

// The ends of each range are allowed.
TEST(SubmissionSettingsTest, TheRangeEndsSave) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheRangeEndsSave", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        SubmissionSettingsUpdate low;
        low.maxPendingDates = 1;
        low.maxSubmissionsPerDay = 1;
        EXPECT_TRUE(SaveSubmissionSettings(*secrets, transaction, low).ok);
        SubmissionSettingsUpdate high;
        high.maxPendingDates = 100;
        high.maxSubmissionsPerDay = 50;
        SaveSettingsResult saved = SaveSubmissionSettings(*secrets, transaction, high);
        ASSERT_TRUE(saved.ok);
        EXPECT_EQ(saved.settings.maxPendingDates, 100);
        EXPECT_EQ(saved.settings.maxSubmissionsPerDay, 50);
    });
}

// A bad row can never switch a limit off: blank, junk and out-of-range values
// all read as the default.
TEST(SubmissionSettingsTest, BadStoredValuesReadAsTheDefault) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BadStoredValuesReadAsTheDefault", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (std::string_view bad : { "", "lots", "7x", " 7", "0", "-3", "101", "99999999999999999999" }) {
            secrets->AddSecret(transaction, Secrets::kEventSubmissionMaxPendingDates, bad);
            secrets->AddSecret(transaction, Secrets::kEventSubmissionMaxPerDay, bad);
            SubmissionSettings settings = LoadSubmissionSettings(*secrets, transaction);
            EXPECT_EQ(settings.maxPendingDates, 10) << "\"" << bad << "\"";
            EXPECT_EQ(settings.maxSubmissionsPerDay, 5) << "\"" << bad << "\"";
        }
        // 51 is past the per-day range but inside the pending one.
        secrets->AddSecret(transaction, Secrets::kEventSubmissionMaxPendingDates, "51");
        secrets->AddSecret(transaction, Secrets::kEventSubmissionMaxPerDay, "51");
        SubmissionSettings settings = LoadSubmissionSettings(*secrets, transaction);
        EXPECT_EQ(settings.maxPendingDates, 51);
        EXPECT_EQ(settings.maxSubmissionsPerDay, 5);
    });
}

// A bad row can never close the form either: only the exact "true" pauses.
TEST(SubmissionSettingsTest, OnlyTrueStoredPausesSubmissions) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OnlyTrueStoredPausesSubmissions", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (std::string_view open : { "", "false", "yes", "TRUE", "True", "1", " true", "true " }) {
            secrets->AddSecret(transaction, Secrets::kEventSubmissionsPaused, open);
            EXPECT_FALSE(LoadSubmissionSettings(*secrets, transaction).submissionsPaused)
                << "\"" << open << "\"";
        }
        secrets->AddSecret(transaction, Secrets::kEventSubmissionsPaused, "true");
        EXPECT_TRUE(LoadSubmissionSettings(*secrets, transaction).submissionsPaused);
    });
}

// One bad field refuses the whole save: the good fields aren't written either,
// the pause switch included.
TEST(SubmissionSettingsTest, OneBadFieldWritesNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OneBadFieldWritesNothing", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        SubmissionSettingsUpdate update;
        update.maxPendingDates = 20;
        update.maxSubmissionsPerDay = 0;
        update.submissionsPaused = true;
        SaveSettingsResult saved = SaveSubmissionSettings(*secrets, transaction, update);
        EXPECT_FALSE(saved.ok);
        EXPECT_EQ(saved.errorCode, kSubmissionErrorLimitOutOfRange);
        EXPECT_EQ(saved.field, kSubmissionLimitMaxSubmissionsPerDay);
        EXPECT_TRUE(secrets->LookupSecret(transaction, Secrets::kEventSubmissionMaxPendingDates).empty());
        EXPECT_TRUE(secrets->LookupSecret(transaction, Secrets::kEventSubmissionsPaused).empty());

        update.maxPendingDates = 101;
        update.maxSubmissionsPerDay = 3;
        saved = SaveSubmissionSettings(*secrets, transaction, update);
        EXPECT_FALSE(saved.ok);
        EXPECT_EQ(saved.field, kSubmissionLimitMaxPendingDates);
        EXPECT_TRUE(secrets->LookupSecret(transaction, Secrets::kEventSubmissionMaxPerDay).empty());
        EXPECT_FALSE(LoadSubmissionSettings(*secrets, transaction).submissionsPaused);
    });
}

TEST(SubmissionSettingsTest, SavingOneFieldLeavesTheOthers) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SavingOneFieldLeavesTheOthers", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        SubmissionSettingsUpdate both;
        both.maxPendingDates = 30;
        both.maxSubmissionsPerDay = 8;
        ASSERT_TRUE(SaveSubmissionSettings(*secrets, transaction, both).ok);

        SubmissionSettingsUpdate one;
        one.maxSubmissionsPerDay = 3;
        SaveSettingsResult saved = SaveSubmissionSettings(*secrets, transaction, one);
        ASSERT_TRUE(saved.ok);
        EXPECT_EQ(saved.settings.maxPendingDates, 30);
        EXPECT_EQ(saved.settings.maxSubmissionsPerDay, 3);
        EXPECT_FALSE(saved.settings.submissionsPaused);

        // The switch alone leaves the limits; a limit alone leaves the switch.
        SubmissionSettingsUpdate pause;
        pause.submissionsPaused = true;
        saved = SaveSubmissionSettings(*secrets, transaction, pause);
        ASSERT_TRUE(saved.ok);
        EXPECT_EQ(saved.settings.maxPendingDates, 30);
        EXPECT_EQ(saved.settings.maxSubmissionsPerDay, 3);
        EXPECT_TRUE(saved.settings.submissionsPaused);
        SubmissionSettingsUpdate limit;
        limit.maxPendingDates = 40;
        saved = SaveSubmissionSettings(*secrets, transaction, limit);
        ASSERT_TRUE(saved.ok);
        EXPECT_EQ(saved.settings.maxPendingDates, 40);
        EXPECT_TRUE(saved.settings.submissionsPaused);
    });
}

// The seeded values and the read-side defaults are written in two places; this
// keeps them from drifting apart.
TEST(SubmissionSettingsTest, AppSecretDefaultsMatchTheCodeDefaults) {
    std::map<std::string, std::string> defaults;
    App::FillInAppSecretDefaults([&](std::string_view key, std::string_view value) {
        defaults[std::string(key)] = std::string(value);
    });
    EXPECT_EQ(defaults[std::string(Secrets::kEventSubmissionMaxPendingDates)],
              StringFromInt(kMaxPendingDatesRange.defaultValue));
    EXPECT_EQ(defaults[std::string(Secrets::kEventSubmissionMaxPerDay)],
              StringFromInt(kMaxSubmissionsPerDayRange.defaultValue));
    EXPECT_EQ(defaults[std::string(Secrets::kEventSubmissionsPaused)],
              kSubmissionsPausedDefault ? "true" : "false");
}

}  // namespace
}  // namespace Events
