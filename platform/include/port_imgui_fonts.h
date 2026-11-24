#pragma once

#include <aurora/aurora.h>

namespace PortImGuiFonts {

// Aurora's imGuiInitCallback: builds the overlay's font set before the atlas
// is first built.
void Init(const AuroraWindowSize* size);

} // namespace PortImGuiFonts
