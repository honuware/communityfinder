#pragma once

#include "command_runner.h"

namespace TestHelper {

enum class ReplAction {
    Quit,
    Dashboard,  // Return to FTXUI dashboard
};

// Run the replxx REPL loop.
// Returns when user types 'quit'/'exit' (Quit) or 'back'/'dashboard'/empty Enter (Dashboard).
// prePopulated: optional initial text in the prompt (from dashboard context).
ReplAction RunRepl(TestHelperContext& ctx, const std::string& prePopulated = "");

}  // namespace TestHelper
