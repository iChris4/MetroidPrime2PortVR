#pragma once

#include "port_game_ui.h"

// The cinematic skip's "Skip cutscene?" prompt (PortOverlay::SkipConfirm*) as a game box
// (port_game_ui.h): the scan box's line, no fill, at the bottom of the screen over the letterbox, while the
// skip is armed. Enhancements > Pace > Game-style skip prompt (PortOverlay::SkipPromptBox); off, the
// overlay draws its own "Press [A] to skip".
namespace PortSkipPrompt {
// Draws the box while the skip is armed (CInGameGuiManager::Draw, over a playing cutscene, under the
// pause screen).
void Draw();
// main.cpp, after the IOWin draw: draws it if Draw didn't this frame. The game loader's intro text
// arms the skip too, and no CInGameGuiManager draws there.
void DrawLate();
// Where it last drew, for a tap or click on it (the overlay's).
PortGameUi::ScreenRect LastRect();
// True when the box can't be drawn (its frame didn't load): the overlay draws its own prompt.
bool Unavailable();
} // namespace PortSkipPrompt
