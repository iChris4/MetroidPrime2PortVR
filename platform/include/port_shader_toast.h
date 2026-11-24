#pragma once

// "Compiling shaders 12 / 340" in the game's text (port_game_ui.h), top centre, while the
// startup shader compile runs (the pipeline cache queued before the first frame): after half a
// second, until that queue has drained; shaders met in game later compile without it. Video >
// "Show shader compilation" (PortOverlay::ShowShaderCompilation). From MP1's overlay toast.
namespace PortShaderToast {
// Draws it over whatever the game drew, on the game thread after the IOWin draw.
void Draw();
// The bottom of the count, in fractions of the screen's height from its top, or 0 when it is not up:
// the update and note toasts (port_update_toast.h, port_note_toast.h) go below it.
float Bottom();
} // namespace PortShaderToast
