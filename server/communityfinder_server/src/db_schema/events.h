#pragma once

#include "sql_util/database_access/transaction.h"
#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV1 Slice 2 — one row per dated occurrence. Recurring events are concrete
	// instances grouped by `series`, never generated on the fly.
	//
	// A pipeline item, not a listing: it carries the review workflow (`status` +
	// the reviewer quartet) instead of GD1 §5's freshness columns. The public
	// sees only `approved` rows, and only through the events endpoints — this
	// table is deliberately not on the public allowed_tables list, so pending
	// and rejected rows never leak through the generic table reads.
	inline constexpr std::string_view kEventsTable = "events";
	inline constexpr std::string_view kEventsId = "id";
	inline constexpr std::string_view kEventsTitle = "title";
	inline constexpr std::string_view kEventsDescription = "description";
	// Real UTC instants, rendered in America/Los_Angeles.
	inline constexpr std::string_view kEventsStartsAtUs = "starts_at_us";
	// NULL = no stated end.
	inline constexpr std::string_view kEventsEndsAtUs = "ends_at_us";
	inline constexpr std::string_view kEventsVenueId = "venue_id";
	// Where it is when there is no venue row (a park, "DM for address").
	inline constexpr std::string_view kEventsLocationText = "location_text";
	inline constexpr std::string_view kEventsUrl = "url";
	inline constexpr std::string_view kEventsCostText = "cost_text";
	// NULL = entered by hand, not ingested.
	inline constexpr std::string_view kEventsSourceId = "source_id";
	// NULL = a one-off.
	inline constexpr std::string_view kEventsSeriesId = "series_id";
	// The source's own id for this item (an ICS UID, a ticket-page path). NULL
	// for events that did not come from an ingest — nullable rather than '' so
	// two hand-entered events credited to the same source never collide on the
	// (source_id, external_key) constraint below.
	inline constexpr std::string_view kEventsExternalKey = "external_key";
	inline constexpr std::string_view kEventsOrigin = "origin";
	inline constexpr std::string_view kEventsStatus = "status";
	// The two person columns hold a people.id but are not FKs, so deleting an
	// account never deletes the events it entered or reviewed. A deleted
	// person's id simply stops resolving.
	inline constexpr std::string_view kEventsCreatedByPersonId = "created_by_person_id";
	// The review quartet: who last approved or rejected the event, when, and why.
	inline constexpr std::string_view kEventsReviewedByPersonId = "reviewed_by_person_id";
	inline constexpr std::string_view kEventsReviewedUs = "reviewed_us";
	inline constexpr std::string_view kEventsReviewNotes = "review_notes";
	inline constexpr std::string_view kEventsCreatedUs = "created_us";
	inline constexpr std::string_view kEventsUpdatedUs = "updated_us";

	inline constexpr std::string_view kEventsOriginScanned = "scanned";
	inline constexpr std::string_view kEventsOriginManual = "manual";
	// Reserved for EV3's public submission form.
	inline constexpr std::string_view kEventsOriginUserSubmitted = "user_submitted";

	inline constexpr std::string_view kEventsStatusPending = "pending";
	inline constexpr std::string_view kEventsStatusApproved = "approved";
	inline constexpr std::string_view kEventsStatusRejected = "rejected";
	// Set by EV4's archive job once an event is past; nothing sets it yet.
	inline constexpr std::string_view kEventsStatusArchived = "archived";

	inline constexpr std::string_view kEventsOriginCheck = "events_origin_check";
	inline constexpr std::string_view kEventsStatusCheck = "events_status_check";
	// The ingestion idempotency anchor: one event per item per source. Named the
	// way Postgres names the unnamed UNIQUEs (`<table>_<columns>_key`) so every
	// unique constraint reads alike in psql.
	inline constexpr std::string_view kEventsSourceIdExternalKeyUnique = "events_source_id_external_key_key";

	// Gates editing the events domain: the ingest and review endpoints, and —
	// through admin_table_permissions — every events-domain table in the generic
	// CRUD editor, so a curator role need not be a full admin. The framework
	// permission names live in honuware's db_schema/permissions.h; this one is
	// app-specific, so it sits with the table it gates.
	inline constexpr std::string_view kPermissionManageEvents = "manage_events";

	void MakeEventsTable(DatabaseInfo databaseInfo);

	// Called from CreateAppTables once the table exists; live databases only (the
	// test harness builds tables from MakeDatabaseInfo and never runs this).
	void CreateEventsIndexes(Transaction& transaction);

}  // namespace DbSchema
