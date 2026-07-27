#pragma once

#include "command_runner.h"

namespace TestHelper {

// Run the FTXUI dashboard with modal switch to REPL.
// Returns 0 on clean exit.
int RunDashboard(TestHelperContext& ctx);

}  // namespace TestHelper
