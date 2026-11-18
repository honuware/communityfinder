#include "business_logic/events/event_gallery_helper.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/images/image_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/photo_test_support.h"
#include "sql_util/table_helpers/series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// CM3 §3.3 — the gallery: adding a row (the checks, the consent stamp, the
// cap, the order) and deleting one with its photo.
namespace Events {
namespace {

constexpr int64_t kCuratorId = 7;

struct Parents {
    int64_t eventId = 0;
    int64_t seriesId = 0;
};

Parents AddParents(Transaction& transaction, DatabaseHelper databaseHelper) {
    Parents parents;
    parents.seriesId = TableHelpers::Series(databaseHelper).AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), "Sample Night" },
        { std::string(DbSchema::kSeriesSlug), "sample-night" },
    });
    parents.eventId = TableHelpers::Events(databaseHelper).AddEvent(transaction, {
        { std::string(DbSchema::kEventsTitle), "Sample Night" },
        { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
    });
    return parents;
}

NewGalleryPhoto EventPhoto(int64_t eventId, std::string_view altText) {
    NewGalleryPhoto photo;
    photo.eventId = eventId;
    photo.altText = std::string(altText);
    photo.consentConfirmed = true;
    return photo;
}

std::vector<std::string> AltTexts(const std::vector<GalleryPhoto>& gallery) {
    std::vector<std::string> altTexts;
    for (const GalleryPhoto& photo : gallery) {
        altTexts.push_back(photo.altText);
    }
    return altTexts;
}

TEST(EventGalleryHelperTest, AddRecordsTheConsentAndGoesLast) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddRecordsTheConsentAndGoesLast", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryHelper helper(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        const int64_t beforeUs =
            std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));

        NewGalleryPhoto first = EventPhoto(parents.eventId, "  The stage  ");
        first.caption = " Doors at nine ";
        first.credit = "Sample Photographer";
        AddGalleryPhotoResult added = helper.AddGalleryPhoto(transaction, first, kCuratorId, 12);
        ASSERT_TRUE(added.ok) << added.errorCode;
        AddGalleryPhotoResult second =
            helper.AddGalleryPhoto(transaction, EventPhoto(parents.eventId, "The bar"), kCuratorId, 12);
        ASSERT_TRUE(second.ok) << second.errorCode;

        TableHelpers::EventGalleryPhotos rows(databaseHelper);
        KeyValueTable row = rows.GetGalleryPhoto(transaction, added.id);
        // Trimmed, on the event, stamped with the curator and now.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosAltText)), "The stage");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosCaption)), "Doors at nine");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosCredit)), "Sample Photographer");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosEventId)), StringFromInt(parents.eventId));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosSeriesId)), "");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId)),
                  StringFromInt(kCuratorId));
        EXPECT_GE(std::stoll(row.at(std::string(DbSchema::kEventGalleryPhotosConsentConfirmedUs))), beforeUs);
        // Each add goes after the last.
        EXPECT_LT(std::stoll(row.at(std::string(DbSchema::kEventGalleryPhotosSortOrder))),
                  std::stoll(rows.GetGalleryPhoto(transaction, second.id)
                                 .at(std::string(DbSchema::kEventGalleryPhotosSortOrder))));
    });
}

// A curator who reordered the gallery (sort_order 10) still adds at the end.
TEST(EventGalleryHelperTest, AddGoesAfterTheHighestSortOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddGoesAfterTheHighestSortOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryHelper helper(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        TableHelpers::EventGalleryPhotos rows(databaseHelper);
        int64_t moved = helper.AddGalleryPhoto(
            transaction, EventPhoto(parents.eventId, "moved"), kCuratorId, 12).id;
        rows.UpdateGalleryPhoto(transaction, moved, {
            { std::string(DbSchema::kEventGalleryPhotosSortOrder), "10" } });

        int64_t added = helper.AddGalleryPhoto(
            transaction, EventPhoto(parents.eventId, "added"), kCuratorId, 12).id;
        EXPECT_EQ(rows.GetGalleryPhoto(transaction, added).at(std::string(DbSchema::kEventGalleryPhotosSortOrder)),
                  "11");
    });
}

TEST(EventGalleryHelperTest, AddChecksEachField) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddChecksEachField", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryHelper helper(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        auto expectRefused = [&](const NewGalleryPhoto& photo, std::string_view errorCode,
                                 std::string_view field) {
            AddGalleryPhotoResult result = helper.AddGalleryPhoto(transaction, photo, kCuratorId, 12);
            EXPECT_FALSE(result.ok) << errorCode;
            EXPECT_EQ(result.errorCode, errorCode);
            EXPECT_EQ(result.field, field) << errorCode;
        };

        NewGalleryPhoto neither = EventPhoto(0, "nowhere");
        expectRefused(neither, kGalleryErrorOneParent, kGalleryFieldEventId);
        NewGalleryPhoto both = EventPhoto(parents.eventId, "both");
        both.seriesId = parents.seriesId;
        expectRefused(both, kGalleryErrorOneParent, kGalleryFieldEventId);
        expectRefused(EventPhoto(parents.eventId + 1000, "lost"), kGalleryErrorParentNotFound,
                      kGalleryFieldEventId);
        NewGalleryPhoto lostSeries = EventPhoto(0, "lost");
        lostSeries.seriesId = parents.seriesId + 1000;
        expectRefused(lostSeries, kGalleryErrorParentNotFound, kGalleryFieldSeriesId);

        expectRefused(EventPhoto(parents.eventId, "   "), kGalleryErrorAltTextRequired,
                      kGalleryFieldAltText);
        expectRefused(EventPhoto(parents.eventId, std::string(301, 'a')), kGalleryErrorTooLong,
                      kGalleryFieldAltText);
        NewGalleryPhoto longCaption = EventPhoto(parents.eventId, "ok");
        longCaption.caption = std::string(301, 'c');
        expectRefused(longCaption, kGalleryErrorTooLong, kGalleryFieldCaption);
        NewGalleryPhoto longCredit = EventPhoto(parents.eventId, "ok");
        longCredit.credit = std::string(121, 'c');
        expectRefused(longCredit, kGalleryErrorTooLong, kGalleryFieldCredit);
        NewGalleryPhoto unvouched = EventPhoto(parents.eventId, "ok");
        unvouched.consentConfirmed = false;
        expectRefused(unvouched, kGalleryErrorConsentRequired, kGalleryFieldConsentConfirmed);

        // Lengths are characters, as the database counts them: 300 two-byte
        // letters fit.
        std::string accented;
        for (int i = 0; i < 300; ++i) accented += "\xC3\xA9";  // é
        EXPECT_TRUE(helper.AddGalleryPhoto(
            transaction, EventPhoto(parents.eventId, accented), kCuratorId, 12).ok);

        // Only that one was written.
        EXPECT_EQ(TableHelpers::EventGalleryPhotos(databaseHelper)
                      .GetGalleryForEvent(transaction, parents.eventId).size(), 1u);
    });
}

// The cap counts the gallery's rows, with their photos or not; another
// parent's gallery has its own.
TEST(EventGalleryHelperTest, AFullGalleryRefusesAnother) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AFullGalleryRefusesAnother", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryHelper helper(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(helper.AddGalleryPhoto(
                transaction, EventPhoto(parents.eventId, "photo"), kCuratorId, 3).ok);
        }
        AddGalleryPhotoResult full =
            helper.AddGalleryPhoto(transaction, EventPhoto(parents.eventId, "one more"), kCuratorId, 3);
        EXPECT_FALSE(full.ok);
        EXPECT_EQ(full.errorCode, kGalleryErrorFull);
        EXPECT_EQ(full.maxPhotos, 3);

        NewGalleryPhoto seriesPhoto = EventPhoto(0, "series photo");
        seriesPhoto.seriesId = parents.seriesId;
        EXPECT_TRUE(helper.AddGalleryPhoto(transaction, seriesPhoto, kCuratorId, 3).ok);
        // A raised cap lets it in.
        EXPECT_TRUE(helper.AddGalleryPhoto(
            transaction, EventPhoto(parents.eventId, "one more"), kCuratorId, 4).ok);
    });
}

// The public gallery is the rows whose photo has arrived, in order.
TEST(EventGalleryHelperTest, GalleriesShowOnlyUploadedPhotosInOrder) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GalleriesShowOnlyUploadedPhotosInOrder", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryHelper helper(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        auto add = [&](NewGalleryPhoto photo, bool upload) {
            int64_t id = helper.AddGalleryPhoto(transaction, photo, kCuratorId, 12).id;
            if (upload) {
                TableHelpers::PhotoTest::AttachTestPhoto(
                    transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable, id);
            }
            return id;
        };
        add(EventPhoto(parents.eventId, "first"), true);
        add(EventPhoto(parents.eventId, "waiting"), false);
        add(EventPhoto(parents.eventId, "second"), true);
        NewGalleryPhoto seriesPhoto = EventPhoto(0, "series");
        seriesPhoto.seriesId = parents.seriesId;
        int64_t seriesId = add(seriesPhoto, true);

        EXPECT_EQ(AltTexts(helper.GetEventGallery(transaction, parents.eventId)),
                  (std::vector<std::string>{ "first", "second" }));
        std::vector<GalleryPhoto> series = helper.GetSeriesGallery(transaction, parents.seriesId);
        ASSERT_EQ(series.size(), 1u);
        EXPECT_EQ(series[0].id, seriesId);
        EXPECT_TRUE(helper.GetEventGallery(transaction, parents.eventId + 1000).empty());
    });
}

TEST(EventGalleryHelperTest, DeleteRemovesTheRowAndItsPhoto) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DeleteRemovesTheRowAndItsPhoto", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryHelper helper(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        int64_t uploaded = helper.AddGalleryPhoto(
            transaction, EventPhoto(parents.eventId, "uploaded"), kCuratorId, 12).id;
        TableHelpers::PhotoTest::AttachTestPhoto(
            transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable, uploaded);
        int64_t waiting = helper.AddGalleryPhoto(
            transaction, EventPhoto(parents.eventId, "waiting"), kCuratorId, 12).id;

        Images::ImageHelper images(databaseHelper);
        ASSERT_TRUE(images.HasPhoto(transaction, DbSchema::kEventGalleryPhotosTable, uploaded));
        EXPECT_TRUE(helper.DeleteGalleryPhoto(transaction, uploaded));
        EXPECT_FALSE(images.HasPhoto(transaction, DbSchema::kEventGalleryPhotosTable, uploaded));
        // A row still waiting for its photo deletes too.
        EXPECT_TRUE(helper.DeleteGalleryPhoto(transaction, waiting));
        EXPECT_FALSE(helper.DeleteGalleryPhoto(transaction, waiting));
        EXPECT_TRUE(TableHelpers::EventGalleryPhotos(databaseHelper)
                        .GetGalleryForEvent(transaction, parents.eventId).empty());
    });
}

}  // namespace
}  // namespace Events
