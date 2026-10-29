#include "MetroidPrime/CPauseScreenBlur.hpp"

#include "MetroidPrime/CAutoMapper.hpp"
#include "MetroidPrime/CInGameGuiManager.hpp"
#include "MetroidPrime/Cameras/CCameraFilterPass.hpp"
#include "MetroidPrime/SFX/PickupsSpecial.h"
#include "MetroidPrime/SFX/UI.h"
#include "MetroidPrime/SFX/UI3.h"
#include "MetroidPrime/Tweaks/CTweakGui.hpp"
#include "MetroidPrime/Tweaks/CTweakGuiColors.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"

#include "float.h"
#include "rstl/math.hpp"
#ifdef TARGET_PC
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include <aurora/gfx.h>
#include <dolphin/gx.h>

#include <cmath>
#endif

CPauseScreenBlur::CPauseScreenBlur()
: mMapLightQuarter(gpSimplePool->GetObj("TXTR_MapLightQuarter"))
, mPrevState(kS_InGame)
, mNextState(kS_InGame)
, mBlurAmt(0.f)
, mBlurring(false)
#ifdef TARGET_PC
, mGameDraw(true)
, mPortVrLiveWorld(false)
, mPortVrCopyPending(false) {}
#else
, mGameDraw(true) {}
#endif

CPauseScreenBlur::~CPauseScreenBlur() {}

#ifdef TARGET_PC
namespace {
// PortVr: the pause blur's CRT scan lines in the headset: dark bands at fixed elevations on a sphere
// around the camera, routed AURORA_STEREO_ROUTE_SKY so each eye sees them with the head's rotation
// only. They stay put in the world (at infinity, without disparity) while the head looks around,
// where the TV's lines (CCameraFilterPass::DrawScanLines) are glued to the picture. Each band's edges
// are rings of flat sides (48: the middles sag under a tenth of a degree), the ring starting half a
// side round so a side's middle is straight ahead.
const float kPortVrScanPitchDegrees = 0.8f;
const float kPortVrScanReachDegrees = 88.f; // above and below the horizon
const int kPortVrScanRingSides = 48;
const float kPortVrScanRadius = 10.f; // game units; at infinity any radius looks the same

void PortVrDrawWorldScanLines(const CStateManager& mgr, const CColor& color) {
  const CGraphics::CProjectionState projection = CGraphics::GetProjectionState();
  const CTransform4f viewPoint = CGraphics::GetViewMatrix();
  const CTransform4f model = CGraphics::GetModelMatrix();
  // The mono picture (the desktop mirror) sees them through the game camera's field of view; each
  // eye puts its own frustum in its place.
  const CViewport& viewport = CGraphics::GetViewport();
  const float aspect = viewport.mHeight > 0 ? static_cast< float >(viewport.mWidth) /
                                                  static_cast< float >(viewport.mHeight)
                                            : 1.f;
  CGraphics::SetPerspective(mgr.GetCameraManager(0)->GetCurrentCamera(mgr, true)->GetFov(), aspect,
                            0.1f, 2.f * kPortVrScanRadius);
  CGraphics::SetViewPointMatrix(CTransform4f::Identity());
  gpRender->SetModelMatrix(CTransform4f::Identity());
  gpRender->SetBlendMode_ColorMultiply();
  gpRender->SetDepthReadWrite(false, false);
  CGraphics::SetCullMode(kCM_None);
  AuroraSetStereoDrawRoute(AURORA_STEREO_ROUTE_SKY);
  const float degrees = 3.14159265f / 180.f;
  const float sideRadians = 2.f * 3.14159265f / kPortVrScanRingSides;
  const int bands = static_cast< int >(2.f * kPortVrScanReachDegrees / kPortVrScanPitchDegrees);
  for (int band = 0; band < bands; ++band) {
    const float low = (-kPortVrScanReachDegrees + kPortVrScanPitchDegrees * band) * degrees;
    const float high = low + 0.5f * kPortVrScanPitchDegrees * degrees;
    const float r0 = kPortVrScanRadius * std::cos(low);
    const float z0 = kPortVrScanRadius * std::sin(low);
    const float r1 = kPortVrScanRadius * std::cos(high);
    const float z1 = kPortVrScanRadius * std::sin(high);
    gpRender->BeginTriangleStrip(2 * (kPortVrScanRingSides + 1));
    gpRender->PrimColor(color);
    for (int side = 0; side <= kPortVrScanRingSides; ++side) {
      const float a = sideRadians * (static_cast< float >(side) + 0.5f);
      const float c = std::cos(a);
      const float sn = std::sin(a);
      gpRender->PrimVertex(CVector3f(r0 * c, r0 * sn, z0));
      gpRender->PrimVertex(CVector3f(r1 * c, r1 * sn, z1));
    }
    gpRender->EndPrimitive();
  }
  AuroraSetStereoDrawRoute(AURORA_STEREO_ROUTE_WORLD);
  CGraphics::SetCullMode(kCM_Front);
  gpRender->SetBlendMode_AlphaBlended();
  gpRender->SetModelMatrix(model);
  CGraphics::SetViewPointMatrix(viewPoint);
  CGraphics::SetProjectionState(projection);
}
} // namespace

void CPauseScreenBlur::PortVrSetLiveWorld(bool live) {
  // Any paused screen's state: a message box, the save prompt, the pause menu and logbook, the map.
  // For the last three CInGameGuiManager::EnsureStates then skips the area texture dump, which
  // needs the world stopped.
  const bool pausedState = mPrevState != kS_InGame || mNextState != kS_InGame;
  const bool wasLive = mPortVrLiveWorld;
  mPortVrLiveWorld = live && pausedState;
  if (mPortVrLiveWorld) {
    // The world may have stopped while the box was on the virtual screen (the setting turned on,
    // or the headset came back, with the box up): draw it again from this frame.
    mGameDraw = true;
    mPortVrCopyPending = false;
  } else if (wasLive && pausedState && mGameDraw) {
    // Leaving the live view with the box still up (the setting turned off, the headset asleep):
    // take one copy after this frame's world, then stop the world as the TV does (Draw's latch).
    // The visor and the HUD are drawn for that copy, as the TV's copy holds them.
    mCamBlur.FreePersistentFbTexture();
    mBlurring = true;
    mPortVrCopyPending = true;
  }
}
#endif
void CPauseScreenBlur::OnNewInGameGuiState(EInGameGuiState state, const CStateManager& mgr,
                                           const CInGameGuiManager& guiMgr) {
  if (InGameGuiStates::IsGameplayState(state)) {
    SetState(kS_InGame, guiMgr);
    return;
  }

  switch (state) {
  case kIGGS_MapScreen:
    SetState(kS_MapScreen, guiMgr);
    break;
  case kIGGS_PauseSaveGame:
    SetState(kS_SaveGame, guiMgr);
    break;
  case kIGGS_PauseHUDMessage:
    SetState(kS_HUDMessage, guiMgr);
    break;
  case kIGGS_PauseGame:
  case kIGGS_PauseLogBook:
    SetState(kS_Pause, guiMgr);
    break;
  }
}

void CPauseScreenBlur::Update(float dt, const CStateManager& mgr, bool b) {
  if (mPrevState == mNextState) {
    return;
  }

  if (mBlurAmt < 0.f) {
    mBlurAmt = rstl::min_val(0.f, mBlurAmt + dt);
  } else if (mBlurAmt > 0.f) {
    mBlurAmt = rstl::min_val(1.f, 2.f * dt + mBlurAmt);
  }

  if (mBlurAmt == 0.f || mBlurAmt == 1.f) {
    OnBlurComplete(b);
  }

  if (mBlurAmt == 0.f && b) {
    mCamBlur.DisableBlur(0.f);
  } else {
    mCamBlur.SetBlur(CCameraBlurPass::kBT_HiBlur,
                     GetBlurAmtInline() * gpTweakGui->GetPauseBlurFactor(), 0.f, true);
    mBlurring = true;
  }
}

void CPauseScreenBlur::Draw(const CStateManager& mgr) {
#ifdef TARGET_PC
  if (mPortVrLiveWorld) {
    // PortVr: the world under the blur is live, so this frame's picture is copied again (each eye's
    // own: Aurora takes a stereo copy per eye) rather than the first copy kept.
    mCamBlur.FreePersistentFbTexture();
  }
#endif
  mCamBlur.Draw();
  CGraphics::DisableAllLights();
  CGraphics::SetAmbientColor(CColor(0xffffffff));
  const float t = fabs(mBlurAmt);
  const float ease = 2.f * t - t * t;
  if (mCamBlur.GetCurrType() != CCameraBlurPass::kBT_NoBlur) {
#ifdef TARGET_PC
    const CColor portTint =
        CColor::Lerp(CColor::White(), gpTweakGuiColors->GetPauseScreenBGModulateColor(), ease);
    const CColor portLines = CColor::Lerp(
        CColor::White(), CColor(uchar(0xc0), uchar(0xc0), uchar(0xc0), uchar(0xff)), ease);
    if (mPortVrLiveWorld) {
      // PortVr: in the headset the quarters' vignette (white at the centre, a quarter as bright at
      // the corners) would darken the edges of the lens view: the tint alone, as the picture's
      // centre gets it. The scan lines stay put in the world.
      CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Multiply,
                                    CCameraFilterPass::kFS_Fullscreen, portTint, nullptr, ease);
      PortVrDrawWorldScanLines(mgr, portLines);
    } else {
      CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Multiply,
                                    CCameraFilterPass::kFS_FullscreenQuarters, portTint,
                                    *mMapLightQuarter, ease);
      CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Multiply,
                                    CCameraFilterPass::kFS_ScanLinesEven, portLines, nullptr, ease);
    }
#else
    CCameraFilterPass::DrawFilter(
        CCameraFilterPass::kFT_Multiply, CCameraFilterPass::kFS_FullscreenQuarters,
        CColor::Lerp(CColor::White(), gpTweakGuiColors->GetPauseScreenBGModulateColor(), ease),
        *mMapLightQuarter, ease);
    CCameraFilterPass::DrawFilter(
        CCameraFilterPass::kFT_Multiply, CCameraFilterPass::kFS_ScanLinesEven,
        CColor::Lerp(CColor::White(), CColor(0xc0c0c0ff), ease),
        nullptr, ease);
#endif
  }

  if (mBlurring && mCamBlur.GetNoPersistentCopy()) {
    mBlurring = false;
#ifdef TARGET_PC
    // PortVr: the live world keeps being drawn (PortVrSetLiveWorld); a copy the live view's end
    // asked for has now been taken.
    if (!mPortVrLiveWorld) {
      mGameDraw = false;
    }
    mPortVrCopyPending = false;
#else
    mGameDraw = false;
#endif
  }
}

void CPauseScreenBlur::OnBlurComplete(bool b) {
  if (mNextState == kS_InGame && !b) {
    return;
  }
  mPrevState = mNextState;
  if (mPrevState == kS_InGame) {
    mGameDraw = true;
#ifdef TARGET_PC
    mPortVrCopyPending = false;
#endif
  }
}

void CPauseScreenBlur::SetState(EState state, const CInGameGuiManager& guiMgr) {
  switch (mPrevState) {
  case kS_InGame:
    if (state != kS_InGame) {
      CSfxManager::SetChannel(CSfxManager::kSC_PauseScreen);
      if (state == kS_HUDMessage) {
        CSfxManager::SfxStart(SFXui_x_override_00_oneshot, 0x7f, 0x40);
      } else if (state == kS_MapScreen) {
        const CAutoMapper* mapper = guiMgr.GetAutoMapper();
        if (mapper != nullptr && mapper->GetMapMode() == CAutoMapper::kMM_Teleport) {
          CSfxManager::SfxStart(SFXpik_x_map_00_oneshot, 0x7f, 0x40);
        }
        CSfxManager::SfxStart(SFXui_x_pause_00_oneshot, 0x7f, 0x40);
      }
      mBlurAmt = FLT_EPSILON;
    }
    break;
  case kS_MapScreen:
  case kS_SaveGame:
  case kS_HUDMessage:
  case kS_Pause:
    break;
  }

  switch (state) {
  case kS_InGame: {
    bool valid = true;
    if (mPrevState == kS_InGame && mNextState == kS_InGame) {
      valid = false;
    }
    if (valid) {
      CSfxManager::SetChannel(CSfxManager::kSC_Game);

      if (mPrevState == kS_HUDMessage) {
        CSfxManager::SfxStart(SFXui_x_override_01_oneshot, 0x7f, 0x40);
      } else if (mPrevState == kS_MapScreen) {
        CSfxManager::SfxStart(SFXui_x_pause_01_oneshot, 0x7f, 0x40);
      }

      mBlurAmt = -1.f;
    }
    break;
  }
  case kS_MapScreen:
  case kS_SaveGame:
  case kS_HUDMessage:
  case kS_Pause:
    break;
  }

  mNextState = state;
}

float CPauseScreenBlur::GetBlurAmt() const { return GetBlurAmtInline(); }
