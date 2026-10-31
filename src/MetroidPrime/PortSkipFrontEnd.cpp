// Port-only: the front-end skip. `MP2_SKIP_FRONTEND=1` boots straight into the memory card's save
// instead of playing the front end's menu, `MP2_SLOT=<0|1|2>` picks the slot (default 0), and
// `MP2_NEW_GAME=1` starts a new game when that slot (or the whole card) is empty.
//
// **Why the card has to be driven by hand.** The world tables the game needs at boot come from the
// *disc* - the port's `CMemoryCard` is built from the resource factory's MLVL list, which is why
// `[warp] world table: 12 worlds` prints before any card is touched - but a save's *game state*
// comes from the card, and only `CMemoryCardDriver` reads it: the file select panel has to ask for
// it (`CSaveGameScreen::StartGame`, src/MetroidPrime/CSaveGameScreen.cpp:391), which is what
// `CMain::StreamNewGameState` then turns into the running game state. So a skip that does not play
// the menu has to run the read itself and leave the state where that call expects it.
//
// **What the skip reproduces, and what it does not.** `TakeSlot` mirrors `CSaveGameScreen::StartGame`
// (minus the write transaction a *new* game starts: a test boots a save that exists, and an empty
// slot gives up instead of writing to the card), and `StartGame` mirrors the three steps of retail's
// single-player start from the front end (`StartGameFromFrontEnd`, src/MetroidPrime/Player/
// CGameState.cpp:467). The front end's *player data* - which controller joined, its options - is not
// reproduced, because the front end's own defaults are what a headless test would have chosen
// anyway; the selection is forced to controller 0, which is the pad the harness drives.
//
// **Failure is a fall-back, not a hang.** Any error (no card, no file, an empty slot, a read that
// never settles) logs `[skip] ...` and lets the front end play: a run that cannot skip is a slow
// run, never a stuck one.
//
// The flow hooks are `CPreFrontEnd::OnMessage` (pump) and `CMainFlow::SetGameState` (start), both
// upstream files, both under `#ifdef TARGET_PC`.

#include "MetroidPrime/PortSkipFrontEnd.hpp"

#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CMemoryCardDriver.hpp"
#include "MetroidPrime/CSaveGameScreen.hpp"
#include "MetroidPrime/Player/CGMSinglePlayer.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Player/CWorldState.hpp"

#include "Kyoto/CResFactory.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

#include "port_harness.h"
#include "port_savestate.h"
#include "port_warp.h"

#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>

// `main.cpp` defines it and no header declares it; `CRezbitEffect.cpp` declares it the same way.
extern CIOWinManager* gpIOWinManager;

namespace {

enum EPhase {
  kIdle,     // the flag has not been read yet
  kReading,  // the card is being read for the requested slot
  kReady,    // the slot's state is in gpGameState; the flow has not started the game yet
  kStarted,  // the game is running from the save
  kGivenUp,  // the skip failed (or is not armed); the front end plays as usual
};

bool sReadEnv = false;
bool sArmed = false;
bool sSlotGiven = false;
// MP2_NEW_GAME=1: an empty slot, or a card with no save file at all, starts a new game in that slot
// instead of giving up - what the file select's new file does, minus its write to the card.
bool sNewGame = false;
// The splash dismissal: every window it has removed, and how many calls since the last one found
// any. The chain is done once it has been quiet for kSplashQuietCalls calls after a removal.
int sSplashRemoved = 0;
int sSplashQuietCalls = 0;
int sSplashCalls = 0;
bool sSplashDone = false;
const int kSplashQuietCalls = 30;
// A chain that never shows up (a boot with the splash disabled) stops being looked for after this.
const int kSplashCallBudget = 6000;
int sSlot = 0;
EPhase sPhase = kIdle;
CMemoryCardDriver* sDriver = nullptr;
int sTicks = 0;

// A card that never settles (a damaged image, a probe that answers kCR_BUSY forever) must not hold
// the flow in the pre-front-end window: the menu path would have shown its own error screen, and a
// test only wants to know that the skip did not happen.
const int kTickBudget = 900;

void ReadEnvOnce() {
  if (sReadEnv) {
    return;
  }
  sReadEnv = true;
  const char* value = std::getenv("MP2_SKIP_FRONTEND");
  sArmed = value != nullptr && value[0] != '\0' && value[0] != '0';
  if (!sArmed) {
    sPhase = kGivenUp;
    return;
  }
  if (const char* slot = std::getenv("MP2_SLOT")) {
    // `atoi` would read "" and "abc" as 0, which is a *slot* and would silently override the
    // card's own choice below; parse it properly and treat anything that is not 0..2 as "not given".
    char* end = nullptr;
    const long value = std::strtol(slot, &end, 10);
    if (end == slot || *end != '\0' || value < 0 || value > 2) {
      fprintf(stderr, "[skip] MP2_SLOT='%s' is not a save slot; using the card's own\n", slot);
    } else {
      sSlot = static_cast< int >(value);
      sSlotGiven = true;
    }
  }
  const char* newGame = std::getenv("MP2_NEW_GAME");
  sNewGame = newGame != nullptr && newGame[0] != '\0' && newGame[0] != '0';
  fprintf(stderr, "[skip] MP2_SKIP_FRONTEND=%s: the front end is skipped, save slot %d (%.1f s)\n",
          value, sSlot, PortHarness::Seconds());
}

// The boot's splash chain (progressive-mode check, health warning, the logo screens) sits above
// CMainFlow in the IO win manager and blocks it until a screen times out or a button is pressed -
// and the health warning's timeout is 61 seconds. A test run that presses nothing, which is the
// point of the skip, would pay that on every run (measured: the front end's flow only reached
// `CPreFrontEnd` at 56.6 s without a button press and at 11.6 s with one). So while the skip is
// armed, dismiss the chain. One pass is not enough: a screen's successor is queued by its own
// `CSplashScreen::OnMessage`, so it appears after the pass that removed its predecessor. Every call
// therefore removes whatever "SplashScreen" windows are up, and the dismissal ends once the chain
// has been quiet for kSplashQuietCalls calls. The progressive-mode prompt is not answered, so
// `sProgressiveModePrompt` stays as it was and the prompt would come back on a later boot - with the
// port's progressive mode off either way (`platform/main.cpp` passes `false` to `CGraphicsSys`).
void DismissSplashWindows() {
  ReadEnvOnce();
  if (!sArmed || sSplashDone) {
    return;
  }
  if (gpIOWinManager == nullptr) {
    return; // not up yet: try again on the next iteration rather than giving up on the chain
  }
  if (++sSplashCalls > kSplashCallBudget) {
    sSplashDone = true;
    fprintf(stderr, "[skip] no boot splash window after %d iterations; no longer looking (%.1f s)\n",
            kSplashCallBudget, PortHarness::Seconds());
    return;
  }
  int removed = 0;
  for (; removed < 8; ++removed) {
    rstl::ncrc_ptr< CIOWin > splash = gpIOWinManager->FindIOWin(rstl::string_l("SplashScreen"));
    if (splash.IsNull()) {
      break;
    }
    gpIOWinManager->RemoveIOWin(splash);
  }
  if (removed > 0) {
    sSplashRemoved += removed;
    sSplashQuietCalls = 0;
    return;
  }
  if (sSplashRemoved > 0 && ++sSplashQuietCalls >= kSplashQuietCalls) {
    sSplashDone = true;
    fprintf(stderr, "[skip] dismissed %d boot splash window(s); the chain is gone at %.1f s\n",
            sSplashRemoved, PortHarness::Seconds());
  }
}

void GiveUp(const char* why) {
  fprintf(stderr, "[skip] %s; the front end plays as usual\n", why);
  sPhase = kGivenUp;
  // A driver this module created has the card mounted, and the front end's own file select will
  // construct one of its own: release this one, exactly as closing the menu does.
  delete sDriver;
  sDriver = nullptr;
}

// `CSaveGameScreen::ConstructCardDriver` looks these three up by name and dereferences the result,
// and the driver's constructor locks the textures: on the menu path they are already in a loaded
// pak because the front end is up. The skip runs before the front end loads, so check rather than
// crash (FrontEnd.pak is added at boot, so this only fires on a disc without it).
bool SaveScreenAssetsAreLoaded() {
  static const char* const kNames[] = {"TXTR_SaveBanner", "TXTR_SaveIcon0", "TXTR_SaveIcon1"};
  for (int i = 0; i < 3; ++i) {
    if (gpResourceFactory->GetResourceIdByName(kNames[i]) == nullptr) {
      fprintf(stderr, "[skip] '%s' is in no loaded pak\n", kNames[i]);
      return false;
    }
  }
  return true;
}

// One line naming every slot, so a log says what the card holds and a test that asked for the wrong
// slot does not have to guess: "which slot has the save" is the first thing to want to know.
void LogSlots() {
  fprintf(stderr, "[skip] card slots at %.1f s (the card points at %d):", PortHarness::Seconds(),
          sDriver->GetSaveIdx());
  for (int i = 0; i < 3; ++i) {
    const CGameState::GameFileStateInfo* info = sDriver->GetGameFileStateInfo(i);
    if (info == nullptr) {
      fprintf(stderr, " %d=empty", i);
    } else {
      fprintf(stderr, " %d=%08x %d%% %02d:%02d", i, info->mMlvlId, info->mItemPercent,
              int(info->mPlayTime) / 3600, int(info->mPlayTime) % 3600 / 60);
    }
  }
  fprintf(stderr, "\n");
}

// The slot's game state is on the card and in `sDriver` (ReadFinished): put it in `gpGameState`
// where `CMain::StreamNewGameState` reads it from.
void TakeSlot() {
  LogSlots();

  // The slot to boot: MP2_SLOT if it was given, otherwise the one the card's own header points at -
  // which is what the front end's file select opens on - and if that is empty, the first slot that
  // has a save, so a card left on a different slot still boots without the test asking twice.
  if (!sSlotGiven) {
    sSlot = sDriver->GetSaveIdx();
    if (sSlot < 0 || sSlot > 2 || sDriver->GetGameFileStateInfo(sSlot) == nullptr) {
      for (int i = 0; i < 3; ++i) {
        if (sDriver->GetGameFileStateInfo(i) != nullptr) {
          sSlot = i;
          break;
        }
      }
    }
  }

  const CGameState::GameFileStateInfo* info = sDriver->GetGameFileStateInfo(sSlot);
  if (info == nullptr && sNewGame) {
    fprintf(stderr, "[skip] slot %d is empty: starting a new game (MP2_NEW_GAME; nothing is written to "
                    "the card)\n", sSlot);
  } else if (info == nullptr) {
    char why[96];
    snprintf(why, sizeof why, "slot %d is empty, so there is no save to boot", sSlot);
    GiveUp(why); // releases the driver, so every failure path releases it exactly once
    return;
  }

  gpGameState->SystemOptions().SetSaveIdx(sSlot);
  sDriver->ExportPersistentOptions();
  sDriver->ExportGameOptions();
  sDriver->BuildNewFileSlot(sSlot);

  fprintf(stderr, "[skip] booting slot %d\n", sSlot);

  // The read is done with: release the driver, which unmounts the card exactly as the save screen's
  // own destructor does when the menu closes. A save later in the run then gets a driver of its own,
  // and no buffer of ours can be written back over the card.
  delete sDriver;
  sDriver = nullptr;
  sPhase = kReady;
}

} // namespace

namespace PortSkipFrontEnd {

void DismissSplash() { DismissSplashWindows(); }

bool Pump() {
  ReadEnvOnce();
  if (sPhase != kIdle && sPhase != kReading) {
    return true;
  }
  if (gpGameState == nullptr || gpMemoryCard == nullptr) {
    return false; // the caller only reaches this with a card, but never crash if it does not
  }

  if (sDriver == nullptr) {
    if (!SaveScreenAssetsAreLoaded()) {
      GiveUp("the save screen's textures are not in any loaded pak");
      return true;
    }
    sDriver = CSaveGameScreen::ConstructCardDriver(true);
    sDriver->StartCardProbe();
    sPhase = kReading;
  }
  if (++sTicks > kTickBudget) {
    GiveUp("the card read did not settle");
    return true;
  }

  sDriver->Update();
  switch (sDriver->GetState()) {
  case kS_CardCheckDone:
    // The check passed: open the save file and start its read, which is what CSaveGameScreen::Update
    // does with the same state (src/MetroidPrime/CSaveGameScreen.cpp:305).
    sDriver->IndexFiles();
    break;
  case kS_Ready:
    // The file is read, its signature checked and its slots parsed (ReadFinished). TakeSlot fills
    // gpGameState from the slot and releases the driver, so nothing below may touch sDriver again.
    TakeSlot();
    break;
  case kS_FileBad:
    // No save file on the card: with MP2_NEW_GAME the slots are all empty and the driver's system
    // data holds its defaults, which is exactly the state a new file starts from.
    if (sNewGame && sDriver->GetError() == CMemoryCardDriver::kE_FileMissing) {
      TakeSlot();
    } else {
      char why[96];
      snprintf(why, sizeof why, "the card's save file is unusable (error %d)", int(sDriver->GetError()));
      GiveUp(why);
    }
    break;
  case kS_CardProbe:
  case kS_CardMount:
  case kS_CardCheck:
  case kS_FileRead:
  case kS_FileDeleteBad:
    break; // still busy
  default: {
    char why[128];
    snprintf(why, sizeof why, "the card read stopped at state %d with error %d",
             int(sDriver->GetState()), int(sDriver->GetError()));
    GiveUp(why);
    break;
  }
  }

  return sPhase == kReady || sPhase == kGivenUp || sPhase == kStarted;
}

bool StartGame() {
  if (sPhase != kReady) {
    return false;
  }
  sPhase = kStarted;

  const int slot = gpGameState->SystemOptions().GetSaveIdx();

  // Retail's single-player start from the front end, without the front end: the state the card was
  // read into becomes the running state, that slot's options come with it, and the mode is single
  // player. `kRM_Default` is what the first front-end state runs under anyway (the loader reads it
  // to decide whether to show the world's name), and it is set here so the skip does not depend on
  // where in the flow it was reached from.
  const CGameState::SPreviousGameResults results = gpGameState->PreviousGameResults();
  gpMain->StreamNewGameState(false);
  gpGameState->LoadCompressedGameOptions(slot);
  gpGameState->GameOptions().EnsureOptions();
  gpGameState->SetGameMode(rs_new CGMSinglePlayer());
  gpGameState->PreviousGameResults() = results;
  ConfigureGameModeLayers();
  gpGameState->WriteBackupBuf();

  // The harness drives pad 0 (see platform/port_events.cpp), so the player has to be on it: the
  // front end would have assigned the controller that joined, and the save's own value is from
  // whenever it was written.
  const uint selection = gpGameState->PlayerState(0)->GetPlayerSelection();
  if (selection != 0) {
    fprintf(stderr, "[skip] player 1 was on controller %d in the save; moving it to 0 for the run\n",
            int(selection));
    gpGameState->PlayerState(0)->FUN_80085c18(0);
  }

  gpMain->SetRestartMode(CMain::kRM_Default);
  // MP2_WORLD="<world>,<area>": start in the destination rather than the save's world.
  if (!PortWarp::DirectBoot()) {
    // A pending quick resume names the room to start in: the card's own area (a new game's Landing
    // Site, with its intro) is never loaded. The resume's load still runs over it once the game is
    // up, for the position, the room state and the cinematic.
    uint32_t resumeWorld = 0;
    uint32_t resumeArea = 0;
    if (PortSaveState::ResumeBootArea(resumeWorld, resumeArea)) {
      gpMain->EnsureWorldPakReady(resumeWorld);
      gpGameState->SetCurrentWorldId(resumeWorld);
      gpGameState->CurrentWorldState().SetDesiredAreaAssetId(resumeArea);
      fprintf(stderr, "[skip] quick resume: starting in world %08x area %08x\n", resumeWorld,
              resumeArea);
    }
  }
  fprintf(stderr, "[skip] the save is the running state at %.1f s: world %08x, desired area %08x\n",
          PortHarness::Seconds(), gpGameState->CurrentWorldAssetId(),
          gpGameState->CurrentWorldState().GetDesiredAreaAssetId());
  return true;
}

bool Started() { return sPhase == kStarted; }

void ArmFromResume(int fileIdx) {
  sReadEnv = true;
  sArmed = true;
  sSlot = fileIdx;
  sSlotGiven = true;
  sPhase = kIdle;
  sSplashDone = false;
  sSplashCalls = 0;
  sSplashRemoved = 0;
  sSplashQuietCalls = 0;
}

bool Armed() {
  ReadEnvOnce();
  return sArmed;
}

} // namespace PortSkipFrontEnd
