#include "register_app_services.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/auth/server_config.h"
#include "business_logic/images/image_helper.h"
#include "db_schema/events.h"
#include "db_schema/guide_pages.h"
#include "db_schema/organizations.h"
#include "db_schema/people.h"
#include "db_schema/places.h"
#include "db_schema/venues.h"
#include "endpoints/endpoint_test_helper.h"
#include "endpoints/web_app.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/guide_pages.h"
#include "sql_util/table_helpers/organizations.h"
#include "sql_util/table_helpers/photo_support_tables.h"
#include "sql_util/table_helpers/places.h"
#include "sql_util/table_helpers/venues.h"
#include "test/src/util/database_test_helper.h"
#include "util/thread_pool.h"

// After the app headers: on Windows, <windows.h> (pulled in above) must come
// before libjpeg, whose jmorecfg.h otherwise typedefs INT32 first and clashes
// with basetsd.h's.
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

// EV1 §5.6 — the photo seam main.cpp wires through RegisterAppServices: event and
// venue images, and the guide's organization, place and page images (GD2 §4.10),
// serve to signed-out visitors, and nothing else does.
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
crow::response GetScaledPhoto(EndpointTestHelper& helper, std::string_view table, int64_t id) {
    crow::request req;
    req.method = crow::HTTPMethod::Get;
    req.url = "/api/get_scaled_photo/" + std::string(table) + "/" + std::to_string(id) + "/32/24";
    crow::response resp;
    helper.GetWebApp().GetApp().handle_full(req, resp);
    ThreadPool::GetInstance().Shutdown();
    return resp;
}

// Attaches a photo to a row, as the framework upload endpoint does after its checks.
void AttachPhoto(Transaction& transaction, TestDatabaseUtil& testDb,
                 std::string_view table, int64_t id) {
    TableHelpers::PhotoSupportTables(testDb.GetDatabaseHelper())
        .AddPhotoSupportTable(transaction, std::string(table));
    Images::ImageHelper(testDb.GetDatabaseHelper())
        .UploadAndAssociatePhoto(transaction, std::string(table), id, MakeTestJpeg(64, 48), "jpeg");
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
        int64_t eventId = TableHelpers::Events(testDb.GetDatabaseHelper()).AddEvent(transaction, {
            { std::string(DbSchema::kEventsTitle), "T4T" },
            { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
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

// The set is exactly those five tables: any other table's photos still need a
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
