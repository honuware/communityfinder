#pragma once

#include "command_registry.h"

namespace TestHelper {

// CM3 §6 — local data for event and series media: set-event-video (a video
// link, through ParseVideoLink) and add-gallery-photo (a gallery row and its
// picture from a file, with the logged-in person as the consent record).
void RegisterMediaCommands(CommandRegistry& registry);

}  // namespace TestHelper
