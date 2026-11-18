#include "business_logic/events/event_gallery_helper.h"

#include <algorithm>
#include <cctype>

#include "business_logic/images/image_helper.h"
#include "db_schema/event_gallery_photos.h"
#include "util/types.h"

namespace Events {

namespace {

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

std::string Trimmed(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

// Characters, as the database's char_length counts them: UTF-8 continuation
// bytes don't start one.
size_t CharacterCount(std::string_view text) {
    return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](char c) {
        return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    }));
}

std::vector<GalleryPhoto> PhotosWithPictures(const KeyValueTableArray& rows) {
    std::vector<GalleryPhoto> photos;
    for (const KeyValueTable& row : rows) {
        if (!StringToBool(Column(row, TableHelpers::kGalleryPhotoHasPhoto))) {
            continue;
        }
        GalleryPhoto photo;
        photo.id = std::stoll(Column(row, DbSchema::kEventGalleryPhotosId));
        photo.altText = Column(row, DbSchema::kEventGalleryPhotosAltText);
        photo.caption = Column(row, DbSchema::kEventGalleryPhotosCaption);
        photo.credit = Column(row, DbSchema::kEventGalleryPhotosCredit);
        photos.push_back(std::move(photo));
    }
    return photos;
}

}  // namespace

EventGalleryHelper::EventGalleryHelper(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper),
      gallery_(databaseHelper),
      events_(databaseHelper),
      series_(databaseHelper) {
}

std::vector<GalleryPhoto> EventGalleryHelper::GetEventGallery(
    Transaction& transaction, int64_t eventId) const {
    return PhotosWithPictures(gallery_.GetGalleryForEvent(transaction, eventId));
}

std::vector<GalleryPhoto> EventGalleryHelper::GetSeriesGallery(
    Transaction& transaction, int64_t seriesId) const {
    return PhotosWithPictures(gallery_.GetGalleryForSeries(transaction, seriesId));
}

AddGalleryPhotoResult EventGalleryHelper::AddGalleryPhoto(
    Transaction& transaction, const NewGalleryPhoto& photo, int64_t personId,
    int64_t maxPhotos) {
    AddGalleryPhotoResult result;
    auto fail = [&](std::string_view errorCode, std::string_view field) {
        result.errorCode = std::string(errorCode);
        result.field = std::string(field);
        return result;
    };

    const bool forEvent = photo.eventId > 0;
    if (forEvent == (photo.seriesId > 0)) {
        return fail(kGalleryErrorOneParent, kGalleryFieldEventId);
    }
    if (forEvent ? events_.GetEvent(transaction, photo.eventId).empty()
                 : series_.GetSeries(transaction, photo.seriesId).empty()) {
        return fail(kGalleryErrorParentNotFound,
                    forEvent ? kGalleryFieldEventId : kGalleryFieldSeriesId);
    }

    const std::string altText = Trimmed(photo.altText);
    const std::string caption = Trimmed(photo.caption);
    const std::string credit = Trimmed(photo.credit);
    if (altText.empty()) {
        return fail(kGalleryErrorAltTextRequired, kGalleryFieldAltText);
    }
    if (CharacterCount(altText) > DbSchema::kEventGalleryPhotosAltTextMaxLength) {
        return fail(kGalleryErrorTooLong, kGalleryFieldAltText);
    }
    if (CharacterCount(caption) > DbSchema::kEventGalleryPhotosCaptionMaxLength) {
        return fail(kGalleryErrorTooLong, kGalleryFieldCaption);
    }
    if (CharacterCount(credit) > DbSchema::kEventGalleryPhotosCreditMaxLength) {
        return fail(kGalleryErrorTooLong, kGalleryFieldCredit);
    }
    if (!photo.consentConfirmed) {
        return fail(kGalleryErrorConsentRequired, kGalleryFieldConsentConfirmed);
    }

    const KeyValueTableArray existing = forEvent
        ? gallery_.GetGalleryForEvent(transaction, photo.eventId)
        : gallery_.GetGalleryForSeries(transaction, photo.seriesId);
    if (static_cast<int64_t>(existing.size()) >= maxPhotos) {
        result.maxPhotos = maxPhotos;
        return fail(kGalleryErrorFull, forEvent ? kGalleryFieldEventId : kGalleryFieldSeriesId);
    }
    // At the end: one past the highest sort_order there.
    int64_t sortOrder = 0;
    for (const KeyValueTable& row : existing) {
        sortOrder = std::max<int64_t>(sortOrder, std::stoll(Column(row, DbSchema::kEventGalleryPhotosSortOrder)) + 1);
    }

    result.id = gallery_.AddGalleryPhoto(transaction, {
        { std::string(forEvent ? DbSchema::kEventGalleryPhotosEventId
                               : DbSchema::kEventGalleryPhotosSeriesId),
          StringFromInt(forEvent ? photo.eventId : photo.seriesId) },
        { std::string(DbSchema::kEventGalleryPhotosSortOrder), StringFromInt(sortOrder) },
        { std::string(DbSchema::kEventGalleryPhotosAltText), altText },
        { std::string(DbSchema::kEventGalleryPhotosCaption), caption },
        { std::string(DbSchema::kEventGalleryPhotosCredit), credit },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedByPersonId), StringFromInt(personId) },
        { std::string(DbSchema::kEventGalleryPhotosConsentConfirmedUs),
          std::string(DbSchema::kDatabaseInfoDefaultNow) },
    });
    result.ok = true;
    return result;
}

bool EventGalleryHelper::DeleteGalleryPhoto(Transaction& transaction, int64_t id) {
    if (gallery_.GetGalleryPhoto(transaction, id).empty()) {
        return false;
    }
    // A row without its photo yet has nothing to delete there.
    Images::ImageHelper(databaseHelper_).DeletePhotoForItem(
        transaction, DbSchema::kEventGalleryPhotosTable, id);
    gallery_.DeleteGalleryPhoto(transaction, id);
    return true;
}

}  // namespace Events
