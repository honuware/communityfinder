#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sql_util/database_access/database_helper.h"

// CM3 — a photo on a row, for tests at every layer that need one (the photo
// picker, the gallery, the public-read rule). Plain helpers, not a fixture.
namespace TableHelpers::PhotoTest {

// A tiny SVG, which honuware stores as uploaded: no image codec in the test.
inline constexpr std::string_view kTestSvg =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"/>)";

// Gives `table` photo support if it hasn't any (the harness seeds none), then
// attaches kTestSvg to the row through ImageHelper, as the upload endpoint does
// after its checks.
void AttachTestPhoto(Transaction& transaction, DatabaseHelper databaseHelper,
                     std::string_view table, int64_t id);

// A small solid-colour JPEG, for the paths that refuse a vector (a submitter's
// flyer).
std::vector<char> MakeTestJpeg(int width, int height);

// Gives `table` photo support if it hasn't any.
void EnsurePhotoSupport(Transaction& transaction, DatabaseHelper databaseHelper,
                        std::string_view table);

}  // namespace TableHelpers::PhotoTest
