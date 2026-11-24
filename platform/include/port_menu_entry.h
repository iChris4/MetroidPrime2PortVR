#pragma once

// A menu entry apart from any menu: what a switch or an action is called, says and does, for
// whatever draws it (the F1 overlay, the special menu, port_special_menu.h). The cheats
// (port_cheats.h) and the play options (port_play_options.h) are tables of these, so each menu
// reads the same names, help and state.
#include "port_overlay.h"

namespace PortMenuEntry {

// Something on or off.
struct Switch {
  const char* name;
  const char* help;    // one short line (the special menu's line under its list)
  const char* details; // the whole story, for a tooltip (F1)
  bool (*get)();       // as set: Original experience's hold is Blocked's, not this
  void (*set)(bool);
  bool retailOff = false; // held off while Original experience is on (F1 > Game)
};

// The switch is held off by Original experience: shown off, and not to be turned on.
inline bool Blocked(const Switch& entry) { return entry.retailOff && PortOverlay::OriginalExperience(); }

// Something done once.
struct Action {
  const char* name;
  const char* help;
  const char* verb;           // its button's word ("Refill")
  const char* done;           // what it did ("Energy refilled"); unused when it closes the menu
  bool (*run)();              // false: it could not be done now (outside gameplay)
  const char* confirm = nullptr; // a question to answer Yes first ("Exit the game?")
  bool closes = false;           // the game has to run for it (a warp, photo mode): the menu closes
  bool (*available)() = nullptr; // false: shown but not to be done now; null: always
  const char* unavailable = "Only while playing"; // why, when not available
};

inline bool Available(const Action& entry) { return entry.available == nullptr || entry.available(); }

} // namespace PortMenuEntry
