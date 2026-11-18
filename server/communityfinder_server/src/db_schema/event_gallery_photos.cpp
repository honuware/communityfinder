#include "event_gallery_photos.h"

#include <string>

#include "events.h"
#include "series.h"

namespace DbSchema {

	void MakeEventGalleryPhotosTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kEventGalleryPhotosTable);
		databaseInfo.AddColumnPrimaryKey(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kEventsTable,
			kEventsId,
			kEventGalleryPhotosTable,
			kEventGalleryPhotosEventId);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kSeriesTable,
			kSeriesId,
			kEventGalleryPhotosTable,
			kEventGalleryPhotosSeriesId);
		// Exactly one parent: a photo with none would hang off nothing, and one
		// with both would show on two pages.
		databaseInfo.AddCheckConstraint(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosExactlyOneParentCheck,
			"num_nonnulls(" + std::string(kEventGalleryPhotosEventId) + ", " +
				std::string(kEventGalleryPhotosSeriesId) + ") = 1");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosSortOrder,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultZero);
		// Alt text is required and never blank; the lengths match the editor's.
		// The CHECKs back up the generic editor, which no app code sees.
		databaseInfo.AddColumnSimple(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosAltText,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosAltTextCheck,
			"btrim(" + std::string(kEventGalleryPhotosAltText) + ") <> '' AND char_length(" +
				std::string(kEventGalleryPhotosAltText) + ") <= " +
				std::to_string(kEventGalleryPhotosAltTextMaxLength));
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosCaption,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddCheckConstraint(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosCaptionCheck,
			"char_length(" + std::string(kEventGalleryPhotosCaption) + ") <= " +
				std::to_string(kEventGalleryPhotosCaptionMaxLength));
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosCredit,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddCheckConstraint(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosCreditCheck,
			"char_length(" + std::string(kEventGalleryPhotosCredit) + ") <= " +
				std::to_string(kEventGalleryPhotosCreditMaxLength));
		// No defaults: a row without its consent record can't be added.
		databaseInfo.AddColumnSimple(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosConsentConfirmedByPersonId,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnSimple(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosConsentConfirmedUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventGalleryPhotosTable,
			kEventGalleryPhotosUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

	void CreateEventGalleryPhotosIndexes(Transaction& transaction) {
		// A gallery in order (GetGalleryForEvent / GetGalleryForSeries) and its
		// count against the cap — and the cascade when the parent is deleted.
		transaction.RunSqlStatement(
			"CREATE INDEX IF NOT EXISTS event_gallery_photos_event_idx "
			"ON event_gallery_photos (event_id, sort_order)");
		transaction.RunSqlStatement(
			"CREATE INDEX IF NOT EXISTS event_gallery_photos_series_idx "
			"ON event_gallery_photos (series_id, sort_order)");
	}

}  // namespace DbSchema
