#pragma once

#include "command_registry.h"

namespace TestHelper {

// GD2 §5 — simulation commands for the guide: create-demo-guide,
// publish-page, verify-listing and list-graveyard. Each prints the ids it
// touches.
void RegisterGuideCommands(CommandRegistry& registry);

}  // namespace TestHelper
