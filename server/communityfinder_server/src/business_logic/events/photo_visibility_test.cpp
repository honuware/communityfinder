#include "business_logic/events/photo_visibility.h"

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "db_schema/venues.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/series.h"
#include "test/src/util/database_test_helper.h"
#include "util/types.h"

// CM3 H15 — which rows' photos a signed-out visitor may read.
namespace Events {
namespace {

int64_t AddEventWithStatus(Transaction& transaction, DatabaseHelper databaseHelper,
                           std::string_view status) {
    return TableHelpers::Events(databaseHelper).AddEvent(transaction, {
        { std::string(DbSchema::kEventsTitle), "Sample Night" },
        { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
        { std::string(DbSchema::kEventsStatus), std::string(status) },
    });
}

int64_t AddGalleryRow(Transaction& transaction, DatabaseHelper databaseHelper,
                      std::string_view parentColumn, int64_t parentId) {
    return TableHelpers::EventGalleryPhotos(databaseHelper).AddGalleryPhoto(transaction, {
        { std::string(parentColumn), StringFromInt(parentId) },
        { std::string(DbSchema::kEventGalleryPhotosAltText), "The dance floor" },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId), "1" },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedUs), "1700000000000000" },
    });
}

TEST(PhotoVisibilityTest, AnEventsPhotoIsPublicOnlyWithAPublicStatus) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnEventsPhotoIsPublicOnlyWithAPublicStatus", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        for (const auto& [status, isPublic] : { std::pair<std::string_view, bool>{ DbSchema::kEventsStatusApproved, true },
                                                std::pair<std::string_view, bool>{ DbSchema::kEventsStatusArchived, true },
                                                std::pair<std::string_view, bool>{ DbSchema::kEventsStatusPending, false },
                                                std::pair<std::string_view, bool>{ DbSchema::kEventsStatusRejected, false } }) {
            int64_t eventId = AddEventWithStatus(transaction, databaseHelper, status);
            EXPECT_EQ(IsPhotoRowPublic(databaseHelper, transaction, DbSchema::kEventsTable, eventId), isPublic)
                << status;
        }
        EXPECT_FALSE(IsPhotoRowPublic(databaseHelper, transaction, DbSchema::kEventsTable, 999999));
    });
}

TEST(PhotoVisibilityTest, AGalleryRowFollowsItsParent) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AGalleryRowFollowsItsParent", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        int64_t seriesId = TableHelpers::Series(databaseHelper).AddSeries(transaction, {
            { std::string(DbSchema::kSeriesTitle), "Sample Night" },
            { std::string(DbSchema::kSeriesSlug), "sample-night" },
        });
        int64_t approved = AddEventWithStatus(transaction, databaseHelper, DbSchema::kEventsStatusApproved);
        int64_t pending = AddEventWithStatus(transaction, databaseHelper, DbSchema::kEventsStatusPending);

        auto isPublic = [&](int64_t row) {
            return IsPhotoRowPublic(databaseHelper, transaction, DbSchema::kEventGalleryPhotosTable, row);
        };
        EXPECT_TRUE(isPublic(AddGalleryRow(transaction, databaseHelper,
                                           DbSchema::kEventGalleryPhotosSeriesId, seriesId)));
        EXPECT_TRUE(isPublic(AddGalleryRow(transaction, databaseHelper,
                                           DbSchema::kEventGalleryPhotosEventId, approved)));
        int64_t pendingRow =
            AddGalleryRow(transaction, databaseHelper, DbSchema::kEventGalleryPhotosEventId, pending);
        EXPECT_FALSE(isPublic(pendingRow));
        // A row that's gone (its parent's delete cascaded) leaves its photo
        // behind, and that photo isn't public.
        EXPECT_FALSE(isPublic(pendingRow + 1000));
    });
}

// Every other listed table is public row by row, as before H15.
TEST(PhotoVisibilityTest, OtherTablesAreAlwaysPublic) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OtherTablesAreAlwaysPublic", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        EXPECT_TRUE(IsPhotoRowPublic(databaseHelper, transaction, DbSchema::kSeriesTable, 1));
        EXPECT_TRUE(IsPhotoRowPublic(databaseHelper, transaction, DbSchema::kVenuesTable, 1));
    });
}

}  // namespace
}  // namespace Events
