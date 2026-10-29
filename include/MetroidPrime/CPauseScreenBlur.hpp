#ifndef _CPAUSESCREENBLUR
#define _CPAUSESCREENBLUR

#include "MetroidPrime/CInGameGuiManagerCommon.hpp"
#include "MetroidPrime/Cameras/CCameraBlurPass.hpp"

#include "Kyoto/TToken.hpp"

#include "math.h"

class CInGameGuiManager;
class CStateManager;
class CTexture;

class CPauseScreenBlur {
public:
  enum EState { kS_InGame, kS_MapScreen, kS_SaveGame, kS_HUDMessage, kS_Pause };

  CPauseScreenBlur();
  virtual ~CPauseScreenBlur();

  void OnNewInGameGuiState(EInGameGuiState state, const CStateManager& mgr,
                           const CInGameGuiManager& guiMgr);
  bool IsGameDraw() const { return mGameDraw; }
  void Update(float dt, const CStateManager& mgr, bool b);
  void Draw(const CStateManager& mgr);
  float GetBlurAmt() const;
  bool IsNotTransitioning() const { return mPrevState == mNextState; }
#ifdef TARGET_PC
  // PortVr: a message box, the save prompt, the pause menu, logbook or map is shown in the
  // headset's immersive view (PortVr::VrMessageBoxInView, VrGameMenuInView), set by
  // CInGameGuiManager::PortVrRefreshPausedScreens every tick. The world keeps being drawn under the
  // blur and the blur's framebuffer copy is retaken every frame, so each eye blurs its own live
  // view, instead of the one frozen copy the TV keeps showing.
  void PortVrSetLiveWorld(bool live);
  bool PortVrLiveWorld() const { return mPortVrLiveWorld; }
  // The visor and the HUD go under the blur: while the world is live, and on the frame that takes
  // the one copy kept once the live view ends mid-box.
  bool PortVrHudUnderBlur() const { return mPortVrLiveWorld || mPortVrCopyPending; }
#endif

private:
  TLockedToken< CTexture > mMapLightQuarter;
  EState mPrevState;
  EState mNextState;
  float mBlurAmt;
  CCameraBlurPass mCamBlur;
  bool mBlurring : 1;
  bool mGameDraw : 1;
#ifdef TARGET_PC
  // In the retail bitfield's spare bits: the class keeps its size (and the heap its layout).
  bool mPortVrLiveWorld : 1;
  bool mPortVrCopyPending : 1;
#endif

  void OnBlurComplete(bool b);
  void SetState(EState state, const CInGameGuiManager& guiMgr);
  float GetBlurAmtInline() const { return fabs(mBlurAmt); }
};
CHECK_SIZEOF(CPauseScreenBlur, 0x40)

#endif // _CPAUSESCREENBLUR
