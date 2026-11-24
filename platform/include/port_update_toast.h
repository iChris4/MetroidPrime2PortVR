#pragma once

// "Version 0.19.0 is out" in the game's text (port_game_ui.h), top centre, once per launch for
// twelve seconds when a newer release than this build is out (PortUpdateCheck), as the note toast
// and the shader count are drawn. A click or tap on it opens the release page: the overlay takes
// the click (DrawUpdateToast) and Android's touch view the tap (PortDebug::TapUpdateToast), both
// over LastRect.
#include "port_game_ui.h"

namespace PortUpdateToast {
// Draws it over whatever the game drew, on the game thread after the IOWin draw.
void Draw();
// It is up, and where it was last drawn.
bool Showing();
PortGameUi::ScreenRect LastRect();
// Opened (or timed out): it is not shown again this launch.
void Dismiss();
} // namespace PortUpdateToast
