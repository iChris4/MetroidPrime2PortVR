#include "MetroidPrime/CRumbleManager.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"

#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/RumbleFxTable.hpp"

#ifdef TARGET_PC
#include "vr/vr_pad.h"
#include "vr/vr_view.h"

namespace {
// The port's generator rides in the handle's bits 4-5, which CRumbleVoice leaves unused
// ((owner << 8) | channel 0-3), so StopRumble stops the effect where it plays.
const int kPortRumbleHandleShift = 4;
const short kPortRumbleHandleMask = 3 << kPortRumbleHandleShift;

CRumbleManager::EPortRumbleSource PortRumbleSourceOf(ERumbleFxId fx) {
  switch (fx) {
  case kRFX_PlayerGunCharge:
  case kRFX_PlayerMissileFire:
    return CRumbleManager::kPRS_ArmCannon;
  case kRFX_PlayerGrappleFire:
  case kRFX_PlayerGrappleSwoosh:
    return CRumbleManager::kPRS_GrappleArm;
  default:
    return CRumbleManager::kPRS_Body;
  }
}
} // namespace

CRumbleManager::CRumbleManager(int playerIndex, EIOPort port)
: mPlayerIndex(playerIndex)
, mPort(port)
, mRumbleGenerator(port)
, mPortCannonRumble(kIOP_Player2)
, mPortGrappleRumble(kIOP_Player3) {}

CRumbleManager::~CRumbleManager() { HardStopAll(); }

short CRumbleManager::Rumble(CStateManager& mgr, ERumbleFxId fx, float gain,
                             ERumblePriority priority) {
  return PortRumble(mgr, fx, gain, priority, PortRumbleSourceOf(fx));
}

short CRumbleManager::PortRumble(CStateManager& mgr, ERumbleFxId fx, float gain,
                                 ERumblePriority priority, EPortRumbleSource source) {
  CGameOptions& options = gpGameState->GameOptions();
  if (mgr.IsMultiplayer()) {
    if (options.GetIsPlayerRumbleEnabled(mPlayerIndex)) {
      return mRumbleGenerator.Rumble(skRumbleFxTable[fx], gain, priority);
    }
    return -1;
  }
  if (!options.GetIsRumbleEnabled()) {
    return -1;
  }
  if (mPlayerIndex == 0 && source != kPRS_Body && PortVr::VrPadOwnsPort0()) {
    CRumbleGenerator& hand = source == kPRS_ArmCannon ? mPortCannonRumble : mPortGrappleRumble;
    const short handle = hand.Rumble(skRumbleFxTable[fx], gain, priority);
    if (handle == -1) {
      return -1;
    }
    const short slot = source == kPRS_ArmCannon ? 1 : 2;
    return short(handle | (slot << kPortRumbleHandleShift));
  }
  return mRumbleGenerator.Rumble(skRumbleFxTable[fx], gain, priority);
}

bool CRumbleManager::PortVrRecoil(CStateManager& mgr, const CPlayer* player) {
  if (player == nullptr || mgr.IsMultiplayer() || player != mgr.GetPlayer(0) ||
      !PortVr::VrImmersive()) {
    return false;
  }
  mgr.RumbleManager(0)->PortRumble(mgr, kRFX_CameraShake, 1.f, kRP_Two, kPRS_ArmCannon);
  return true;
}
#else
CRumbleManager::CRumbleManager(int playerIndex, EIOPort port)
: mPlayerIndex(playerIndex), mPort(port), mRumbleGenerator(port) {}

CRumbleManager::~CRumbleManager() { mRumbleGenerator.HardStopAll(); }

short CRumbleManager::Rumble(CStateManager& mgr, ERumbleFxId fx, float gain,
                             ERumblePriority priority) {
  CGameOptions& options = gpGameState->GameOptions();
  if (mgr.IsMultiplayer()) {
    if (options.GetIsPlayerRumbleEnabled(mPlayerIndex)) {
      return mRumbleGenerator.Rumble(skRumbleFxTable[fx], gain, priority);
    }
    return -1;
  }
  if (options.GetIsRumbleEnabled()) {
    return mRumbleGenerator.Rumble(skRumbleFxTable[fx], gain, priority);
  }
  return -1;
}
#endif

short CRumbleManager::Rumble(CStateManager& mgr, const CVector3f& pos, ERumbleFxId fx, float dist,
                             ERumblePriority priority) {
  if (!close_enough(dist, 0.f)) {
    CVector3f delta = mgr.GetPlayer(mPlayerIndex)->GetTranslation() - pos;
    if (delta.MagSquared() < dist * dist) {
      return Rumble(mgr, fx, 1.f - delta.Magnitude() / dist, priority);
    }
  }
  return -1;
}

void CRumbleManager::StopRumble(short id) {
  if (id == -1) {
    return;
  }
#ifdef TARGET_PC
  const short slot = short((id & kPortRumbleHandleMask) >> kPortRumbleHandleShift);
  const short voice = short(id & ~kPortRumbleHandleMask);
  if (slot == 1) {
    mPortCannonRumble.Stop(voice);
  } else if (slot == 2) {
    mPortGrappleRumble.Stop(voice);
  } else {
    mRumbleGenerator.Stop(voice);
  }
#else
  mRumbleGenerator.Stop(id);
#endif
}

#ifdef TARGET_PC
void CRumbleManager::Update(float dt) {
  mRumbleGenerator.Update(dt);
  mPortCannonRumble.Update(dt);
  mPortGrappleRumble.Update(dt);
}
#else
void CRumbleManager::Update(float dt) { mRumbleGenerator.Update(dt); }
#endif
