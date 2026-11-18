#include "business_logic/events/gallery_settings.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "business_logic/app_secret_values.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// CM3 OQ5 — the gallery cap, through the real config_secrets store (the test
// database starts with no rows in it).
namespace Events {
namespace {

TEST(GallerySettingsTest, NothingStoredReadsTheDefault) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NothingStoredReadsTheDefault", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        EXPECT_EQ(LoadGallerySettings(*secrets, transaction).maxPhotos, 12);
    });
}

TEST(GallerySettingsTest, SaveThenLoad) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SaveThenLoad", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        GallerySettingsUpdate update;
        update.maxPhotos = 20;
        SaveGallerySettingsResult saved = SaveGallerySettings(*secrets, transaction, update);
        ASSERT_TRUE(saved.ok) << saved.errorCode;
        EXPECT_EQ(saved.settings.maxPhotos, 20);
        EXPECT_EQ(LoadGallerySettings(*secrets, transaction).maxPhotos, 20);
        EXPECT_EQ(secrets->LookupSecret(transaction, Secrets::kEventGalleryMaxPhotos), "20");

        // An empty update changes nothing.
        ASSERT_TRUE(SaveGallerySettings(*secrets, transaction, {}).ok);
        EXPECT_EQ(LoadGallerySettings(*secrets, transaction).maxPhotos, 20);
    });
}

TEST(GallerySettingsTest, TheRangeEndsSave) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("TheRangeEndsSave", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (int64_t cap : { int64_t{ 1 }, int64_t{ 60 } }) {
            GallerySettingsUpdate update;
            update.maxPhotos = cap;
            SaveGallerySettingsResult saved = SaveGallerySettings(*secrets, transaction, update);
            ASSERT_TRUE(saved.ok) << cap;
            EXPECT_EQ(saved.settings.maxPhotos, cap);
        }
    });
}

TEST(GallerySettingsTest, BadStoredValuesReadAsTheDefault) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BadStoredValuesReadAsTheDefault", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (std::string_view bad : { "", "a dozen", "12x", " 12", "0", "-1", "61", "99999999999999999999" }) {
            secrets->AddSecret(transaction, Secrets::kEventGalleryMaxPhotos, bad);
            EXPECT_EQ(LoadGallerySettings(*secrets, transaction).maxPhotos, 12) << "\"" << bad << "\"";
        }
        secrets->AddSecret(transaction, Secrets::kEventGalleryMaxPhotos, "5");
        EXPECT_EQ(LoadGallerySettings(*secrets, transaction).maxPhotos, 5);
    });
}

TEST(GallerySettingsTest, AnOutOfRangeCapWritesNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnOutOfRangeCapWritesNothing", [&](Transaction& transaction) {
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(testDb.GetDatabaseHelper());
        for (int64_t bad : { int64_t{ 0 }, int64_t{ 61 } }) {
            GallerySettingsUpdate update;
            update.maxPhotos = bad;
            SaveGallerySettingsResult saved = SaveGallerySettings(*secrets, transaction, update);
            EXPECT_FALSE(saved.ok) << bad;
            EXPECT_EQ(saved.errorCode, kGalleryErrorMaxPhotosOutOfRange);
            EXPECT_EQ(saved.field, kGallerySettingMaxPhotos);
        }
        EXPECT_TRUE(secrets->LookupSecret(transaction, Secrets::kEventGalleryMaxPhotos).empty());
    });
}

// The seeded value and the read-side default are written in two places; this
// keeps them from drifting apart.
TEST(GallerySettingsTest, AppSecretDefaultMatchesTheCodeDefault) {
    std::map<std::string, std::string> defaults;
    App::FillInAppSecretDefaults([&](std::string_view key, std::string_view value) {
        defaults[std::string(key)] = std::string(value);
    });
    EXPECT_EQ(defaults[std::string(Secrets::kEventGalleryMaxPhotos)],
              StringFromInt(kEventGalleryMaxPhotosRange.defaultValue));
}

}  // namespace
}  // namespace Events
