#pragma once

#include "sql_util/database_access/transaction.h"
#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// EV3 §1 — the envelope of one public submission: who sent it, what they told
	// the curators, and how it was decided. The event details themselves live on
	// its 1–12 `events` rows (origin user_submitted), which point back here
	// through events.submission_id.
	//
	// Curator-only: never on the public allowed_tables list, and never shown on a
	// public event page (GD1 §6.9, no outing).
	inline constexpr std::string_view kEventSubmissionsTable = "event_submissions";
	inline constexpr std::string_view kEventSubmissionsId = "id";
	// A people.id, but not an FK (EV1 OQ8 (e)): honuware's FKs always cascade,
	// and deleting an account must not delete the events it submitted.
	inline constexpr std::string_view kEventSubmissionsSubmittedByPersonId = "submitted_by_person_id";
	inline constexpr std::string_view kEventSubmissionsSubmitterNote = "submitter_note";
	inline constexpr std::string_view kEventSubmissionsRelationship = "relationship";
	// "This is a regular thing": a hint that a curator may want a series.
	inline constexpr std::string_view kEventSubmissionsRegularThing = "regular_thing";
	// One reason per submission, not per date: "Reject all" is the normal case.
	// NULL until a date is rejected.
	inline constexpr std::string_view kEventSubmissionsRejectReason = "reject_reason";
	inline constexpr std::string_view kEventSubmissionsMessageToSubmitter = "message_to_submitter";
	// NULL = the decision email has not been sent. Stamped once, when the last
	// pending date is decided, so the email goes out exactly once.
	inline constexpr std::string_view kEventSubmissionsDecisionNotifiedUs = "decision_notified_us";
	inline constexpr std::string_view kEventSubmissionsCreatedUs = "created_us";
	inline constexpr std::string_view kEventSubmissionsUpdatedUs = "updated_us";

	inline constexpr std::string_view kEventSubmissionsRelationshipOrganizer = "organizer";
	inline constexpr std::string_view kEventSubmissionsRelationshipVenue = "venue";
	inline constexpr std::string_view kEventSubmissionsRelationshipPerformer = "performer";
	inline constexpr std::string_view kEventSubmissionsRelationshipAttendee = "attendee";
	inline constexpr std::string_view kEventSubmissionsRelationshipOther = "other";

	// The six a curator picks from.
	inline constexpr std::string_view kEventSubmissionsRejectReasonDuplicate = "duplicate";
	inline constexpr std::string_view kEventSubmissionsRejectReasonNotAFit = "not-a-fit";
	inline constexpr std::string_view kEventSubmissionsRejectReasonMissingDetails = "missing-details";
	inline constexpr std::string_view kEventSubmissionsRejectReasonPast = "past";
	inline constexpr std::string_view kEventSubmissionsRejectReasonSpam = "spam";
	inline constexpr std::string_view kEventSubmissionsRejectReasonOther = "other";
	// Set by the submitter's own Withdraw.
	inline constexpr std::string_view kEventSubmissionsRejectReasonWithdrawn = "withdrawn";
	// Set by EV4's expiry sweep when the dates passed before anyone reviewed them.
	inline constexpr std::string_view kEventSubmissionsRejectReasonExpired = "expired";

	inline constexpr std::string_view kEventSubmissionsRelationshipCheck = "event_submissions_relationship_check";
	inline constexpr std::string_view kEventSubmissionsRejectReasonCheck = "event_submissions_reject_reason_check";

	void MakeEventSubmissionsTable(DatabaseInfo databaseInfo);

	// Called from CreateAppTables once the table exists; live databases only (the
	// test harness builds tables from MakeDatabaseInfo and never runs this).
	void CreateEventSubmissionsIndexes(Transaction& transaction);

}  // namespace DbSchema
