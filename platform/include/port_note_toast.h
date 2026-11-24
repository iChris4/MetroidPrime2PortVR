#pragma once

// A short note ("Save state slot 3 (empty)", a save state's result, a custom action's note) in the
// game's text (port_game_ui.h), top centre, for two and a half seconds, fading out at the end, as
// the shader compile's count is (port_shader_toast.h), below it and the update toast
// (port_update_toast.h) while they are up. PortDebug::ShowNote shows one; a new note
// replaces the one up.
#include <string>

namespace PortNoteToast {
// Any thread.
void Show(const std::string& text);
// Draws it over whatever the game drew, on the game thread after the IOWin draw.
void Draw();
// Its bottom, in fractions of the screen's height from its top, or 0 when it is not up: the
// overlay's own top toasts go below it (BeginTopToastStack).
float Bottom();
} // namespace PortNoteToast
