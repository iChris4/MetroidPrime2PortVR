#pragma once

// The special menu: the port's own in-game pages, drawn by the game's own GUI (a
// PortGameUi::Page, port_game_ui_page.h) so a player who set the game up in F1 can stay in the
// game's look while playing. The "Special menu" binding (L3 + R3 on a pad, F2) opens it in game.
// While it is open the game stands still under the pause screen's blur and dark, and reads a
// neutral pad, as with the F1 overlay. Its pages are named along the top: left and right (or a
// click on a name) turn through them, and each has its actions along the bottom (a press of the
// action's button, or a click on it).
//  - Save states: the eight slots. A loads the selected one, Y saves to it (overwriting asks first),
//    X makes it the quick slot (the one the Save state / Load state bindings, F5 / F9, use).
//  - Memory card: the card's three saves, which A loads (after a Yes) by quitting to the title and
//    starting that save as the file select would (PortSkipFrontEnd::QueueCardLoad); the card is
//    never written from here.
// The pages of rows below are tables of port_menu_entry.h entries, the same ones F1 draws: A or a
// click turns a switch on or off or does an action (one that needs the game running, a warp or
// photo mode, closes the menu; Return to title and Exit game ask first). A long page scrolls.
//  - Game: the play options (port_play_options.h): Hide helmet, Hide visor effects, Third-person
//    camera, the on-screen in-game time; Show current hint, Photo mode, Return to title, Exit game.
//  - Cheats: port_cheats.h's switches worth flipping mid-game, Full health, Grant everything.
//  - Items: every upgrade and key, on while Samus has it: give or take each.
//  - Warp: the previous or next room or world, the room's twin in the other dimension (a note says
//    when it has none), the room rebuilt.
// B closes it.

#include <string>

namespace PortSpecialMenu {
// The binding's press (PortInputDevices::PollPortActions): opens it in game, closes it if open.
void Toggle(bool inGame);
bool Open();
// Reads the input and draws it (main.cpp, after the IOWin draw).
void Draw();
// Scripted input (the console's `specialmenu`): "open" / "close", or one press for the page
// (PortGameUi::ScriptPagePress: up, down, left, right, a, b, x, y, start). False for an unknown name.
bool Press(const std::string& what);
// What it shows, for the console: closed, or the page, the selected row or slot (and a switch's
// value), a question's choice and the last result.
std::string Describe();
} // namespace PortSpecialMenu
