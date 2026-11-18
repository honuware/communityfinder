#include "event_gallery_photos.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "events.h"
#include "photo_test_support.h"
#include "series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

namespace TableHelpers {
namespace {

struct Parents {
    int64_t eventId = 0;
    int64_t seriesId = 0;
};

Parents AddParents(Transaction& transaction, DatabaseHelper databaseHelper) {
    Parents parents;
    parents.seriesId = Series(databaseHelper).AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), "Sample Night" },
        { std::string(DbSchema::kSeriesSlug), "sample-night" },
    });
    parents.eventId = Events(databaseHelper).AddEvent(transaction, {
        { std::string(DbSchema::kEventsTitle), "Sample Night" },
        { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
    });
    return parents;
}

// A complete row: alt text and the consent record, on one parent.
KeyValueTable MakePhoto(std::string_view altText, std::string_view parentColumn, int64_t parentId) {
    return {
        { std::string(DbSchema::kEventGalleryPhotosAltText), std::string(altText) },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId), "7" },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedUs), "1700000000000000" },
        { std::string(parentColumn), StringFromInt(parentId) },
    };
}

std::vector<std::string> AltTexts(const KeyValueTableArray& rows) {
    std::vector<std::string> altTexts;
    for (const KeyValueTable& row : rows) {
        altTexts.push_back(row.at(std::string(DbSchema::kEventGalleryPhotosAltText)));
    }
    return altTexts;
}

TEST(EventGalleryPhotosTest, EachParentGetsOnlyItsOwnPhotos) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EachParentGetsOnlyItsOwnPhotos", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);

        EXPECT_GT(gallery.AddGalleryPhoto(transaction, MakePhoto(
            "event photo", DbSchema::kEventGalleryPhotosEventId, parents.eventId)), 0);
        gallery.AddGalleryPhoto(transaction, MakePhoto(
            "series photo", DbSchema::kEventGalleryPhotosSeriesId, parents.seriesId));

        EXPECT_EQ(AltTexts(gallery.GetGalleryForEvent(transaction, parents.eventId)),
                  (std::vector<std::string>{ "event photo" }));
        EXPECT_EQ(AltTexts(gallery.GetGalleryForSeries(transaction, parents.seriesId)),
                  (std::vector<std::string>{ "series photo" }));
        EXPECT_TRUE(gallery.GetGalleryForEvent(transaction, parents.eventId + 1000).empty());
    });
}

TEST(EventGalleryPhotosTest, GalleryListsBySortOrderThenId) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GalleryListsBySortOrderThenId", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        auto addWithOrder = [&](std::string_view altText, std::string_view sortOrder) {
            KeyValueTable values =
                MakePhoto(altText, DbSchema::kEventGalleryPhotosSeriesId, parents.seriesId);
            if (!sortOrder.empty()) {
                values[std::string(DbSchema::kEventGalleryPhotosSortOrder)] = std::string(sortOrder);
            }
            gallery.AddGalleryPhoto(transaction, values);
        };
        addWithOrder("third", "2");
        addWithOrder("first", "");  // sort_order defaults to 0
        addWithOrder("second", "1");
        addWithOrder("fourth", "2");

        EXPECT_EQ(AltTexts(gallery.GetGalleryForSeries(transaction, parents.seriesId)),
                  (std::vector<std::string>{ "first", "second", "third", "fourth" }));
    });
}

// The row comes first and its picture second, so a gallery read says which
// rows have one.
TEST(EventGalleryPhotosTest, GalleryReadsSayWhetherEachRowHasItsPhoto) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GalleryReadsSayWhetherEachRowHasItsPhoto", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        int64_t uploaded = gallery.AddGalleryPhoto(transaction, MakePhoto(
            "uploaded", DbSchema::kEventGalleryPhotosEventId, parents.eventId));
        PhotoTest::AttachTestPhoto(
            transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable, uploaded);
        gallery.AddGalleryPhoto(transaction, MakePhoto(
            "waiting", DbSchema::kEventGalleryPhotosEventId, parents.eventId));
        int64_t seriesPhoto = gallery.AddGalleryPhoto(transaction, MakePhoto(
            "series", DbSchema::kEventGalleryPhotosSeriesId, parents.seriesId));
        PhotoTest::AttachTestPhoto(
            transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable, seriesPhoto);

        KeyValueTableArray rows = gallery.GetGalleryForEvent(transaction, parents.eventId);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_TRUE(StringToBool(rows[0].at(std::string(kGalleryPhotoHasPhoto))));
        EXPECT_FALSE(StringToBool(rows[1].at(std::string(kGalleryPhotoHasPhoto))));
        KeyValueTableArray seriesRows = gallery.GetGalleryForSeries(transaction, parents.seriesId);
        ASSERT_EQ(seriesRows.size(), 1u);
        EXPECT_TRUE(StringToBool(seriesRows[0].at(std::string(kGalleryPhotoHasPhoto))));
    });
}

TEST(EventGalleryPhotosTest, GetGalleryPhotoReadsOneRowOrNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GetGalleryPhotoReadsOneRowOrNothing", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakePhoto("a flyer", DbSchema::kEventGalleryPhotosEventId, parents.eventId);
        values[std::string(DbSchema::kEventGalleryPhotosCaption)] = "Doors at nine";
        int64_t id = gallery.AddGalleryPhoto(transaction, values);

        KeyValueTable row = gallery.GetGalleryPhoto(transaction, id);
        ASSERT_FALSE(row.empty());
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosEventId)), StringFromInt(parents.eventId));
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosSeriesId)), "");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosCaption)), "Doors at nine");
        // Credit defaults to empty; the consent record is as given.
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosCredit)), "");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId)), "7");
        EXPECT_TRUE(gallery.GetGalleryPhoto(transaction, id + 1000).empty());
    });
}

// Inserted with updated_us = 0, so a same-microsecond update still shows (CLAUDE.md).
TEST(EventGalleryPhotosTest, UpdateChangesTheRowAndBumpsUpdatedUs) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("UpdateChangesTheRowAndBumpsUpdatedUs", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakePhoto("before", DbSchema::kEventGalleryPhotosEventId, parents.eventId);
        values[std::string(DbSchema::kEventGalleryPhotosUpdatedUs)] = "0";
        int64_t id = gallery.AddGalleryPhoto(transaction, values);

        gallery.UpdateGalleryPhoto(transaction, id, {
            { std::string(DbSchema::kEventGalleryPhotosAltText), "after" },
            { std::string(DbSchema::kEventGalleryPhotosSortOrder), "5" },
        });

        KeyValueTable row = gallery.GetGalleryPhoto(transaction, id);
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosAltText)), "after");
        EXPECT_EQ(row.at(std::string(DbSchema::kEventGalleryPhotosSortOrder)), "5");
        EXPECT_GT(std::stoll(row.at(std::string(DbSchema::kEventGalleryPhotosUpdatedUs))), 0);
    });
}

TEST(EventGalleryPhotosTest, DeleteRemovesOnlyThatRow) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DeleteRemovesOnlyThatRow", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        int64_t doomed = gallery.AddGalleryPhoto(transaction, MakePhoto(
            "doomed", DbSchema::kEventGalleryPhotosEventId, parents.eventId));
        gallery.AddGalleryPhoto(transaction, MakePhoto(
            "kept", DbSchema::kEventGalleryPhotosEventId, parents.eventId));

        gallery.DeleteGalleryPhoto(transaction, doomed);
        // Deleting a row that isn't there is a no-op.
        gallery.DeleteGalleryPhoto(transaction, doomed);

        EXPECT_EQ(AltTexts(gallery.GetGalleryForEvent(transaction, parents.eventId)),
                  (std::vector<std::string>{ "kept" }));
    });
}

// Both parents are real FKs, so deleting one deletes its gallery rows (their
// photos are ImageHelper's to clean up; see the CM3 doc's known limits).
TEST(EventGalleryPhotosTest, DeletingTheParentDeletesItsRows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("DeletingTheParentDeletesItsRows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        int64_t id = gallery.AddGalleryPhoto(transaction, MakePhoto(
            "goes with it", DbSchema::kEventGalleryPhotosEventId, parents.eventId));

        transaction.RunSqlStatement("DELETE FROM events WHERE id = $1", StringFromInt(parents.eventId));

        EXPECT_TRUE(gallery.GetGalleryPhoto(transaction, id).empty());
    });
}

// event_gallery_photos_exactly_one_parent: a photo with no parent shows nowhere.
TEST(EventGalleryPhotosTest, NoParentThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("NoParentThrows", [&](Transaction& transaction) {
        EventGalleryPhotos gallery(testDb.GetDatabaseHelper());
        KeyValueTable values = MakePhoto("orphan", DbSchema::kEventGalleryPhotosEventId, 1);
        values.erase(std::string(DbSchema::kEventGalleryPhotosEventId));
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(gallery.AddGalleryPhoto(transaction, values), std::exception);
    });
}

// event_gallery_photos_exactly_one_parent: one gallery, never two.
TEST(EventGalleryPhotosTest, BothParentsThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BothParentsThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakePhoto("both", DbSchema::kEventGalleryPhotosEventId, parents.eventId);
        values[std::string(DbSchema::kEventGalleryPhotosSeriesId)] = StringFromInt(parents.seriesId);
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(gallery.AddGalleryPhoto(transaction, values), std::exception);
    });
}

TEST(EventGalleryPhotosTest, BlankAltTextThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("BlankAltTextThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(gallery.AddGalleryPhoto(transaction, MakePhoto(
            "   ", DbSchema::kEventGalleryPhotosEventId, parents.eventId)), std::exception);
    });
}

TEST(EventGalleryPhotosTest, OverlongCreditThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OverlongCreditThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakePhoto("credited", DbSchema::kEventGalleryPhotosEventId, parents.eventId);
        values[std::string(DbSchema::kEventGalleryPhotosCredit)] =
            std::string(DbSchema::kEventGalleryPhotosCreditMaxLength + 1, 'x');
        // The CHECK violation aborts the transaction — last statement.
        EXPECT_THROW(gallery.AddGalleryPhoto(transaction, values), std::exception);
    });
}

// The consent record has no default: a row without it can't be added (CM3 OQ6).
TEST(EventGalleryPhotosTest, MissingConsentRecordThrows) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("MissingConsentRecordThrows", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EventGalleryPhotos gallery(databaseHelper);
        Parents parents = AddParents(transaction, databaseHelper);
        KeyValueTable values =
            MakePhoto("unvouched", DbSchema::kEventGalleryPhotosEventId, parents.eventId);
        values.erase(std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId));
        // The NOT NULL violation aborts the transaction — last statement.
        EXPECT_THROW(gallery.AddGalleryPhoto(transaction, values), std::exception);
    });
}

}  // namespace
}  // namespace TableHelpers
