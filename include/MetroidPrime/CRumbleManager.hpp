#ifndef _CRUMBLEMANAGER
#define _CRUMBLEMANAGER

#include "types.h"

#include "Kyoto/Input/CRumbleGenerator.hpp"

class CStateManager;
class CVector3f;
#ifdef TARGET_PC
class CPlayer;
#endif

class CRumbleManager {
public:
  CRumbleManager(int playerIndex, EIOPort port);
  ~CRumbleManager();

  short Rumble(CStateManager&, const CVector3f&, ERumbleFxId, float, ERumblePriority);
  short Rumble(CStateManager&, ERumbleFxId, float, ERumblePriority);

  void StopRumble(short id);
#ifdef TARGET_PC
  void HardStopAll() {
    mRumbleGenerator.HardStopAll();
    mPortCannonRumble.HardStopAll();
    mPortGrappleRumble.HardStopAll();
  }
  // Reconstructed accessors for the native pause-state gate.
  bool GetDisabled() const { return mRumbleGenerator.GetDisabled(); }
  void SetDisabled(bool disabled) {
    mRumbleGenerator.SetDisabled(disabled);
    mPortCannonRumble.SetDisabled(disabled);
    mPortGrappleRumble.SetDisabled(disabled);
  }
#else
  void HardStopAll() { mRumbleGenerator.HardStopAll(); }
  // Reconstructed accessors for the native pause-state gate.
  bool GetDisabled() const { return mRumbleGenerator.GetDisabled(); }
  void SetDisabled(bool disabled) { mRumbleGenerator.SetDisabled(disabled); }
#endif
  void Update(float dt);
#ifdef TARGET_PC
  // PortVr: while the headset controllers own port 0, the arm cannon's effects pulse on port 1 and
  // the grapple arm's on port 2, which vr_pad.cpp turns into the gun hand's and the off hand's
  // haptics (vr/vr_rumble.h); the body keeps port 0. Rumble picks the source from the effect;
  // PortRumble names it for an effect shared with another source.
  enum EPortRumbleSource { kPRS_Body, kPRS_ArmCannon, kPRS_GrappleArm };
  short PortRumble(CStateManager& mgr, ERumbleFxId fx, float gain, ERumblePriority priority,
                   EPortRumbleSource source);
  // PortVr: the weapons' recoil shakes the view, which the headset does not; with it on, player 0's
  // recoil is a pulse on the gun hand instead (true), and the caller skips its camera shaker.
  static bool PortVrRecoil(CStateManager& mgr, const CPlayer* player);
#endif

private:
  int mPlayerIndex;
  EIOPort mPort;
  CRumbleGenerator mRumbleGenerator;
#ifdef TARGET_PC
  CRumbleGenerator mPortCannonRumble;
  CRumbleGenerator mPortGrappleRumble;
#endif
};
CHECK_SIZEOF(CRumbleManager, 0x4c)

#endif // _CRUMBLEMANAGER
