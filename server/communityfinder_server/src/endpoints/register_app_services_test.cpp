#include "register_app_services.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/images/image_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/guide_pages.h"
#include "db_schema/organizations.h"
#include "db_schema/people.h"
#include "db_schema/places.h"
#include "db_schema/series.h"
#include "db_schema/venues.h"
#include "endpoints/endpoint_test_helper.h"
#include "endpoints/web_app.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/photo_support_tables.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/series.h"
#include "sql_util/table_helpers/venues.h"
#include "test/src/util/database_test_helper.h"
#include "util/image_resize.h"
#include "util/thread_pool.h"

// After the app headers: on Windows, <windows.h> (pulled in above) must come
// before libjpeg, whose jmorecfg.h otherwise typedefs INT32 first and clashes
// with basetsd.h's.
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

// EV1 §5.6 — the photo seam main.cpp wires through RegisterAppServices: event and
// venue images, the guide's organization, place and page images (GD2 §4.10), and
// series and gallery images (CM3) serve to signed-out visitors, and nothing else
// does. A pending event's images wait for its approval (CM3 H15).
namespace Endpoints {
namespace {

using VectorSink = boost::iostreams::back_insert_device<std::vector<char>>;
using VectorStream = boost::iostreams::stream<VectorSink>;

// A small solid-colour JPEG (knottyyoga's get_scaled_photo_test idiom).
std::vector<char> MakeTestJpeg(int width, int height) {
    boost::gil::rgb8_image_t image(width, height);
    auto view = boost::gil::view(image);
    boost::gil::rgb8_pixel_t purple(128, 0, 128);
    for (int y = 0; y < view.height(); ++y) {
        auto row = view.row_begin(y);
        for (int x = 0; x < view.width(); ++x) {
            row[x] = purple;
        }
    }
    std::vector<char> bytes;
    VectorSink sink(bytes);
    VectorStream stream(sink);
    boost::gil::write_view(stream, boost::gil::const_view(image), boost::gil::jpeg_tag());
    stream.flush();
    return bytes;
}

// An anonymous scaled-photo read. Nothing is logged in, so no cookie is set.
crow::response GetScaledPhoto(EndpointTestHelper& helper, std::string_view table, int64_t id,
                              int width = 32, int height = 24) {
    crow::request req;
    req.method = crow::HTTPMethod::Get;
    req.url = "/api/get_scaled_photo/" + std::string(table) + "/" + std::to_string(id) + "/" +
              std::to_string(width) + "/" + std::to_string(height);
    crow::response resp;
    helper.GetWebApp().GetApp().handle_full(req, resp);
    ThreadPool::GetInstance().Shutdown();
    return resp;
}

// Attaches a photo to a row, as the framework upload endpoint does after its checks.
void AttachPhoto(Transaction& transaction, TestDatabaseUtil& testDb,
                 std::string_view table, int64_t id, int width = 64, int height = 48) {
    TableHelpers::PhotoSupportTables photoSupport(testDb.GetDatabaseHelper());
    if (!photoSupport.IsPhotoSupportedForTable(transaction, table)) {
        photoSupport.AddPhotoSupportTable(transaction, std::string(table));
    }
    Images::UploadResult result = Images::ImageHelper(testDb.GetDatabaseHelper())
        .UploadAndAssociatePhoto(transaction, std::string(table), id, MakeTestJpeg(width, height), "jpeg");
    EXPECT_TRUE(result.success) << table << " " << id << ": " << result.errorMessage;
}

// A gallery row on one parent, with its consent record.
int64_t AddGalleryRow(Transaction& transaction, TestDatabaseUtil& testDb,
                      std::string_view parentColumn, int64_t parentId) {
    return TableHelpers::EventGalleryPhotos(testDb.GetDatabaseHelper()).AddGalleryPhoto(transaction, {
        { std::string(parentColumn), std::to_string(parentId) },
        { std::string(DbSchema::kEventGalleryPhotosAltText), "The dance floor" },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId), "1" },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedUs), "1700000000000000" },
    });
}

TEST(RegisterAppServicesTest, VenueAndEventPhotosServeToSignedOutVisitors) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("VenueAndEventPhotosServeToSignedOutVisitors", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        RegisterAppServices(helper.GetWebApp());

        int64_t venueId = TableHelpers::Venues(testDb.GetDatabaseHelper()).AddVenue(transaction, {
            { std::string(DbSchema::kVenuesName), "The Cuff" },
        });
        // Approved: a pending event's photo is private (CM3 H15, below).
        int64_t eventId = TableHelpers::Events(testDb.GetDatabaseHelper()).AddEvent(transaction, {
            { std::string(DbSchema::kEventsTitle), "T4T" },
            { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusApproved) },
        });
        AttachPhoto(transaction, testDb, DbSchema::kVenuesTable, venueId);
        AttachPhoto(transaction, testDb, DbSchema::kEventsTable, eventId);

        for (const auto& [table, id] : { std::pair<std::string_view, int64_t>{ DbSchema::kVenuesTable, venueId },
                                         std::pair<std::string_view, int64_t>{ DbSchema::kEventsTable, eventId } }) {
            crow::response resp = GetScaledPhoto(helper, table, id);
            EXPECT_EQ(resp.code, 200) << table << ": " << resp.body;
            EXPECT_EQ(resp.get_header_value("Content-Type"), "image/jpeg") << table;
            EXPECT_FALSE(resp.body.empty()) << table;
            // Public imagery may be stored by the edge cache.
            EXPECT_EQ(resp.get_header_value("Cache-Control"), "public, no-cache") << table;
        }
    });
    Auth::ServerConfig::Shutdown();
}

// GD2 §4.10 — the guide's organization, place and page images serve the same way.
TEST(RegisterAppServicesTest, GuidePhotosServeToSignedOutVisitors) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("GuidePhotosServeToSignedOutVisitors", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        RegisterAppServices(helper.GetWebApp());
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();

        int64_t organizationId = TableHelpers::Organizations(databaseHelper).AddOrganization(transaction, {
            { std::string(DbSchema::kOrganizationsName), "Example League" },
            { std::string(DbSchema::kOrganizationsSlug), "example-league" },
            { std::string(DbSchema::kOrganizationsOrgKind),
              std::string(DbSchema::kOrganizationsOrgKindSportsLeague) },
        });
        int64_t placeId = TableHelpers::Places(databaseHelper).AddPlace(transaction, {
            { std::string(DbSchema::kPlacesName), "Example Beach" },
            { std::string(DbSchema::kPlacesSlug), "example-beach" },
            { std::string(DbSchema::kPlacesPlaceKind), std::string(DbSchema::kPlacesPlaceKindParkBeach) },
        });
        int64_t pageId = TableHelpers::GuidePages(databaseHelper).AddPage(transaction, {
            { std::string(DbSchema::kGuidePagesTitle), "Example Page" },
            { std::string(DbSchema::kGuidePagesSlug), "example-page" },
            { std::string(DbSchema::kGuidePagesSection), std::string(DbSchema::kGuidePagesSectionHealth) },
        });
        AttachPhoto(transaction, testDb, DbSchema::kOrganizationsTable, organizationId);
        AttachPhoto(transaction, testDb, DbSchema::kPlacesTable, placeId);
        AttachPhoto(transaction, testDb, DbSchema::kGuidePagesTable, pageId);

        for (const auto& [table, id] : { std::pair<std::string_view, int64_t>{ DbSchema::kOrganizationsTable, organizationId },
                                         std::pair<std::string_view, int64_t>{ DbSchema::kPlacesTable, placeId },
                                         std::pair<std::string_view, int64_t>{ DbSchema::kGuidePagesTable, pageId } }) {
            crow::response resp = GetScaledPhoto(helper, table, id);
            EXPECT_EQ(resp.code, 200) << table << ": " << resp.body;
            EXPECT_EQ(resp.get_header_value("Content-Type"), "image/jpeg") << table;
            EXPECT_FALSE(resp.body.empty()) << table;
        }
    });
    Auth::ServerConfig::Shutdown();
}

// CM3 — a series' photo and both kinds of gallery row serve the same way.
TEST(RegisterAppServicesTest, SeriesAndGalleryPhotosServeToSignedOutVisitors) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SeriesAndGalleryPhotosServeToSignedOutVisitors", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        RegisterAppServices(helper.GetWebApp());
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();

        int64_t seriesId = TableHelpers::Series(databaseHelper).AddSeries(transaction, {
            { std::string(DbSchema::kSeriesTitle), "Sample Night" },
            { std::string(DbSchema::kSeriesSlug), "sample-night" },
        });
        int64_t eventId = TableHelpers::Events(databaseHelper).AddEvent(transaction, {
            { std::string(DbSchema::kEventsTitle), "Sample Night" },
            { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusApproved) },
        });
        int64_t seriesRow = AddGalleryRow(transaction, testDb, DbSchema::kEventGalleryPhotosSeriesId, seriesId);
        int64_t eventRow = AddGalleryRow(transaction, testDb, DbSchema::kEventGalleryPhotosEventId, eventId);
        AttachPhoto(transaction, testDb, DbSchema::kSeriesTable, seriesId);
        AttachPhoto(transaction, testDb, DbSchema::kEventGalleryPhotosTable, seriesRow);
        AttachPhoto(transaction, testDb, DbSchema::kEventGalleryPhotosTable, eventRow);

        for (const auto& [table, id] : { std::pair<std::string_view, int64_t>{ DbSchema::kSeriesTable, seriesId },
                                         std::pair<std::string_view, int64_t>{ DbSchema::kEventGalleryPhotosTable, seriesRow },
                                         std::pair<std::string_view, int64_t>{ DbSchema::kEventGalleryPhotosTable, eventRow } }) {
            crow::response resp = GetScaledPhoto(helper, table, id);
            EXPECT_EQ(resp.code, 200) << table << " " << id << ": " << resp.body;
            EXPECT_EQ(resp.get_header_value("Cache-Control"), "public, no-cache") << table << " " << id;
        }
    });
    Auth::ServerConfig::Shutdown();
}

// CM3 H15 — until its event is public, an event's photo and its gallery's need
// a login: a stranger's flyer is never public before a curator approves it.
TEST(RegisterAppServicesTest, APendingEventsPhotosNeedALogin) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("APendingEventsPhotosNeedALogin", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        RegisterAppServices(helper.GetWebApp());
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();

        TableHelpers::Events events(databaseHelper);
        int64_t pending = events.AddEvent(transaction, {
            { std::string(DbSchema::kEventsTitle), "Sample Night" },
            { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusPending) },
        });
        int64_t galleryRow = AddGalleryRow(transaction, testDb, DbSchema::kEventGalleryPhotosEventId, pending);
        AttachPhoto(transaction, testDb, DbSchema::kEventsTable, pending);
        AttachPhoto(transaction, testDb, DbSchema::kEventGalleryPhotosTable, galleryRow);

        EXPECT_EQ(GetScaledPhoto(helper, DbSchema::kEventsTable, pending).code, 401);
        EXPECT_EQ(GetScaledPhoto(helper, DbSchema::kEventGalleryPhotosTable, galleryRow).code, 401);
        // A gallery row that isn't there (its parent's delete cascaded) isn't public either.
        EXPECT_EQ(GetScaledPhoto(helper, DbSchema::kEventGalleryPhotosTable, galleryRow + 1000).code, 401);

        // Approved, both serve.
        events.UpdateEvent(transaction, pending, {
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusApproved) },
        });
        EXPECT_EQ(GetScaledPhoto(helper, DbSchema::kEventsTable, pending).code, 200);
        EXPECT_EQ(GetScaledPhoto(helper, DbSchema::kEventGalleryPhotosTable, galleryRow).code, 200);
    });
    Auth::ServerConfig::Shutdown();
}

// CM3 OQ10 — CommunityFinder serves photos up to 1600×1200, not honuware's
// 800×800, so the gallery's full view and the 1200×630 hero come out whole. The
// source is bigger than the cap, so the answer shows both that the cap was raised
// and that it still applies. (2048×1536 scales by exactly 0.78125; a ratio that
// isn't exact in binary can truncate the clipped height to 1199.)
TEST(RegisterAppServicesTest, ASignedOutVisitorGetsPhotosUpTo1600By1200) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("PhotosUpTo1600By1200", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        RegisterAppServices(helper.GetWebApp());

        int64_t seriesId = TableHelpers::Series(testDb.GetDatabaseHelper()).AddSeries(transaction, {
            { std::string(DbSchema::kSeriesTitle), "Sample Night" },
            { std::string(DbSchema::kSeriesSlug), "sample-night" },
        });
        AttachPhoto(transaction, testDb, DbSchema::kSeriesTable, seriesId, 2048, 1536);

        crow::response resp = GetScaledPhoto(helper, DbSchema::kSeriesTable, seriesId, 2048, 1536);
        ASSERT_EQ(resp.code, 200) << resp.body;
        const ImageResize::ImageDimensions served = ImageResize::GetImageDimensions(
            std::vector<char>(resp.body.begin(), resp.body.end()), ImageResize::IMAGE_TYPE_JPEG);
        EXPECT_EQ(served.width, 1600);
        EXPECT_EQ(served.height, 1200);
    });
    Auth::ServerConfig::Shutdown();
}

// The set is exactly those seven tables: any other table's photos still need a
// login, refused before any lookup.
TEST(RegisterAppServicesTest, OtherPhotosStillNeedALogin) {
    Auth::ServerConfig::Shutdown();
    Auth::ServerConfig::InitializeTestMode();
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("OtherPhotosStillNeedALogin", [&](Transaction& transaction) {
        EndpointTestHelper helper(transaction, testDb);
        RegisterAppServices(helper.GetWebApp());
        EXPECT_EQ(GetScaledPhoto(helper, DbSchema::kPeopleTable, 1).code, 401);
    });
    Auth::ServerConfig::Shutdown();
}

}  // namespace
}  // namespace Endpoints
