// The cheats, apart from any menu (include/port_cheats.h).
#include "port_cheats.h"

#include "port_overlay.h"
#include "port_savestate.h"
#include "port_warp.h"

#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/ScriptObjects/CScriptConditionalRelay.hpp"

#include <iterator>

namespace PortCheats {
namespace {
// In Cheat's order.
constexpr Switch kSwitches[] = {
    {"God mode", "You take no damage", "The player takes no damage; other actors still do. Not saved.",
     PortOverlay::GodMode, PortOverlay::SetGodMode, false},
    {"No Dark Aether damage", "Dark Aether's air doesn't hurt you",
     "Dark Aether's air does not hurt, as with the Light Suit; light crystals still heal. Not saved.",
     PortOverlay::NoDarkAetherDamage, PortOverlay::SetNoDarkAetherDamage, false},
    {"Infinite ammo", "Missiles, Power Bombs and beam ammo never run out",
     "Firing spends no Missiles, Power Bombs, or Light or Dark Ammo. Not saved.",
     [] { return PortOverlay::InfiniteAmmo(PortOverlay::kInfAmmoAll); },
     [](bool on) { PortOverlay::SetInfiniteAmmo(PortOverlay::kInfAmmoAll, on); }, false},
    {"Unlock beam doors", "Colour-locked doors open to any beam. Kept as a setting",
     "Colour-locked doors open to any of the four beams. Missile locks stay.", PortOverlay::UnlockBeamDoorsSet,
     PortOverlay::SetUnlockBeamDoors, true},
    {"Stack beams", "Each shot also fires your other beams",
     "Each shot also fires the other beams you own, at no extra ammo cost.", PortOverlay::StackBeamsSet,
     PortOverlay::SetStackBeams, true},
    {"Leave out Dark Beam", "Stacked shots skip the Dark Beam",
     "Stacked shots skip the Dark Beam unless it's the one in hand, so they don't drain light beacons and "
     "crystals.",
     PortOverlay::StackBeamsSkipDark, PortOverlay::SetStackBeamsSkipDark, true},
    {"Moon jump", "Hold Jump in the air to rise",
     "Holding Jump in the air lifts Samus at a steady speed, on foot only. Not saved.", PortOverlay::MoonJumpSet,
     PortOverlay::SetMoonJump, true},
    {"Boost anywhere", "The Boost Ball also fires in the air",
     "The Boost Ball also fires in the air, after a bomb or spring jump, and a boost no longer stops short when "
     "the ball leaves the ground, so a boost into a spring jump keeps its height. Not saved.",
     PortOverlay::BoostAnywhereSet, PortOverlay::SetBoostAnywhere, true},
    {"Steer while boosting", "The stick steers a boosted ball",
     "The stick turns a boosted ball's course, keeping its speed. Not saved.", PortOverlay::BoostSteerSet,
     PortOverlay::SetBoostSteer, true},
    {"Phazon shot anywhere", "A full charge fires the Phazon shot. Needs the Charge Beam",
     "A full charge turns into the Phazon shot of the final fight against Dark Samus, in any room and with any "
     "beam, at no ammo cost. Needs the Charge Beam. Not saved.",
     PortOverlay::PhazonShotSet, PortOverlay::SetPhazonShot, true},
};
static_assert(std::size(kSwitches) == static_cast< size_t >(Cheat::Count));

constexpr Action kActions[] = {
    {"Full health", "Refills your energy", "Refill", "Energy refilled", FullHealth},
    {"Grant everything", "Every upgrade and key, full ammo and Energy Tanks", "Grant", "Everything granted",
     GrantEverything},
};

bool InGame() { return PlayerInGame() != nullptr; }

constexpr Action kWarps[] = {
    {"Previous area", "Warp to the previous room in this world", "Warp", nullptr,
     [] { return PortWarp::CycleArea(-1), true; }, nullptr, true, InGame},
    {"Next area", "Warp to the next room in this world", "Warp", nullptr, [] { return PortWarp::CycleArea(1), true; },
     nullptr, true, InGame},
    {"Previous world", "Warp to the previous world", "Warp", nullptr, [] { return PortWarp::CycleWorld(-1), true; },
     nullptr, true, InGame},
    {"Next world", "Warp to the next world", "Warp", nullptr, [] { return PortWarp::CycleWorld(1), true; }, nullptr,
     true, InGame},
    {"Light / Dark world", "Cross to the same spot in the other dimension, if this room has a twin", "Cross",
     nullptr, [] { return PortWarp::Dimension(), true; }, nullptr, true, InGame},
    {"Reload room", "Rebuild the room from the disc, as leaving and coming back does", "Reload", nullptr,
     [] { return PortSaveState::RequestRoomReload(), true; }, nullptr, true, InGame},
};
static_assert(std::size(kWarps) == static_cast< size_t >(Warp::Count));

constexpr Item kAbilities[] = {
    {"Power Beam", CPlayerState::kIT_PowerBeam},
    {"Dark Beam", CPlayerState::kIT_DarkBeam},
    {"Light Beam", CPlayerState::kIT_LightBeam},
    {"Annihilator Beam", CPlayerState::kIT_AnnihilatorBeam},
    {"Charge Beam", CPlayerState::kIT_ChargeBeam},
    {"Super Missile", CPlayerState::kIT_SuperMissile},
    {"Darkburst", CPlayerState::kIT_Darkburst},
    {"Sunburst", CPlayerState::kIT_Sunburst},
    {"Sonic Boom", CPlayerState::kIT_SonicBoom},
    {"Combat Visor", CPlayerState::kIT_CombatVisor},
    {"Scan Visor", CPlayerState::kIT_ScanVisor},
    {"Dark Visor", CPlayerState::kIT_DarkVisor},
    {"Echo Visor", CPlayerState::kIT_EchoVisor},
    {"Varia Suit", CPlayerState::kIT_VariaSuit},
    {"Dark Suit", CPlayerState::kIT_DarkSuit},
    {"Light Suit", CPlayerState::kIT_LightSuit},
    {"Morph Ball", CPlayerState::kIT_MorphBall},
    {"Boost Ball", CPlayerState::kIT_BoostBall},
    {"Spider Ball", CPlayerState::kIT_SpiderBall},
    {"Morph Ball Bombs", CPlayerState::kIT_MorphBallBombs},
    {"Grapple Beam", CPlayerState::kIT_GrappleBeam},
    {"Space Jump Boots", CPlayerState::kIT_SpaceJumpBoots},
    {"Gravity Boost", CPlayerState::kIT_GravityBoost},
    {"Seeker Launcher", CPlayerState::kIT_SeekerLauncher},
    {"Screw Attack", CPlayerState::kIT_ScrewAttack},
    {"Violet Translator", CPlayerState::kIT_VioletTranslator},
    {"Amber Translator", CPlayerState::kIT_AmberTranslator},
    {"Emerald Translator", CPlayerState::kIT_EmeraldTranslator},
    {"Cobalt Translator", CPlayerState::kIT_CobaltTranslator},
};

constexpr Item kKeys[] = {
    {"Sky Temple Key 1", CPlayerState::kIT_TempleKey1},
    {"Sky Temple Key 2", CPlayerState::kIT_TempleKey2},
    {"Sky Temple Key 3", CPlayerState::kIT_TempleKey3},
    {"Sky Temple Key 4", CPlayerState::kIT_TempleKey4},
    {"Sky Temple Key 5", CPlayerState::kIT_TempleKey5},
    {"Sky Temple Key 6", CPlayerState::kIT_TempleKey6},
    {"Sky Temple Key 7", CPlayerState::kIT_TempleKey7},
    {"Sky Temple Key 8", CPlayerState::kIT_TempleKey8},
    {"Sky Temple Key 9", CPlayerState::kIT_TempleKey9},
    {"Agon Key 1", CPlayerState::kIT_AgonKey1},
    {"Agon Key 2", CPlayerState::kIT_AgonKey2},
    {"Agon Key 3", CPlayerState::kIT_AgonKey3},
    {"Torvus Key 1", CPlayerState::kIT_TorvusKey1},
    {"Torvus Key 2", CPlayerState::kIT_TorvusKey2},
    {"Torvus Key 3", CPlayerState::kIT_TorvusKey3},
    {"Hive Key 1", CPlayerState::kIT_HiveKey1},
    {"Hive Key 2", CPlayerState::kIT_HiveKey2},
    {"Hive Key 3", CPlayerState::kIT_HiveKey3},
};

} // namespace

const Switch& Get(Cheat cheat) { return kSwitches[static_cast< int >(cheat)]; }

std::span< const Action > Actions() { return kActions; }

std::span< const Action > Warps() { return kWarps; }

const Action& Get(Warp warp) { return kWarps[static_cast< int >(warp)]; }

std::span< const Item > Abilities() { return kAbilities; }

std::span< const Item > Keys() { return kKeys; }

// The Prime 1 port's GrantItem in this game's terms (`capacity` is MP1's power-up, `amount` its
// pickup), on the console's `give`/`take` path. The capacity moves by the difference, as AddPowerUp
// takes an expansion, and the amount is then set outright: lowering a counter must lower the amount
// even when the capacity has room for it (which AddPowerUp alone would not do).
void GrantItem(CPlayerState& ps, CPlayerState::EItemType type, int capacity, int amount) {
  ps.AddPowerUp(type, capacity - ps.GetItemCapacity(type));
  ps.SetItemAmount(type, amount);
  // A translator gate reads the inventory once, when its area loads; this has it read again.
  CScriptConditionalRelay::PortNoteInventoryCheat();
}

CPlayerState* PlayerInGame() {
  if (gpGameState == nullptr || gpGameState->GetGameMode().GetGameModeType() == 'FRND') {
    return nullptr;
  }
  return gpGameState->PlayerState(0).GetPtr();
}

bool FullHealth() {
  CPlayerState* ps = PlayerInGame();
  if (ps == nullptr) {
    return false;
  }
  ps->HealthInfo()->SetHP(ps->CalculateHealth());
  return true;
}

bool GrantEverything() {
  CPlayerState* ps = PlayerInGame();
  if (ps == nullptr) {
    return false;
  }
  for (const Item& item : kAbilities) {
    GrantItem(*ps, item.type, 1, 1);
  }
  for (const Item& item : kKeys) {
    GrantItem(*ps, item.type, 1, 1);
  }
  for (const CPlayerState::EItemType counter :
       {CPlayerState::kIT_Missile, CPlayerState::kIT_Powerbomb, CPlayerState::kIT_EnergyTanks,
        CPlayerState::kIT_DarkAmmo, CPlayerState::kIT_LightAmmo}) {
    const int max = CPlayerState::PortPowerUpMaxValue(counter);
    GrantItem(*ps, counter, max, max);
  }
  ps->HealthInfo()->SetHP(ps->CalculateHealth());
  return true;
}
} // namespace PortCheats
