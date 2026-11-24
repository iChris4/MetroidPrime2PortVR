// F1 overlay: the Enhancements page, optional changes to how the game plays or looks that are
// not cheats (they are saved). State and helpers shared with the other parts are in
// debug_ui_internal.h.

#include "debug_ui_internal.h"

#include "port_overlay.h"
#include "port_prime1_tweaks.h"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

#include <imgui.h>

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>

namespace PortDebug {
namespace {

// The Prime 1 disc picked in the file dialog, handed from its callback (any thread) to the next
// frame; empty when cancelled.
std::mutex sPrime1DiscMutex;
std::optional< std::string > sPrime1DiscPick;
std::atomic< bool > sPrime1DiscDialogOpen{false}; // read by the frame outside the lock

void OpenPrime1DiscDialog() {
  int windowCount = 0;
  SDL_Window** windows = SDL_GetWindows(&windowCount);
  SDL_Window* window = windows != nullptr && windowCount > 0 ? windows[0] : nullptr;
  SDL_free(windows);
  static const SDL_DialogFileFilter filters[] = {
      {"GameCube disc images", "iso;gcm;ciso;gcz;rvz;wia;nfs"},
      {"All files", "*"},
  };
  const SDL_DialogFileCallback done = [](void*, const char* const* files, int) {
    std::lock_guard lock(sPrime1DiscMutex);
    sPrime1DiscPick = files != nullptr && files[0] != nullptr ? std::string(files[0]) : std::string();
    sPrime1DiscDialogOpen = false;
  };
  sPrime1DiscDialogOpen = true;
  SDL_ShowOpenFileDialog(done, nullptr, window, filters, 2, nullptr, false);
}

// Prime 1 boost ball's disc: the pick and what was read from it.
void DrawPrime1Disc() {
  {
    std::lock_guard lock(sPrime1DiscMutex);
    if (sPrime1DiscPick.has_value()) {
      if (!sPrime1DiscPick->empty()) {
        PortOverlay::SetPrime1DiscPath(*sPrime1DiscPick);
        PortPrime1Tweaks::Reload();
      }
      sPrime1DiscPick.reset();
    }
  }
  ImGui::BeginDisabled(sPrime1DiscDialogOpen);
  if (ImGui::Button("Choose Prime 1 disc...")) {
    OpenPrime1DiscDialog();
  }
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Your own Metroid Prime 1 GameCube disc. Restore Prime 1 Boost Ball reads\n"
                        "its Boost Ball values (charge times, boost speeds) from it; the port\n"
                        "ships none of them. The option stays greyed out until one is read.");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", PortPrime1Tweaks::Status().c_str());
}

} // namespace

void DrawEnhancementsTab() {
  // Original experience plays the game as it shipped: every enhancement acts as retail.
  ImGui::BeginDisabled(PortOverlay::OriginalExperience());
  ImGui::SeparatorText("Dark Visor");
  Changed(ImGui::Checkbox("Full-screen Dark Visor", &sDarkVisorFull));
  Changed(ImGui::SliderInt("Dark Visor static", &sDarkVisorStatic, 0, PortOverlay::kDarkVisorStaticMax, "%d%%"));

  ImGui::SeparatorText("Ammo");
  Changed(ImGui::Checkbox("Infinite Light and Dark Ammo", &sInfiniteBeamAmmo));
  ImGui::BeginDisabled(sInfiniteBeamAmmo);
  Changed(ImGui::Checkbox("Only charged shots use ammo", &sChargeOnlyBeamAmmo));
  ImGui::SetItemTooltip("Uncharged Dark, Light and Annihilator Beam shots cost no ammo.\n"
                        "Charged shots and combos cost what they always do. Single-player only.");
  ImGui::EndDisabled();

  ImGui::SeparatorText("Music");
  // Read when the next track starts: a room change or a portal hop.
  Changed(ImGui::Checkbox("Light world music in Dark Aether", &sDarkWorldLightMusic));

  ImGui::SeparatorText("Pace");
  // The game reads this every frame in its cinematic flow state.
  Changed(ImGui::Checkbox("Skippable cutscenes", &sSkipCinematics));
  Changed(ImGui::Checkbox("Game-style skip prompt", &sSkipPromptBox));
  ItemHelp("The first press while a cutscene plays asks \"Skip cutscene?\" in a box drawn like the "
           "game's own, at the bottom of the screen; a second press skips. Off: a plain "
           "\"Press [A] to skip\" label.");
  // Read when a pickup is touched and when a map station's message box comes up.
  int seamless = sSeamlessPickups;
  if (ImGui::Combo("Seamless item pickups", &seamless, "Off\0" "All\0" "Repeats only\0")) {
    sSeamlessPickups = std::clamp(seamless, static_cast< int >(PortOverlay::kSeamlessPickups_Off),
                                  static_cast< int >(PortOverlay::kSeamlessPickups_Repeats));
    Changed(true);
  }
  ItemHelp("No control lock, message box or item cutscene: the item's name shows as a HUD memo. "
           "All: every pickup and map station. Repeats only: pickups of an item you already have, "
           "such as a second Missile Expansion or Energy Tank, so each kind's first box still "
           "shows.");
  // Off walks at retail speed; a custom action bound to this switch makes the slider a sprint.
  CheckboxTip("Custom walk speed", &sWalkSpeedEnabled,
                  "Off: retail speed. Bind a custom action to this setting (Toggle or While\n"
                  "held) in Controls to sprint at the speed below.");
  ImGui::BeginDisabled(!sWalkSpeedEnabled);
  int walkSpeed = sWalkSpeed;
  if (ImGui::SliderInt("Walk speed", &walkSpeed, PortOverlay::kWalkSpeedMin,
                       PortOverlay::kWalkSpeedMax, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
    sWalkSpeed = walkSpeed;
    Changed(true);
  }
  ImGui::EndDisabled();
  int scanSpeed = sScanSpeed;
  if (ImGui::SliderInt("Scan speed", &scanSpeed, PortOverlay::kScanSpeedMin,
                       PortOverlay::kScanSpeedMax, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
    sScanSpeed = scanSpeed;
    Changed(true);
  }
  int beamSwitchSpeed = sBeamSwitchSpeed;
  if (ImGui::SliderInt("Beam switch speed", &beamSwitchSpeed, PortOverlay::kBeamSwitchSpeedMin,
                       PortOverlay::kBeamSwitchSpeedMax, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
    sBeamSwitchSpeed = beamSwitchSpeed;
    Changed(true);
  }
  ItemHelp("How fast the cannon closes, morphs and opens on a beam change; 100% is retail. The "
           "rest of the gun keeps its speed.");
  // Elevator and portal share a row: same choices, short labels.
  const float rideWidth = ImGui::GetFontSize() * 6.f;
  int elevatorRide = sElevatorRide;
  ImGui::SetNextItemWidth(rideWidth);
  if (ImGui::Combo("Elevator ride cutscene", &elevatorRide, "Original\0" "Fast\0" "Skip\0")) {
    sElevatorRide = std::clamp(elevatorRide, static_cast< int >(PortOverlay::kElevatorRide_Original),
                               static_cast< int >(PortOverlay::kElevatorRide_Skip));
    Changed(true);
  }
  ImGui::SameLine();
  int portalTransition = sPortalTransition;
  ImGui::SetNextItemWidth(rideWidth);
  if (ImGui::Combo("Portal transition", &portalTransition, "Original\0" "Fast\0" "Skip\0")) {
    sPortalTransition = std::clamp(portalTransition,
                                   static_cast< int >(PortOverlay::kPortalTransition_Original),
                                   static_cast< int >(PortOverlay::kPortalTransition_Skip));
    Changed(true);
  }
  int rotatingRooms = sRotatingRooms;
  ImGui::SetNextItemWidth(rideWidth);
  if (ImGui::Combo("Energy controller access cutscene", &rotatingRooms, "Original\0" "Fast\0")) {
    sRotatingRooms = std::clamp(rotatingRooms, static_cast< int >(PortOverlay::kRotatingRooms_Original),
                                static_cast< int >(PortOverlay::kRotatingRooms_Fast));
    Changed(true);
  }
  int travelCutscenes = sTravelCutscenes;
  ImGui::SetNextItemWidth(rideWidth);
  if (ImGui::Combo("Elevator and light beam cutscenes", &travelCutscenes, "Original\0" "Fast\0")) {
    sTravelCutscenes = std::clamp(travelCutscenes,
                                  static_cast< int >(PortOverlay::kTravelCutscenes_Original),
                                  static_cast< int >(PortOverlay::kTravelCutscenes_Fast));
    Changed(true);
  }
  ItemHelp("Fast plays the cutscenes in the room as you leave or arrive by elevator or light beam "
           "teleporter at double speed. The ride between the worlds is Elevator ride's.");

  ImGui::SeparatorText("Morph Ball");
  Changed(ImGui::Checkbox("Fast Morph", &sFastMorph));
  ImGui::SameLine();
  Changed(ImGui::Checkbox("Spring Ball", &sSpringBall));

  ImGui::SeparatorText("Prime 1 Player Physics");
  // Single player only: CPlayerDynamics and CMorphBall ignore both in multiplayer.
  CheckboxTip("Restore Prime 1 Jumps and Dashes", &sPrime1AirPhysics,
              "Echoes patched most Prime 1 speedrunning techniques that allowed Samus to\n"
              "build up lots of speed while in the air. This option restores Prime 1's jumping behavior,\n"
              "line-for-line. L-Jumping, R-Jumping, and Scan / Combat Dashing work as they did in Prime 1.");
  // The Boost Ball option needs Prime 1's values: the disc comes first, and the option and its
  // slider stay greyed until it has been read (CMorphBall ignores the option without it too).
  DrawPrime1Disc();
  PortPrime1Tweaks::BoostBall prime1Values;
  ImGui::BeginDisabled(!PortPrime1Tweaks::Values(prime1Values));
  CheckboxTip("Restore Prime 1 Boost Ball", &sPrime1BoostBall,
              "Requires your own copy of Prime 1. The tuning values have to be read from a disc.\n"
              "Echoes reduced the rate in which boosts can be charged, and heavily dampens any excess speed\n"
              "you get from boosting. This option restores both to their Prime 1 behavior.\n"
              "WARNING: Prime 1's boosts are pretty aggressive in Prime 2's smaller rooms, and may affect how\n"
              "some puzzles function. The slider below can restore the dampening behavior, scaled to your\n"
              "preference. 25%-33% is a good starting point.");
  ImGui::BeginDisabled(!sPrime1BoostBall);
  int boostFalloff = sPrime1BoostFalloff;
  if (ImGui::SliderInt("Boost Dampening", &boostFalloff, PortOverlay::kPrime1BoostFalloffMin,
                       PortOverlay::kPrime1BoostFalloffMax, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
    sPrime1BoostFalloff = boostFalloff;
    Changed(true);
  }
  ItemHelp("Prime 2 dampens a boost the faster the ball is already moving, essentially a soft speed cap. "
           "Prime 1 never does this (0%), letting you stack boosts on top of one another and pick up lots of speed. 100% is Echoes retail.");
  ImGui::EndDisabled();
  ImGui::EndDisabled();
  ImGui::EndDisabled();

  ImGui::SeparatorText("Hints");
  // The game's own option, the same switch as the pause menu's Hint System and saved with the other
  // game options, so Original experience leaves it alone.
  ImGui::BeginDisabled(gpGameState == nullptr);
  bool hints = gpGameState != nullptr && gpGameState->GameOptions().GetIsHintSystemEnabled();
  if (ImGui::Checkbox("Hints", &hints) && gpGameState != nullptr) {
    gpGameState->GameOptions().SetIsHintSystemEnabled(hints);
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(PortOverlay::OriginalExperience());
  int hintDelay = sHintDelay;
  if (ImGui::SliderInt("Hint delay", &hintDelay, PortOverlay::kHintDelayMin,
                       PortOverlay::kHintDelayMax, hintDelay == 0 ? "Game default" : "%d min",
                       ImGuiSliderFlags_AlwaysClamp)) {
    sHintDelay = hintDelay;
    Changed(true);
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(gpGameState == nullptr ||
                       !gpGameState->GameOptions().GetIsHintSystemEnabled());
  if (ImGui::Button("Show current hint")) {
    PortOverlay::RequestShowHint();
  }
  ImGui::EndDisabled();
}

} // namespace PortDebug
