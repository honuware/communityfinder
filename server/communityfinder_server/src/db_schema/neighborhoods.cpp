#include "neighborhoods.h"

#include <string>

namespace DbSchema {

	void MakeNeighborhoodsTable(DatabaseInfo databaseInfo) {
		databaseInfo.AddTable(kNeighborhoodsTable);
		databaseInfo.AddColumnPrimaryKey(
			kNeighborhoodsTable,
			kNeighborhoodsId,
			DB_TYPE_BIGSERIAL);
		// NOT NULL + a table-level UNIQUE: AddColumnUnique would emit UNIQUE alone,
		// which still admits NULL.
		databaseInfo.AddColumnSimple(
			kNeighborhoodsTable,
			kNeighborhoodsSlug,
			DB_TYPE_STRING);
		databaseInfo.AddUniqueConstraint(kNeighborhoodsTable, kNeighborhoodsSlug);
		databaseInfo.AddColumnSimple(
			kNeighborhoodsTable,
			kNeighborhoodsDisplayName,
			DB_TYPE_STRING);
		// No default: every neighborhood belongs to exactly one region.
		databaseInfo.AddColumnSimple(
			kNeighborhoodsTable,
			kNeighborhoodsRegion,
			DB_TYPE_STRING);
		databaseInfo.AddCheckConstraint(
			kNeighborhoodsTable,
			kNeighborhoodsRegionCheck,
			std::string(kNeighborhoodsRegion) + " IN ('" +
				std::string(kNeighborhoodsRegionSeattleCore) + "', '" +
				std::string(kNeighborhoodsRegionSeattleNorth) + "', '" +
				std::string(kNeighborhoodsRegionSeattleSouth) + "', '" +
				std::string(kNeighborhoodsRegionEastside) + "', '" +
				std::string(kNeighborhoodsRegionSouthKing) + "')");
		databaseInfo.AddColumnNotNullableWithDefault(
			kNeighborhoodsTable,
			kNeighborhoodsSortOrder,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultZero);
		databaseInfo.AddColumnNotNullableWithDefault(
			kNeighborhoodsTable,
			kNeighborhoodsEnabled,
			DB_TYPE_BOOL,
			kDatabaseInfoDefaultTrue);
		databaseInfo.AddColumnNotNullableWithDefault(
			kNeighborhoodsTable,
			kNeighborhoodsCreatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
		databaseInfo.AddColumnNotNullableWithDefault(
			kNeighborhoodsTable,
			kNeighborhoodsUpdatedUs,
			DB_TYPE_BIGINT,
			kDatabaseInfoDefaultNow);
	}

}  // namespace DbSchema
