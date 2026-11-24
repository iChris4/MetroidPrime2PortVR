#pragma once

// The cheats, apart from any menu: what each one is called and does, its state, and the item
// grants, for whatever draws them (the F1 overlay's Cheats rows, debug_ui/cheats.cpp, and the
// special menu's Cheats page, port_special_menu.h). The switches' state is PortOverlay's
// (port_overlay.h), so every menu shows the same values; Unlock beam doors and Stack beams (with
// Leave out Dark Beam) are saved settings, the others last the session. Game thread only.
#include "port_menu_entry.h"

#include "MetroidPrime/Player/CPlayerState.hpp"

#include <span>

namespace PortCheats {

// The cheats that are on or off, each once: a menu draws them by name (the F1 page in its own
// layout, with its sliders between them) or by the list (the special menu's own pick of them).
enum class Cheat {
  GodMode,
  NoDarkAetherDamage,
  InfiniteAmmo, // all four kinds at once (F1 also has a switch per kind, beside each counter)
  UnlockBeamDoors,
  StackBeams,
  StackBeamsSkipDark,
  MoonJump,
  BoostAnywhere,
  BoostSteer,
  PhazonShot,
  Count,
};
using Switch = PortMenuEntry::Switch;
using Action = PortMenuEntry::Action;
const Switch& Get(Cheat cheat);

// The cheats done once: Full health, Grant everything.
std::span< const Action > Actions();
// The warps (the F1 Cheats page's Teleport): the next or previous room or world, the room's twin in
// the other dimension, the room rebuilt. They happen once the game runs (they close the special
// menu); TakeDimensionResult (port_warp.h) says a frame later whether the room had a twin.
enum class Warp { PreviousArea, NextArea, PreviousWorld, NextWorld, Dimension, ReloadRoom, Count };
std::span< const Action > Warps(); // in Warp's order
const Action& Get(Warp warp);

// Samus's player state while a game is being played, else null: the title and file select are a
// game too, in the 'FRND' front-end world, with their own player state and Samus.
CPlayerState* PlayerInGame();
// Energy to full.
bool FullHealth();
// Every upgrade and key, Missiles, Power Bombs, Energy Tanks and beam ammo at their most, energy full.
bool GrantEverything();

// An item to give or take (the upgrades and the keys).
struct Item {
  const char* name;
  CPlayerState::EItemType type;
};
std::span< const Item > Abilities();
std::span< const Item > Keys();
// Sets an item's capacity and amount outright: 0 takes it away (every ability check reads the
// capacity), the most gives a full expansion. The console's `give` path (CScriptPickup's own).
void GrantItem(CPlayerState& ps, CPlayerState::EItemType type, int capacity, int amount);

} // namespace PortCheats
