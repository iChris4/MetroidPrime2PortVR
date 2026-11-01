// SPDX-License-Identifier: MIT
//
// PrimedGun Echoes: vr/vr_host.h on this port. The VR layer reaches the
// settings file, the F1 overlay's switches, the save states and the user
// texture pack only through here.

#include "vr/vr_host.h"

#include "vr/prime_vr_policy.h"
#include "vr/vr_menu.h"
#include "vr/vr_pad.h"
#include "vr/vr_view.h"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

#include "debug_ui/debug_ui_internal.h"
#include "port_debug.h"
#include "port_logfile.h"
#include "port_overlay.h"
#include "port_overlay_internal.h"
#include "port_savestate.h"
#include "port_textures.h"
#include "port_warp.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <vector>

namespace PortDebug {
// platform/debug_ui/cheats.cpp, the Cheats page's buttons.
void CheatFullHealth();
void CheatGrantEverything();
} // namespace PortDebug

namespace PortVrHost {
namespace {
std::atomic< CStateManager* > sStateManager{nullptr};
std::string sUserFolder;
bool sDrawTags = false;
} // namespace

// The front end runs its own state manager with its own Samus (game mode 'FRND'): it is not
// gameplay for the VR layer.
CStateManager* StateManager() noexcept {
  if (gpGameState != nullptr && gpGameState->GetGameMode().GetGameModeType() == 'FRND') {
    return nullptr;
  }
  return sStateManager.load(std::memory_order_acquire);
}

// As the Metroid Prime VR port's main.cpp: the world counts as in game when the previous presented
// frame drew it; anything else (the front end, transitions) goes on the virtual screen; message
// boxes, the save prompt and the paused menus as CInGameGuiManager notes them.
namespace {
bool sAreaTexturesDumped = false; // game thread
} // namespace

void NoteAreaTexturesDumped(bool dumped) noexcept { sAreaTexturesDumped = dumped; }

void PublishGameMode(bool lastFrameDrewWorld, uint64_t framesDrawn) noexcept {
  PortVr::VRGameMode mode = lastFrameDrewWorld ? PortVr::VRGameMode::InGame : PortVr::VRGameMode::FrontEnd;
  if (lastFrameDrewWorld) {
    if (const CStateManager* mgr = StateManager()) {
      const CCameraManager* cameras = mgr->GetCameraManager(0);
      if (cameras != nullptr && cameras->IsInCinematicCamera()) {
        mode = PortVr::VRGameMode::Cinematic;
      } else if (mgr->GetInMapScreen() || mgr->GetGameState() == CStateManager::kGS_Paused) {
        mode = PortVr::VRGameMode::Paused;
      }
    } else {
      mode = PortVr::VRGameMode::FrontEnd;
    }
  }
  // As the Metroid Prime VR port's main.cpp: a message box or the save prompt reports itself
  // whether or not the world was drawn (on the virtual screen the world stops after the pause
  // blur's first copy). Its mode is kept for the frame it closes on, which draws the world again
  // but follows one that did not: a FrontEnd frame there would re-anchor the virtual screen for one
  // frame. The pause menu, the logbook and the map report Paused the same way; one brought into
  // view after its room's textures were dumped stays on the virtual screen until they are back.
  static PortVr::VRGameMode sBoxMode = PortVr::VRGameMode::Message;
  static int sBoxHoldFrames = 0;
  const bool menuWaits =
      !PortVr::VrMessageBoxOpen() && PortVr::VrGameMenuInView() && sAreaTexturesDumped;
  if (PortVr::VrMessageBoxOpen() || PortVr::VrGameMenuOpen()) {
    mode = menuWaits                        ? PortVr::VRGameMode::FrontEnd
           : !PortVr::VrMessageBoxOpen()     ? PortVr::VRGameMode::Paused
           : PortVr::VrMessageBoxCinematic() ? PortVr::VRGameMode::Cinematic
                                             : PortVr::VRGameMode::Message;
    sBoxMode = mode;
    sBoxHoldFrames = 8;
  } else if (sBoxHoldFrames > 0 && !lastFrameDrewWorld && StateManager() != nullptr) {
    mode = sBoxMode;
    --sBoxHoldFrames;
  } else {
    sBoxHoldFrames = 0;
  }
  PortVr::PrimeVRPolicyPublishGameMode(mode, framesDrawn);
}

void SetStateManager(CStateManager* mgr) noexcept { sStateManager.store(mgr, std::memory_order_release); }

void ForgetStateManager(const CStateManager* mgr) noexcept {
  CStateManager* expected = const_cast< CStateManager* >(mgr);
  if (sStateManager.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
    // No message box, menu or held screen outlives the game (the loop would keep publishing it).
    PortVr::VrNoteMessageBox(false, false);
    PortVr::VrNoteGameMenu(false);
    PortVr::VrNoteInGameMenu(false);
    aurora_set_stereo_screen_anchor(false);
    sAreaTexturesDumped = false;
  }
}

const std::string& UserFolder() noexcept { return sUserFolder; }

void SetUserFolder(const std::string& folder) { sUserFolder = folder; }

void MarkSettingsDirty() { PortOverlaySettings::MarkDirty(); }

void SaveSettingsNow() { PortOverlaySettings::SaveSettings(); }

// The launcher's live edits (debug_ui/settings.cpp PollSettingsFile).
void PollSettingsFile(bool now) { PortOverlaySettings::PollSettingsFile(now); }

// As the desktop toast (platform/debug_ui/video.cpp DrawShaderCompilationToast): while the queue the
// pipeline cache filled at start-up drains, after half a second, the count since then.
bool ShaderCompilationProgress(uint32_t& done, uint32_t& total) {
  static double sQueuedSince = -1.0;
  static uint32_t sBase = 0;
  static bool sStartupDone = false;
  const AuroraStats* stats = aurora_get_stats();
  if (stats != nullptr && stats->queuedPipelines == 0) {
    sStartupDone = true;
  }
  if (!PortOverlay::ShowShaderCompilation() || stats == nullptr || sStartupDone) {
    sQueuedSince = -1.0;
    return false;
  }
  const double now =
      std::chrono::duration< double >(std::chrono::steady_clock::now().time_since_epoch()).count();
  if (sQueuedSince < 0.0) {
    sQueuedSince = now;
    sBase = stats->createdPipelines;
  }
  if (now - sQueuedSince < 0.5) {
    return false;
  }
  done = stats->createdPipelines >= sBase ? stats->createdPipelines - sBase : 0;
  total = done + stats->queuedPipelines;
  return true;
}

void CheatFullHealth() { PortDebug::CheatFullHealth(); }
void CheatGrantEverything() { PortDebug::CheatGrantEverything(); }
bool Invulnerable() { return PortOverlay::GodMode(); }
void SetInvulnerable(bool on) { PortOverlay::SetGodMode(on); }
bool AiAudioEnabled() { return PortDebug::AiAudioEnabled(); }
void SetAiAudioEnabled(bool on) { PortDebug::SetAiAudioEnabled(on); }
bool MusyxAudioEnabled() { return PortDebug::MusyxAudioEnabled(); }
void SetMusyxAudioEnabled(bool on) { PortDebug::SetMusyxAudioEnabled(on); }
bool LogFile() { return PortOverlay::LogFile(); }
void SetLogFile(bool on) { PortOverlay::SetLogFile(on); }

void StartLogFile() {
  if (!sUserFolder.empty()) {
    PortLogFile::Start(sUserFolder);
  }
}

// The splash's prompt (port_savestate.cpp ResumePromptTick), dated as the desktop window dates it
// (debug_ui/window.cpp DrawResumePrompt).
bool ResumeQuestionShown(ResumeQuestion& question) {
  PortSaveState::ResumeInfo info;
  bool yes = true;
  if (!PortSaveState::ResumePromptShown(info, yes)) {
    return false;
  }
  question.room = info.room;
  question.afterCrash = info.crashedAfterThis;
  question.crashedAgain = info.crashedAgain;
  question.saved.clear();
  const std::time_t t = static_cast< std::time_t >(info.savedAt);
  std::tm local{};
#if defined(_WIN32)
  if (localtime_s(&local, &t) == 0) {
#else
  if (localtime_r(&t, &local) != nullptr) {
#endif
    char when[32] = "";
    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
    question.saved = when;
  }
  return true;
}

bool QuickResume() { return PortOverlay::QuickResume(); }
void SetQuickResume(bool on) { PortOverlay::SetQuickResume(on); }
// The Metroid Prime port's draw tags (a debug overlay of draw names) have no counterpart here.
bool DrawTags() { return sDrawTags; }
void SetDrawTags(bool on) { sDrawTags = on; }

// The overlay's room lists (PortOverlay::SetGameState copies them from the live world every
// update, on this thread): real rooms only, cutscene stages left out, a room whose map has not
// said yet listed with Light Aether's. Its world list keeps the card's named worlds; the arenas
// and front ends, which the overlay names from a table of its own, are not offered in the headset.
void AddTeleportPlaces(PortVr::VrMenu::View& view) {
  if (StateManager() == nullptr) {
    return;
  }
  using PortDebug::SRoomEntry;
  view.area = PortDebug::sAreaId;
  view.world = PortDebug::sWorldId;
  for (int i = 0; i < static_cast< int >(PortDebug::sRooms.size()); ++i) {
    const SRoomEntry& room = PortDebug::sRooms[i];
    if (!room.real || room.dimension == SRoomEntry::EDimension::kCutscene) {
      continue;
    }
    (room.dimension == SRoomEntry::EDimension::kDark ? view.dark_rooms : view.rooms)
        .push_back({static_cast< uint32_t >(i), room.name});
  }
  // Rebuilt when the card's table changes size, as the overlay's.
  static size_t sWorldCount = 0;
  static std::vector< PortVr::VrMenu::TeleportPlace > sWorlds;
  if (gpMemoryCard != nullptr && gpMemoryCard->GetMemoryWorlds().size() != sWorldCount) {
    const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
    sWorldCount = worlds.size();
    sWorlds.clear();
    for (int i = 0; i < worlds.size(); ++i) {
      std::string name = PortDebug::PlainWorldName(worlds[i].second.GetFrontEndName());
      if (!name.empty()) {
        sWorlds.push_back({static_cast< uint32_t >(worlds[i].first), std::move(name)});
      }
    }
  }
  view.worlds = sWorlds;
}

void TeleportCycleArea(int delta) { PortWarp::CycleArea(delta); }
void TeleportCycleWorld(int delta) { PortWarp::CycleWorld(delta); }
void TeleportCrossDimension() { PortWarp::Dimension(); }
int TakeDimensionResult() { return PortWarp::TakeDimensionResult(); }
// The overlay's Reload: the save-state route rebuilds the room and keeps Samus where she stands
// (a warp to the room would put her at its spawn point).
void TeleportReloadRoom() { PortSaveState::RequestRoomReload(); }

void TeleportToRoom(uint32_t area) {
  char text[16];
  std::snprintf(text, sizeof(text), "%u", static_cast< unsigned >(area));
  PortWarp::Area(text);
}

void TeleportToWorld(uint32_t world) {
  char text[16];
  std::snprintf(text, sizeof(text), "%08x", static_cast< unsigned >(world));
  PortWarp::World(text, "");
}

int SelectedStateSlot() { return PortSaveState::SelectedSlot(); }
void SetSelectedStateSlot(int slot) { PortSaveState::SetSelectedSlot(slot); }

StateSlotInfo StateSlot(int slot) {
  const PortSaveState::Info info = PortSaveState::SlotInfo(slot);
  return StateSlotInfo{.exists = info.exists, .savedAt = info.savedAt};
}

// Queued for the game thread's next update; a refusal shows up later in LastMessage.
bool RequestLoadState(int slot) {
  PortSaveState::RequestLoad(slot);
  return true;
}

bool RequestSaveState(int slot) {
  PortSaveState::RequestSave(slot);
  return true;
}

std::string LastStateMessage() { return PortSaveState::LastMessage(); }

std::string UserTextureRoot() {
  const char* root = PortTextures::UserRoot();
  return root != nullptr ? std::string(root) : std::string();
}

void RequestUserTexturePackReload() { PortTextures::RequestUserPackReload(); }

} // namespace PortVrHost
