#include "commands/media_commands.h"

#include <cctype>
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/event_gallery_helper.h"
#include "business_logic/events/gallery_settings.h"
#include "business_logic/events/video_link.h"
#include "business_logic/images/image_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "db_schema/events.h"
#include "db_schema/series.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/series.h"
#include "util/types.h"

#include "command_runner.h"
#include "console_colors.h"

namespace TestHelper {

namespace {

std::string Arg(const Args& args, const char* name) {
    auto it = args.find(name);
    return it != args.end() ? it->second : std::string();
}

bool HasFlag(const Args& args, const char* name) {
    return Arg(args, name) == "true";
}

// A whole number, or nullopt when the flag is missing or isn't one.
std::optional<int64_t> IntArg(const Args& args, const char* name) {
    std::string text = Arg(args, name);
    if (text.empty()) return std::nullopt;
    try {
        size_t used = 0;
        int64_t value = std::stoll(text, &used);
        if (used == text.size()) return value;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

// What a media command acts on: an event or a series (CM3 OQ7).
struct Parent {
    bool isEvent = false;
    int64_t id = 0;
};

std::string Describe(const Parent& parent) {
    return (parent.isEvent ? "event " : "series ") + StringFromInt(parent.id);
}

// Exactly one of --event_id and --series_id, naming a row that exists.
std::optional<Parent> ParentArg(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    const bool isEvent = !Arg(args, "event_id").empty();
    if (isEvent == !Arg(args, "series_id").empty()) {
        Color::PrintError("Give one of --event_id and --series_id");
        return std::nullopt;
    }
    const char* flag = isEvent ? "event_id" : "series_id";
    std::optional<int64_t> id = IntArg(args, flag);
    const bool found = id
        && !(isEvent ? TableHelpers::Events(ctx.databaseHelper).GetEvent(transaction, *id)
                     : TableHelpers::Series(ctx.databaseHelper).GetSeries(transaction, *id)).empty();
    if (!found) {
        Color::PrintError(std::string(isEvent ? "No event" : "No series") + " with id " + Arg(args, flag));
        return std::nullopt;
    }
    return Parent{ isEvent, *id };
}

// The stored image type for a file, by its extension; "" when it isn't one
// the gallery takes.
std::string ImageTypeForFile(std::string_view path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string_view::npos) return {};
    std::string extension;
    for (char c : path.substr(dot + 1)) {
        extension += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (extension == "jpg" || extension == "jpeg") return "jpeg";
    if (extension == "png" || extension == "svg") return extension;
    return {};
}

int RunSetEventVideo(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    std::optional<Parent> parent = ParentArg(ctx, transaction, args);
    if (!parent) return 1;
    const bool clear = HasFlag(args, "clear");
    const std::string url = Arg(args, "url");
    if (clear == !url.empty()) {
        Color::PrintError("Give --url, or --clear to remove the video");
        return 1;
    }

    std::string stored;
    if (!clear) {
        Events::VideoLinkResult video = Events::ParseVideoLink(url);
        if (!video.ok) {
            Color::PrintError(video.errorCode + ": not a link to one YouTube or Vimeo video");
            return 1;
        }
        stored = video.link.canonicalUrl;
    }
    if (parent->isEvent) {
        TableHelpers::Events(ctx.databaseHelper).UpdateEvent(transaction, parent->id, {
            { std::string(DbSchema::kEventsVideoUrl), stored } });
    } else {
        TableHelpers::Series(ctx.databaseHelper).UpdateSeries(transaction, parent->id, {
            { std::string(DbSchema::kSeriesVideoUrl), stored } });
    }
    Color::PrintSuccess(clear ? "Removed the video from " + Describe(*parent)
                              : "Set the video on " + Describe(*parent) + " to " + stored);
    return 0;
}

int RunAddGalleryPhoto(TestHelperContext& ctx, Transaction& transaction, const Args& args) {
    if (ctx.currentPersonId <= 0) {
        Color::PrintError("Log in first: the photo records who confirmed consent");
        return 1;
    }
    std::optional<Parent> parent = ParentArg(ctx, transaction, args);
    if (!parent) return 1;
    const std::string file = Arg(args, "file");
    const std::string imageType = ImageTypeForFile(file);
    if (imageType.empty()) {
        Color::PrintError("--file must be a .jpg, .jpeg, .png or .svg file");
        return 1;
    }
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        Color::PrintError("Can't read " + file);
        return 1;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    Events::NewGalleryPhoto photo;
    if (parent->isEvent) {
        photo.eventId = parent->id;
    } else {
        photo.seriesId = parent->id;
    }
    photo.altText = Arg(args, "alt");
    photo.caption = Arg(args, "caption");
    photo.credit = Arg(args, "credit");
    // Local data: whoever is logged in stands as having confirmed consent.
    photo.consentConfirmed = true;
    Events::EventGalleryHelper gallery(ctx.databaseHelper);
    Events::AddGalleryPhotoResult added = gallery.AddGalleryPhoto(
        transaction, photo, ctx.currentPersonId,
        Events::LoadGallerySettings(*ctx.secretsHelper, transaction).maxPhotos);
    if (!added.ok) {
        Color::PrintError(added.errorCode + " (" + added.field + ")"
                          + (added.errorCode == Events::kGalleryErrorFull
                                 ? ": the cap is " + StringFromInt(added.maxPhotos) : ""));
        return 1;
    }

    // The row comes first and its picture second, as in the admin editor. A
    // row whose picture didn't take would never show, so it goes back out.
    std::string uploadError;
    try {
        Images::UploadResult upload = Images::ImageHelper(ctx.databaseHelper, ctx.secretsHelper)
            .UploadAndAssociatePhoto(transaction, DbSchema::kEventGalleryPhotosTable, added.id,
                                     bytes, imageType);
        if (!upload.success) uploadError = upload.errorMessage;
    } catch (const std::exception& e) {
        uploadError = e.what();
    }
    if (!uploadError.empty()) {
        gallery.DeleteGalleryPhoto(transaction, added.id);
        Color::PrintError("Upload failed: " + uploadError);
        return 1;
    }
    Color::PrintSuccess("Added gallery photo " + StringFromInt(added.id) + " to " + Describe(*parent)
                        + ", consent confirmed by person " + StringFromInt(ctx.currentPersonId));
    return 0;
}

}  // namespace

void RegisterMediaCommands(CommandRegistry& registry) {
    registry.Register({
        "set-event-video", "sv", "Media",
        "Set or remove the YouTube or Vimeo video on an event or a series",
        {{ "--event_id", "Event id (or --series_id)", false },
         { "--series_id", "Series id (or --event_id)", false },
         { "--url", "The video's link, e.g. https://youtu.be/…", false },
         { "--clear", "Remove the video instead", false }},
        RunSetEventVideo
    });

    registry.Register({
        "add-gallery-photo", "ag", "Media",
        "Add a picture from a file to an event's or a series' gallery (needs a login)",
        {{ "--event_id", "Event id (or --series_id)", false },
         { "--series_id", "Series id (or --event_id)", false },
         { "--file", "A .jpg, .png or .svg file", true },
         { "--alt", "What the picture shows, for screen readers", true },
         { "--caption", "Caption", false },
         { "--credit", "Who took it", false }},
        RunAddGalleryPhoto
    });
}

}  // namespace TestHelper
