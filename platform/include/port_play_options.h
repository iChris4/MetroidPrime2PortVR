#pragma once

// The play options: settings and actions worth reaching while playing, apart from any menu, for
// whatever draws them (the F1 overlay's rows, and the special menu's Game page,
// port_special_menu.h). The switches are saved settings (PortOverlay's), so a change in either
// menu is written to the settings file. Game thread only.
#include "port_menu_entry.h"

#include <span>

namespace PortPlayOptions {

using Switch = PortMenuEntry::Switch;
using Action = PortMenuEntry::Action;

enum class Option {
  HideHelmet,
  HideVisorEffects,
  ThirdPerson,
  IgtTimer, // the on-screen in-game time
  Count,
};
const Switch& Get(Option option);
std::span< const Switch > Switches();

// Show current hint, Photo mode, Return to title, Exit game.
std::span< const Action > Actions();

} // namespace PortPlayOptions
