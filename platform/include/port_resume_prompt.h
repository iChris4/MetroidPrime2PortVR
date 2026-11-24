#pragma once

#include "port_game_ui.h"

// The boot's "Resume where you left off?" (PortSaveState::ResumePrompt*) as the game's Yes/No box
// (port_game_ui.h Question, the quit screen's), in the middle of the screen while the splash holds
// for it. The pad answers through the splash (left/right, A or Start); the overlay takes a pointer:
// hovering a choice highlights it, a click answers (DrawResumePrompt).
namespace PortResumePrompt {
// Draws it while the prompt is up (main.cpp, after the IOWin draw).
void Draw();
// Where choice 0 (Yes) or 1 (No) was last drawn; not valid while it isn't shown.
PortGameUi::ScreenRect ChoiceRect(int choice);
// True when the game's box can't be drawn (its frame didn't load): the overlay draws its own.
bool Unavailable();
} // namespace PortResumePrompt
