#pragma once

#include <cstddef>

#include "sql_util/database_access/transaction.h"
#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// CM3 — an event's or a series' photo gallery. honuware stores one photo
	// per row, so each picture is a row here and the row has its one photo
	// (upload, scaling, the public read and the delete clean-up all reused).
	//
	// Each row has EXACTLY ONE parent, an event or a series, which a CHECK
	// enforces: a recurring night's pictures belong to the series, since each
	// dated instance drops off the calendar once it's past. Both are real FKs,
	// so deleting the parent deletes its rows.
	//
	// Rows are added through POST /api/admin/event_gallery_photos, which stamps the
	// consent record from the session and enforces the configurable cap; the
	// consent columns have no defaults, so the generic editor can't add one
	// without them. Editing, reordering and deleting go through generic CRUD
	// under manage_events.
	inline constexpr std::string_view kEventGalleryPhotosTable = "event_gallery_photos";
	inline constexpr std::string_view kEventGalleryPhotosId = "id";
	inline constexpr std::string_view kEventGalleryPhotosEventId = "event_id";
	inline constexpr std::string_view kEventGalleryPhotosSeriesId = "series_id";
	// Order within one parent's gallery; lower shows first, then id.
	inline constexpr std::string_view kEventGalleryPhotosSortOrder = "sort_order";
	// Required: what the picture shows, for screen readers. Never names an
	// attendee (the photo policy).
	inline constexpr std::string_view kEventGalleryPhotosAltText = "alt_text";
	inline constexpr std::string_view kEventGalleryPhotosCaption = "caption";
	// Who took it, when they want the credit.
	inline constexpr std::string_view kEventGalleryPhotosCredit = "credit";
	// Who ticked "Everyone recognizable in this photo agreed to be shown", and
	// when (CM3 OQ6). A plain people.id, not an FK (EV1 OQ8 (e)): honuware's
	// FKs always cascade, so a link would delete the photos a person added when
	// their account is deleted.
	inline constexpr std::string_view kEventGalleryPhotosConsentConfirmedByPersonId = "consent_confirmed_by_person_id";
	inline constexpr std::string_view kEventGalleryPhotosConsentConfirmedUs = "consent_confirmed_us";
	inline constexpr std::string_view kEventGalleryPhotosCreatedUs = "created_us";
	inline constexpr std::string_view kEventGalleryPhotosUpdatedUs = "updated_us";

	inline constexpr std::size_t kEventGalleryPhotosAltTextMaxLength = 300;
	inline constexpr std::size_t kEventGalleryPhotosCaptionMaxLength = 300;
	inline constexpr std::size_t kEventGalleryPhotosCreditMaxLength = 120;

	inline constexpr std::string_view kEventGalleryPhotosExactlyOneParentCheck = "event_gallery_photos_exactly_one_parent";
	inline constexpr std::string_view kEventGalleryPhotosAltTextCheck = "event_gallery_photos_alt_text_check";
	inline constexpr std::string_view kEventGalleryPhotosCaptionCheck = "event_gallery_photos_caption_check";
	inline constexpr std::string_view kEventGalleryPhotosCreditCheck = "event_gallery_photos_credit_check";

	void MakeEventGalleryPhotosTable(DatabaseInfo databaseInfo);

	// Called from CreateAppTables once the table exists; live databases only (the
	// test harness builds tables from MakeDatabaseInfo and never runs this).
	void CreateEventGalleryPhotosIndexes(Transaction& transaction);

}  // namespace DbSchema
