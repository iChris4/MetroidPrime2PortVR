// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the VR layer needs from the port it runs in, so platform/vr does not
// depend on one port's debug UI. PrimedGun Echoes implements it in
// platform/vr/vr_host_echoes.cpp; the Metroid Prime VR fork's files still call
// its PortDebug directly (the same functions, by their old names).

#pragma once

#include <cstdint>
#include <string>

class CStateManager;

namespace PortVrHost {

// The live game, published by CStateManager::Update and cleared by its
// destructor; null outside gameplay (front end, between worlds).
CStateManager* StateManager() noexcept;
// The presentation policy's game mode for the frame about to be drawn
// (PrimeVRPolicyPublishGameMode), from the game's state and whether the last
// presented frame drew the world. Called by the game loop before BeginScene.
void PublishGameMode(bool lastFrameDrewWorld, uint64_t framesDrawn) noexcept;
void SetStateManager(CStateManager* mgr) noexcept;
void ForgetStateManager(const CStateManager* mgr) noexcept;
// Player 0's in-game GUI dumped its area textures for a paused screen (CInGameGuiManager::Update):
// a menu brought into the headset's view then waits on the virtual screen until they are back.
void NoteAreaTexturesDumped(bool dumped) noexcept;

// The user folder (settings, cards, the cannon texture library); set at boot.
const std::string& UserFolder() noexcept;
void SetUserFolder(const std::string& folder);

// The settings file: a VR setting changed (written with the others), written now,
// and read again for changes made outside the game (the launcher).
void MarkSettingsDirty();
void SaveSettingsNow();
void PollSettingsFile(bool force);

// The startup shader compile while it runs (as the desktop toast shows it).
bool ShaderCompilationProgress(uint32_t& done, uint32_t& total);

// The boot's quick resume question while the game waits on its answer: the desktop prompt is an
// ImGui window the headset never shows. The pad answers it, A yes and B no.
struct ResumeQuestion {
  std::string room;
  std::string saved;         // local time, "2026-10-29 17:40"; empty if unknown
  bool afterCrash = false;   // the last run did not quit cleanly
  bool crashedAgain = false; // ... right after resuming this very snapshot
};
bool ResumeQuestionShown(ResumeQuestion& question);

// The VR menu's DEBUG tab.
void CheatFullHealth();
void CheatGrantEverything();
bool Invulnerable();
void SetInvulnerable(bool on);
bool AiAudioEnabled();
void SetAiAudioEnabled(bool on);
bool MusyxAudioEnabled();
void SetMusyxAudioEnabled(bool on);
bool LogFile();
void SetLogFile(bool on);
void StartLogFile();
bool QuickResume();
void SetQuickResume(bool on);
bool DrawTags();
void SetDrawTags(bool on);

// The save states (the F1 overlay's page).
struct StateSlotInfo {
  bool exists = false;
  int64_t savedAt = 0; // Unix seconds
};
int SelectedStateSlot();
void SetSelectedStateSlot(int slot);
StateSlotInfo StateSlot(int slot);
// False when the request was refused (LastStateMessage says why).
bool RequestLoadState(int slot);
bool RequestSaveState(int slot);
std::string LastStateMessage();

// The user texture pack: its folder, and a reload once files changed there.
std::string UserTextureRoot();
void RequestUserTexturePackReload();

} // namespace PortVrHost
