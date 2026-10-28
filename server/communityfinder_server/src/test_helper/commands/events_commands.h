#pragma once

#include "command_registry.h"

namespace TestHelper {

// EV1 §6 — simulation commands for the events domain: create-venue,
// create-event, ingest-demo and list-upcoming. Each prints the ids it creates.
void RegisterEventsCommands(CommandRegistry& registry);

}  // namespace TestHelper
