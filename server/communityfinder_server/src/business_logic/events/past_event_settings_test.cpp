#include "business_logic/events/past_event_settings.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "business_logic/app_secret_values.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// EV4 §10 — the past-events window and the calendar's default, through the real
// config_secrets store (the test database starts with no rows in it).
namespace Events {
namespace {

TEST(PastEventSettingsTest, NothingStoredReadsTheDefaults) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NothingStoredReadsTheDefaults", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        PastEventSettings settings = LoadPastEventSettings(*secrets, transaction);
        EXPECT_EQ(settings.archivedEventsVisibleDays, 365);
        EXPECT_FALSE(settings.showPastByDefault);
    });
}

TEST(PastEventSettingsTest, SaveThenLoad) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SaveThenLoad", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        PastEventSettingsUpdate update;
        update.archivedEventsVisibleDays = 90;
        update.showPastByDefault = true;
        SavePastEventSettingsResult saved = SavePastEventSettings(*secrets, transaction, update);
        ASSERT_TRUE(saved.ok) << saved.errorCode;
        EXPECT_EQ(saved.settings.archivedEventsVisibleDays, 90);
        EXPECT_TRUE(saved.settings.showPastByDefault);

        PastEventSettings loaded = LoadPastEventSettings(*secrets, transaction);
        EXPECT_EQ(loaded.archivedEventsVisibleDays, 90);
        EXPECT_TRUE(loaded.showPastByDefault);
        EXPECT_EQ(secrets->LookupSecret(transaction, Secrets::kEventsShowPastByDefault), "true");

        PastEventSettingsUpdate off;
        off.showPastByDefault = false;
        ASSERT_TRUE(SavePastEventSettings(*secrets, transaction, off).ok);
        EXPECT_FALSE(LoadPastEventSettings(*secrets, transaction).showPastByDefault);
        EXPECT_EQ(secrets->LookupSecret(transaction, Secrets::kEventsShowPastByDefault), "false");
        // The switch alone leaves the window.
        EXPECT_EQ(LoadPastEventSettings(*secrets, transaction).archivedEventsVisibleDays, 90);
    });
}

TEST(PastEventSettingsTest, TheRangeEndsSave) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheRangeEndsSave", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (int64_t days : { int64_t{ 1 }, int64_t{ 730 } }) {
            PastEventSettingsUpdate update;
            update.archivedEventsVisibleDays = days;
            SavePastEventSettingsResult saved = SavePastEventSettings(*secrets, transaction, update);
            ASSERT_TRUE(saved.ok) << days;
            EXPECT_EQ(saved.settings.archivedEventsVisibleDays, days);
        }
    });
}

// A bad row reads as the default window: blank, junk and out of range alike.
TEST(PastEventSettingsTest, BadStoredValuesReadAsTheDefault) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BadStoredValuesReadAsTheDefault", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (std::string_view bad : { "", "a year", "30x", " 30", "0", "-5", "731", "99999999999999999999" }) {
            secrets->AddSecret(transaction, Secrets::kArchivedEventsVisibleDays, bad);
            EXPECT_EQ(LoadPastEventSettings(*secrets, transaction).archivedEventsVisibleDays, 365)
                << "\"" << bad << "\"";
        }
        secrets->AddSecret(transaction, Secrets::kArchivedEventsVisibleDays, "30");
        EXPECT_EQ(LoadPastEventSettings(*secrets, transaction).archivedEventsVisibleDays, 30);
    });
}

TEST(PastEventSettingsTest, OnlyTrueStoredStartsTheSwitchOn) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OnlyTrueStoredStartsTheSwitchOn", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (std::string_view off : { "", "false", "yes", "TRUE", "1", "true " }) {
            secrets->AddSecret(transaction, Secrets::kEventsShowPastByDefault, off);
            EXPECT_FALSE(LoadPastEventSettings(*secrets, transaction).showPastByDefault)
                << "\"" << off << "\"";
        }
        secrets->AddSecret(transaction, Secrets::kEventsShowPastByDefault, "true");
        EXPECT_TRUE(LoadPastEventSettings(*secrets, transaction).showPastByDefault);
    });
}

// An out-of-range window refuses the whole save, the switch included.
TEST(PastEventSettingsTest, AnOutOfRangeWindowWritesNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnOutOfRangeWindowWritesNothing", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (int64_t bad : { int64_t{ 0 }, int64_t{ 731 } }) {
            PastEventSettingsUpdate update;
            update.archivedEventsVisibleDays = bad;
            update.showPastByDefault = true;
            SavePastEventSettingsResult saved = SavePastEventSettings(*secrets, transaction, update);
            EXPECT_FALSE(saved.ok) << bad;
            EXPECT_EQ(saved.errorCode, kPastEventErrorDaysOutOfRange);
            EXPECT_EQ(saved.field, kPastEventSettingVisibleDays);
        }
        EXPECT_TRUE(secrets->LookupSecret(transaction, Secrets::kArchivedEventsVisibleDays).empty());
        EXPECT_TRUE(secrets->LookupSecret(transaction, Secrets::kEventsShowPastByDefault).empty());
    });
}

// The seeded values and the read-side defaults are written in two places; this
// keeps them from drifting apart.
TEST(PastEventSettingsTest, AppSecretDefaultsMatchTheCodeDefaults) {
    std::map<std::string, std::string> defaults;
    App::FillInAppSecretDefaults([&](std::string_view key, std::string_view value) {
        defaults[std::string(key)] = std::string(value);
    });
    EXPECT_EQ(defaults[std::string(Secrets::kArchivedEventsVisibleDays)],
              StringFromInt(kArchivedEventsVisibleDaysRange.defaultValue));
    EXPECT_EQ(defaults[std::string(Secrets::kEventsShowPastByDefault)],
              kEventsShowPastByDefaultDefault ? "true" : "false");
}

}  // namespace
}  // namespace Events
