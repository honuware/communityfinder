#include "event_submissions.h"

#include <string>

namespace DbSchema {

	void MakeEventSubmissionsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kEventSubmissionsTable);
		databaseInfo.AddColumnPrimaryKey(
			kEventSubmissionsTable,
			kEventSubmissionsId,
			DB_TYPE_BIGSERIAL);
		databaseInfo.AddColumnSimple(
			kEventSubmissionsTable,
			kEventSubmissionsSubmittedByPersonId,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSubmissionsTable,
			kEventSubmissionsSubmitterNote,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSubmissionsTable,
			kEventSubmissionsRelationship,
			DB_TYPE_STRING,
			"'other'");
		databaseInfo.AddCheckConstraint(
			kEventSubmissionsTable,
			kEventSubmissionsRelationshipCheck,
			std::string(kEventSubmissionsRelationship) + " IN ('" +
				std::string(kEventSubmissionsRelationshipOrganizer) + "', '" +
				std::string(kEventSubmissionsRelationshipVenue) + "', '" +
				std::string(kEventSubmissionsRelationshipPerformer) + "', '" +
				std::string(kEventSubmissionsRelationshipAttendee) + "', '" +
				std::string(kEventSubmissionsRelationshipOther) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSubmissionsTable,
			kEventSubmissionsRegularThing,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultFalse);
		// A CHECK passes on NULL, so an undecided submission needs no reason.
		databaseInfo.AddColumnNullable(
			kEventSubmissionsTable,
			kEventSubmissionsRejectReason,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kEventSubmissionsTable,
			kEventSubmissionsRejectReasonCheck,
			std::string(kEventSubmissionsRejectReason) + " IN ('" +
				std::string(kEventSubmissionsRejectReasonDuplicate) + "', '" +
				std::string(kEventSubmissionsRejectReasonNotAFit) + "', '" +
				std::string(kEventSubmissionsRejectReasonMissingDetails) + "', '" +
				std::string(kEventSubmissionsRejectReasonPast) + "', '" +
				std::string(kEventSubmissionsRejectReasonSpam) + "', '" +
				std::string(kEventSubmissionsRejectReasonOther) + "', '" +
				std::string(kEventSubmissionsRejectReasonWithdrawn) + "', '" +
				std::string(kEventSubmissionsRejectReasonExpired) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSubmissionsTable,
			kEventSubmissionsMessageToSubmitter,
			DB_TYPE_STRING,
			"''");
		databaseInfo.AddColumnNullable(
			kEventSubmissionsTable,
			kEventSubmissionsDecisionNotifiedUs,
			DB_TYPE_BIGINT);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSubmissionsTable,
			kEventSubmissionsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kEventSubmissionsTable,
			kEventSubmissionsUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

	void CreateEventSubmissionsIndexes(Transaction& transaction) {
		// "My submissions" (newest first) and the rolling 24-hour rate limit,
		// both per person.
		transaction.RunSqlStatement(
			"CREATE INDEX IF NOT EXISTS event_submissions_person_created_idx "
			"ON event_submissions (submitted_by_person_id, created_us)");
	}

}  // namespace DbSchema
