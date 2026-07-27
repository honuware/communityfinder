#pragma once

#include <string_view>

#include "sql_util/schema/database_info.h"

namespace DbSchema {

	// The database name is app-owned (see business_logic/app_database_config.h)
	// and passed in explicitly — there is no framework default.
	DatabaseInfo MakeDatabaseInfo(std::string_view databaseName);

}  // namespace DbSchema
