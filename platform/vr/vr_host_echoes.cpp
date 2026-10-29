// SPDX-License-Identifier: GPL-3.0-or-later
//
// PrimedGun Echoes: vr/vr_host.h on this port. The VR layer reaches the
// settings file, the F1 overlay's switches, the save states and the user
// texture pack only through here.

#include "vr/vr_host.h"

#include "vr/prime_vr_policy.h"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

#include "port_debug.h"
#include "port_logfile.h"
#include "port_overlay.h"
#include "port_overlay_internal.h"
#include "port_savestate.h"
#include "port_textures.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>

#include <atomic>
#include <chrono>

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
// frame drew it; anything else (the front end, transitions) goes on the virtual screen. Message
// boxes, the save prompt and the immersive pause menus come with the game hooks (docs/PRIMEDGUN.md).
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
  PortVr::PrimeVRPolicyPublishGameMode(mode, framesDrawn);
}

void SetStateManager(CStateManager* mgr) noexcept { sStateManager.store(mgr, std::memory_order_release); }

void ForgetStateManager(const CStateManager* mgr) noexcept {
  CStateManager* expected = const_cast< CStateManager* >(mgr);
  sStateManager.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

const std::string& UserFolder() noexcept { return sUserFolder; }

void SetUserFolder(const std::string& folder) { sUserFolder = folder; }

void MarkSettingsDirty() { PortOverlaySettings::MarkDirty(); }

void SaveSettingsNow() { PortOverlaySettings::SaveSettings(); }

// The launcher's live edits come in Phase 5 (docs/PRIMEDGUN.md); the file is read at start-up.
void PollSettingsFile(bool) {}

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

bool QuickResume() { return PortOverlay::QuickResume(); }
void SetQuickResume(bool on) { PortOverlay::SetQuickResume(on); }
// The Metroid Prime port's draw tags (a debug overlay of draw names) have no counterpart here.
bool DrawTags() { return sDrawTags; }
void SetDrawTags(bool on) { sDrawTags = on; }

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
