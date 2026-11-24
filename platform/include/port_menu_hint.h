#pragma once

// The launch hint "Press F1 for settings" as the game's own text (port_game_ui.h), in the top left
// corner for 8 seconds after it first shows, fading out over the last one; gone for good once the
// settings open. Video > "Show F1 hint at launch" (PortOverlay::ShowMenuHint).
#include "port_game_ui.h"

namespace PortMenuHint {
// Draws it over whatever the game drew (the boot screens, the title screen or the game), on the
// game thread after the IOWin draw.
void Draw();
// Where it was last drawn, invalid when it is not up: the top centre toasts (port_update_toast.h,
// port_note_toast.h) go below it where they would meet it, on a narrow screen.
PortGameUi::ScreenRect LastRect();
} // namespace PortMenuHint
