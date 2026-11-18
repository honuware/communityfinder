#include "photo_test_support.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/images/image_helper.h"
#include "sql_util/table_helpers/photo_support_tables.h"

// After the app headers: on Windows, <windows.h> (pulled in above) must come
// before libjpeg, whose jmorecfg.h otherwise typedefs INT32 first and clashes
// with basetsd.h's.
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

namespace TableHelpers::PhotoTest {

void EnsurePhotoSupport(Transaction& transaction, DatabaseHelper databaseHelper,
                        std::string_view table) {
    PhotoSupportTables photoSupportTables(databaseHelper);
    if (!photoSupportTables.IsPhotoSupportedForTable(transaction, table)) {
        photoSupportTables.AddPhotoSupportTable(transaction, table);
    }
}

void AttachTestPhoto(Transaction& transaction, DatabaseHelper databaseHelper,
                     std::string_view table, int64_t id) {
    EnsurePhotoSupport(transaction, databaseHelper, table);
    Images::UploadResult result = Images::ImageHelper(databaseHelper).UploadAndAssociatePhoto(
        transaction, table, id, std::vector<char>(kTestSvg.begin(), kTestSvg.end()), "svg");
    EXPECT_TRUE(result.success) << table << " " << id << ": " << result.errorMessage;
}

std::vector<char> MakeTestJpeg(int width, int height) {
    using VectorSink = boost::iostreams::back_insert_device<std::vector<char>>;
    boost::gil::rgb8_image_t image(width, height);
    boost::gil::fill_pixels(boost::gil::view(image), boost::gil::rgb8_pixel_t(128, 0, 128));
    std::vector<char> bytes;
    VectorSink sink(bytes);
    boost::iostreams::stream<VectorSink> stream(sink);
    boost::gil::write_view(stream, boost::gil::const_view(image), boost::gil::jpeg_tag());
    stream.flush();
    return bytes;
}

}  // namespace TableHelpers::PhotoTest
