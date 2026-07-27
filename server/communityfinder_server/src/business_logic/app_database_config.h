#pragma once

#include <string_view>

// App-side ownership of the PostgreSQL database name. The framework
// (sql_util) does not hard-code a brand: `DatabaseHelperInit`,
// `MakeProductionDatabaseHelper`, and `MakeDatabaseInfo` all take the name as
// a parameter. The application supplies the value from here — the single seam
// where the multi-tenant plan will later swap in a per-tenant database name.
//
// Parallels business_logic/app_ical_config.h (Phase 1.1 componentization).
namespace App {

inline constexpr std::string_view kDatabaseName = "communityfinder";

}  // namespace App
