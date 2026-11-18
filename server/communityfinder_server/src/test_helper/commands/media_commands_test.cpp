#include "commands/media_commands.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "business_logic/events/event_gallery_helper.h"
#include "business_logic/events/gallery_settings.h"
#include "business_logic/images/image_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "sql_util/table_helpers/event_gallery_photos.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/photo_test_support.h"
#include "sql_util/table_helpers/series.h"
#include "test/src/util/database_test_helper.h"
#include "util/secrets/secrets_helper.h"
#include "util/types.h"

#include "command_runner.h"

// CM3 §6 — the media commands, run through the registry against the test
// database with a hand-built context.
namespace TestHelper {
namespace {

constexpr int64_t kCuratorId = 7;  // consent's person id is not an FK (OQ8 (e))

int RunCommand(TestHelperContext& ctx, Transaction& transaction, const std::string& command,
        const Args& args = {}) {
    const CommandInfo* info = ctx.registry.Find(command);
    EXPECT_NE(info, nullptr) << command;
    return info != nullptr ? info->execute(ctx, transaction, args) : -1;
}

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it != row.end() ? it->second : std::string();
}

int64_t AddSampleEvent(Transaction& transaction, DatabaseHelper databaseHelper) {
    return TableHelpers::Events(databaseHelper).AddEvent(transaction, {
        { std::string(DbSchema::kEventsTitle), "Sample Night" },
        { std::string(DbSchema::kEventsStartsAtUs), "1800000000000000" },
    });
}

int64_t AddSampleSeries(Transaction& transaction, DatabaseHelper databaseHelper) {
    return TableHelpers::Series(databaseHelper).AddSeries(transaction, {
        { std::string(DbSchema::kSeriesTitle), "Sample Series" },
        { std::string(DbSchema::kSeriesSlug), "sample-series" },
    });
}

// A file in the system's temp directory, removed when the test is done with it.
class TempFile {
public:
    TempFile(std::string_view name, std::string_view contents)
        : path_(std::filesystem::temp_directory_path() / std::string(name)) {
        std::ofstream(path_, std::ios::binary) << contents;
    }
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    std::string Path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

TEST(MediaCommandsTest, EachCommandIsRegisteredUnderMediaWithItsAlias) {
    CommandRegistry registry;
    RegisterMediaCommands(registry);
    for (const auto& [name, alias] : std::map<std::string, std::string>{
             { "set-event-video", "sv" }, { "add-gallery-photo", "ag" } }) {
        const CommandInfo* byName = registry.Find(name);
        ASSERT_NE(byName, nullptr) << name;
        EXPECT_EQ(byName->category, "Media");
        EXPECT_EQ(registry.Find(alias), byName) << alias;
    }
}

TEST(MediaCommandsTest, SetEventVideoStoresTheCanonicalLinkAndClears) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("SetEventVideoStoresTheCanonicalLinkAndClears", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        RegisterMediaCommands(ctx.registry);
        const int64_t eventId = AddSampleEvent(transaction, databaseHelper);
        const int64_t seriesId = AddSampleSeries(transaction, databaseHelper);
        auto eventVideo = [&]() {
            return Column(TableHelpers::Events(databaseHelper).GetEvent(transaction, eventId),
                          DbSchema::kEventsVideoUrl);
        };

        ASSERT_EQ(RunCommand(ctx, transaction, "set-event-video",
                      { { "event_id", StringFromInt(eventId) }, { "url", "youtu.be/dQw4w9WgXcQ?t=42" } }), 0);
        EXPECT_EQ(eventVideo(), "https://www.youtube.com/watch?v=dQw4w9WgXcQ");
        ASSERT_EQ(RunCommand(ctx, transaction, "sv",
                      { { "series_id", StringFromInt(seriesId) }, { "url", "https://vimeo.com/76979871" } }), 0);
        EXPECT_EQ(Column(TableHelpers::Series(databaseHelper).GetSeries(transaction, seriesId),
                         DbSchema::kSeriesVideoUrl),
                  "https://vimeo.com/76979871");

        // Refused, and the stored link stays.
        const std::string event = StringFromInt(eventId);
        for (const Args& bad : std::vector<Args>{
                 { { "event_id", event }, { "url", "https://www.tiktok.com/@someone/video/1" } },
                 { { "event_id", event }, { "url", "https://www.youtube.com/playlist?list=PL1" } },
                 { { "event_id", event } },
                 { { "event_id", event }, { "url", "https://vimeo.com/1" }, { "clear", "true" } },
                 { { "event_id", event }, { "series_id", StringFromInt(seriesId) }, { "url", "https://vimeo.com/1" } },
                 { { "url", "https://vimeo.com/1" } },
                 { { "event_id", "999999" }, { "url", "https://vimeo.com/1" } } }) {
            EXPECT_EQ(RunCommand(ctx, transaction, "set-event-video", bad), 1);
        }
        EXPECT_EQ(eventVideo(), "https://www.youtube.com/watch?v=dQw4w9WgXcQ");

        ASSERT_EQ(RunCommand(ctx, transaction, "set-event-video",
                      { { "event_id", event }, { "clear", "true" } }), 0);
        EXPECT_EQ(eventVideo(), "");
    });
}

TEST(MediaCommandsTest, AddGalleryPhotoUploadsTheFileWithTheConsentRecord) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddGalleryPhotoUploadsTheFileWithTheConsentRecord", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        ctx.secretsHelper = Secrets::MakeSecretsHelper(databaseHelper);
        RegisterMediaCommands(ctx.registry);
        TableHelpers::PhotoTest::EnsurePhotoSupport(transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable);
        const int64_t seriesId = AddSampleSeries(transaction, databaseHelper);
        TempFile svg("cm3_media_commands_test.svg", TableHelpers::PhotoTest::kTestSvg);
        const Args args{ { "series_id", StringFromInt(seriesId) }, { "file", svg.Path() },
                         { "alt", "The dance floor" }, { "credit", "Sample Photographer" } };

        // Consent needs someone to record.
        EXPECT_EQ(RunCommand(ctx, transaction, "add-gallery-photo", args), 1);
        ctx.currentPersonId = kCuratorId;
        ASSERT_EQ(RunCommand(ctx, transaction, "ag", args), 0);

        std::vector<Events::GalleryPhoto> shown =
            Events::EventGalleryHelper(databaseHelper).GetSeriesGallery(transaction, seriesId);
        ASSERT_EQ(shown.size(), 1u);
        EXPECT_EQ(shown[0].altText, "The dance floor");
        EXPECT_EQ(shown[0].credit, "Sample Photographer");
        KeyValueTable row = TableHelpers::EventGalleryPhotos(databaseHelper).GetGalleryPhoto(transaction, shown[0].id);
        EXPECT_EQ(Column(row, DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId), StringFromInt(kCuratorId));
        EXPECT_TRUE(Images::ImageHelper(databaseHelper).HasPhoto(transaction, DbSchema::kEventGalleryPhotosTable, shown[0].id));
    });
}

TEST(MediaCommandsTest, AddGalleryPhotoRefusalsLeaveNoRow) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AddGalleryPhotoRefusalsLeaveNoRow", [&](Transaction& transaction) {
        DatabaseHelper databaseHelper = testDb.GetDatabaseHelper();
        TestHelperContext ctx(databaseHelper);
        ctx.secretsHelper = Secrets::MakeSecretsHelper(databaseHelper);
        ctx.currentPersonId = kCuratorId;
        RegisterMediaCommands(ctx.registry);
        TableHelpers::PhotoTest::EnsurePhotoSupport(transaction, databaseHelper, DbSchema::kEventGalleryPhotosTable);
        const std::string eventId = StringFromInt(AddSampleEvent(transaction, databaseHelper));
        TempFile svg("cm3_media_commands_refusals.svg", TableHelpers::PhotoTest::kTestSvg);
        TempFile junk("cm3_media_commands_refusals.png", "not an image at all");
        TempFile text("cm3_media_commands_refusals.txt", "not an image at all");

        for (const Args& bad : std::vector<Args>{
                 { { "event_id", eventId }, { "file", svg.Path() } },  // no alt text
                 { { "event_id", eventId }, { "file", text.Path() }, { "alt", "A flyer" } },
                 { { "event_id", eventId }, { "file", svg.Path() + ".missing.svg" }, { "alt", "A flyer" } },
                 { { "event_id", eventId }, { "file", junk.Path() }, { "alt", "A flyer" } },  // upload fails
                 { { "file", svg.Path() }, { "alt", "A flyer" } } }) {
            EXPECT_EQ(RunCommand(ctx, transaction, "add-gallery-photo", bad), 1);
        }
        EXPECT_TRUE(TableHelpers::EventGalleryPhotos(databaseHelper)
                        .GetGalleryForEvent(transaction, std::stoll(eventId)).empty());

        // The admin cap applies here too.
        Events::GallerySettingsUpdate update;
        update.maxPhotos = 1;
        ASSERT_TRUE(Events::SaveGallerySettings(*ctx.secretsHelper, transaction, update).ok);
        const Args good{ { "event_id", eventId }, { "file", svg.Path() }, { "alt", "A flyer" } };
        ASSERT_EQ(RunCommand(ctx, transaction, "add-gallery-photo", good), 0);
        EXPECT_EQ(RunCommand(ctx, transaction, "add-gallery-photo", good), 1);
        EXPECT_EQ(TableHelpers::EventGalleryPhotos(databaseHelper)
                      .GetGalleryForEvent(transaction, std::stoll(eventId)).size(), 1u);
    });
}

}  // namespace
}  // namespace TestHelper
