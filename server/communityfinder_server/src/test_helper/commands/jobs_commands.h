#pragma once

#include "command_registry.h"

namespace TestHelper {

// EV4 §7 — the scheduled jobs, on demand and without a server: run-job runs
// one job in-process (the same Jobs::RunScheduledJob its endpoint runs) and
// prints its counts; create-alert records an admin alert for the digest to
// mail; age-event moves an event into the past so archive and expire have
// something to do.
void RegisterJobsCommands(CommandRegistry& registry);

}  // namespace TestHelper
