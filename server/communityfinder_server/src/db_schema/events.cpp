#include "events.h"

#include <string>

#include "event_sources.h"
#include "series.h"
#include "venues.h"

namespace DbSchema {

	void MakeEventsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kEventsTable);
		databaseInfo.AddColumnPrimaryKey(
			kEventsTable,
			kEventsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kEventsTable,
			kEventsTitle,
			DB_TYPE_STRING);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsDescription,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnSimple(
			kEventsTable,
			kEventsStartsAtUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNullable(
			kEventsTable,
			kEventsEndsAtUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kVenuesTable,
			kVenuesId,
			kEventsTable,
			kEventsVenueId);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsLocationText,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsUrl,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsCostText,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnForeignKeyRefNullable(
			kEventSourcesTable,
			kEventSourcesId,
			kEventsTable,
			kEventsSourceId);
		databaseInfo.AddColumnForeignKeyRefNullable(
			kSeriesTable,
			kSeriesId,
			kEventsTable,
			kEventsSeriesId);
		databaseInfo.AddColumnNullable(
			kEventsTable,
			kEventsExternalKey,
			DB_TYPE_STRING);
		// Postgres treats NULLs as distinct, so hand-entered events (NULL
		// source_id) never collide here. Its index also serves the ingest's
		// lookup by (source_id, external_key).
		databaseInfo.AddNamedUniqueConstraint(
			kEventsTable,
			kEventsSourceIdExternalKeyUnique,
			kEventsSourceId,
			kEventsExternalKey);
		// Manual by default: a row added in the admin editor is hand-entered.
		// The ingest and EV3's submission form set their origin explicitly.
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsOrigin,
			DB_TYPE_STRING,
			"'manual'");
		databaseInfo.AddCheckConstraint(
			kEventsTable,
			kEventsOriginCheck,
			std::string(kEventsOrigin) + " IN ('" +
				std::string(kEventsOriginScanned) + "', '" +
				std::string(kEventsOriginManual) + "', '" +
				std::string(kEventsOriginUserSubmitted) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsStatus,
			DB_TYPE_STRING,
			"'pending'");
		databaseInfo.AddCheckConstraint(
			kEventsTable,
			kEventsStatusCheck,
			std::string(kEventsStatus) + " IN ('" +
				std::string(kEventsStatusPending) + "', '" +
				std::string(kEventsStatusApproved) + "', '" +
				std::string(kEventsStatusRejected) + "', '" +
				std::string(kEventsStatusArchived) + "')");
		// Plain ids, deliberately not FKs onto people (EV1 OQ8 (e)): honuware's
		// FKs always cascade, so a link here would delete every event a person
		// entered or reviewed when their account is deleted.
		databaseInfo.AddColumnNullable(
			kEventsTable,
			kEventsCreatedByPersonId,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNullable(
			kEventsTable,
			kEventsReviewedByPersonId,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNullable(
			kEventsTable,
			kEventsReviewedUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsReviewNotes,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventsTable,
			kEventsUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

	void CreateEventsIndexes(Transaction& transaction) {
		// The upcoming query (GetEventsInRange / GetUpcomingPublic): approved
		// events in a start-time window.
		transaction.RunSqlStatement(
			"CREATE INDEX IF NOT EXISTS events_status_starts_at_idx "
			"ON events (status, starts_at_us)");
		// A series page's instances (GetEventsBySeries).
		transaction.RunSqlStatement(
			"CREATE INDEX IF NOT EXISTS events_series_idx "
			"ON events (series_id)");
		// The venue filter on the upcoming query — and the cascade when a venue
		// row is deleted, which would otherwise scan every event.
		transaction.RunSqlStatement(
			"CREATE INDEX IF NOT EXISTS events_venue_idx "
			"ON events (venue_id)");
	}

}  // namespace DbSchema
