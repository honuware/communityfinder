#pragma once

#include "command_registry.h"

namespace TestHelper {

// EV3 §6 — simulation commands for the public submission pipeline:
// submit-event (as a demo submitter, through the same SubmissionHelper the
// form uses, so every check and limit applies), list-submissions (what "My
// submissions" shows), and submission-settings (the two limits and the pause
// switch, as the settings page changes them).
void RegisterSubmissionCommands(CommandRegistry& registry);

}  // namespace TestHelper
