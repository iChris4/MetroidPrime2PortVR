#include "MetroidPrime/CRezbitEffect.hpp"

#include "MetroidPrime/CArchitectureMessage.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CIOWin.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"

#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CVector2i.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#ifdef TARGET_PC
#include "vr/openxr_integration.h"
#endif

extern CIOWinManager* gpIOWinManager;

// Guessed local class and phase names, based on framebuffer preservation behavior.
class CRezbitEffectIOWin : public CIOWin {
public:
  enum EFramePhase {
    kFP_Idle = 0,
    kFP_RestoringFrame = 1,
    kFP_AwaitingPreservedFrame = 2,
    kFP_PreservingFrame = 3,
  };

  CRezbitEffectIOWin();

  // CIOWin
  ~CRezbitEffectIOWin() override;
  EMessageReturn OnMessage(const CArchitectureMessage& msg, CArchitectureQueue& queue) override;
  bool GetIsContinueDraw() const override;
  void Draw() const override;

  void RequestExit();
  void SetFrameCount(int count) { mFrameCount = count; }

private:
  int mFrameCount;
  EFramePhase mFramePhase;
  bool mSavedClearFramebuffer;
  bool mExitRequested;
#ifdef TARGET_PC
  bool mPortTicked;
#endif
};
CHECK_SIZEOF(CRezbitEffectIOWin, 0x20)

CRezbitEffect::~CRezbitEffect() {}

CElementGen* CRezbitEffect::CreateParticleEffect(CAssetId asset) {
  if (asset != kInvalidAssetId) {
    TCachedToken< CGenDescription > token(gpSimplePool->GetObj(SObjectTag('PART', asset)), true);
    return rs_new CElementGen(token, CElementGen::kMOT_Normal, CElementGen::kOSF_One);
  }
  return nullptr;
}

CRezbitEffect::CRezbitEffect(TUniqueId uid, const CEntityInfo& info,
                             const CRezbitEffectOptions& options)
: CActor(uid, rstl::string_l("Rezbit Effect"), info, 0, CTransform4f::Identity(),
         CModelData::CModelDataNull(), CMaterialList(), CActorParameters::None(), kInvalidUniqueId)
, mDuration(options.GetDuration())
, mInterferenceTimeLowerBound(options.GetInterferenceTimeLowerBound())
, mInterferenceEndTime(options.GetInterferenceEndTime())
, mElapsedTime(0.f)
, mInterferenceCooldown(0)
, mIOWin(rs_new CRezbitEffectIOWin())
, mParticleEffect(CreateParticleEffect(options.GetParticleEffect()))
, mOptions(options)
, mSound()
, mAwaitingRecovery(options.GetWaitForRecovery()) {
  if (mInterferenceTimeLowerBound > mInterferenceEndTime) {
    mInterferenceEndTime = mInterferenceTimeLowerBound + 0.5f;
  }
  if (mInterferenceEndTime > mDuration) {
    mDuration = mInterferenceEndTime;
  }

  if (!mParticleEffect.null()) {
    mParticleEffect->SetParticleEmission(false);
    mParticleEffect->SetGlobalScale(CVector3f(1.8f, 1.8f, 1.8f));
  }

  mSound = CSfxManager::SfxStart(mOptions.GetVirusSound(), CAudioSys::kMaxVolume, 0x40,
                                 CSfxManager::kAllAreas, true, true, CSfxManager::kMedPriority);
}

void CRezbitEffect::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CActor::AcceptScriptMsg(mgr, msg);
  if (msg.GetMessage() == kSM_AreaLoaded) {
    gpIOWinManager->AddIOWin(mIOWin, 9999, 99999);
  } else if (msg.GetMessage() == kSM_Delete) {
    static_cast< CRezbitEffectIOWin* >(mIOWin.GetPtr())->RequestExit();
  }
}

void CRezbitEffect::Think(float dt, CStateManager& mgr) {
  CActor::Think(dt, mgr);
  if (!GetActive()) {
    return;
  }

  mElapsedTime += dt;
  const CPlayer& player = *mgr.GetPlayer(0);
  if (!mParticleEffect.null()) {
    if (player.GetMorphballTransitionState() == CPlayer::kMS_Morphed) {
      mParticleEffect->SetParticleEmission(true);
      mParticleEffect->SetGlobalTranslation(
          player.GetTranslation() + CVector3f(0.f, 0.f, player.GetMorphBall()->GetBallRadius()));
      mParticleEffect->Update(dt);
    } else {
      mParticleEffect->SetParticleEmission(false);
    }
  }

  if (mAwaitingRecovery && player.GetRezbitState() != CPlayer::kRS_Infected) {
    FinishEffect();
  }
  if (mAwaitingRecovery && mElapsedTime >= mInterferenceEndTime) {
    mElapsedTime -= dt;
  }

  if (mElapsedTime > mDuration) {
    mgr.DeleteObjectRequest(GetUniqueId());
  } else if (GetCurrentAreaId() != mgr.GetNextAreaId()) {
    mgr.SetActorAreaId(*this, mgr.GetNextAreaId());
  }
}

void CRezbitEffect::PreRenderAllViewports(CStateManager& mgr) {}

void CRezbitEffect::PreRender(CStateManager& mgr) {
  const CCameraManager* cameraManager = mgr.GetCurrentRenderCameraManager();
  SetPreRenderClipped(true);
  if (cameraManager->IsInFPCamera()) {
    if (mInterferenceCooldown <= 0 && mElapsedTime < mInterferenceEndTime) {
      CRandom16 random(static_cast< uint >(100.f * CGraphics::GetSecondsMod900()));
      static_cast< CRezbitEffectIOWin* >(mIOWin.GetPtr())->SetFrameCount(random.Range(7, 30));
      mInterferenceCooldown = 2;
    } else {
      --mInterferenceCooldown;
    }
    mgr.RenderLastHUD(GetUniqueId());
  }
}

void CRezbitEffect::Render(const CStateManager& mgr) const {
  gpRender->DrawDarkWorldTransition(CColor(uchar(24), uchar(24), uchar(24), uchar(255)),
                                    CColor::Grey(), CColor::Black(), CColor::Black(),
                                    CVector2i(0, 0), CVector2i(0, 0), CVector2i(32, 32));
}

void CRezbitEffect::AddToRenderer(const CStateManager& mgr) const {
  CActor::AddToRenderer(mgr);
  if (!mParticleEffect.null()) {
    gpRender->AddParticleGen(*mParticleEffect);
  }
}

void CRezbitEffect::FinishEffect() {
  mAwaitingRecovery = false;
  CSfxManager::RemoveEmitter(mSound);
  mSound.Clear();
  CSfxManager::SfxStart(mOptions.GetRebootSound(), CAudioSys::kMaxVolume, 0x40,
                        CSfxManager::kAllAreas, true, false, CSfxManager::kMedPriority);
}

CRezbitEffectOptions::CRezbitEffectOptions(CAssetId particleEffect, ushort virusSound,
                                           ushort rebootSound, float duration,
                                           float interferenceTimeLowerBound,
                                           float interferenceEndTime, bool waitForRecovery)
: mParticleEffect(particleEffect)
, mVirusSound(virusSound)
, mRebootSound(rebootSound)
, mDuration(duration)
, mInterferenceTimeLowerBound(interferenceTimeLowerBound)
, mInterferenceEndTime(interferenceEndTime)
, mWaitForRecovery(waitForRecovery) {}

CRezbitEffectIOWin::CRezbitEffectIOWin()
: CIOWin(rstl::string_l("Rezbit Effect"))
, mFrameCount(0)
, mFramePhase(kFP_Idle)
, mSavedClearFramebuffer(false)
, mExitRequested(false)
#ifdef TARGET_PC
, mPortTicked(false)
#endif
{
}

void CRezbitEffectIOWin::Draw() const {}

bool CRezbitEffectIOWin::GetIsContinueDraw() const {
  return mFramePhase != kFP_PreservingFrame && mFramePhase != kFP_RestoringFrame;
}

void CRezbitEffectIOWin::RequestExit() {
  mExitRequested = true;
  mFrameCount = 0;
}

CIOWin::EMessageReturn CRezbitEffectIOWin::OnMessage(const CArchitectureMessage& msg,
                                                     CArchitectureQueue& queue) {
#ifdef TARGET_PC
  if (msg.GetType() == kAM_TimerTick) {
    mPortTicked = true;
  }
#endif
  if (msg.GetType() == kAM_FrameBegin) {
#ifdef TARGET_PC
    // PrimedGun Echoes: no held frames in the headset. A hold opens no frame, so the eyes stayed
    // on one stale image for 7 to 30 ticks out of every few while the virus was in (it lasts
    // until the reboot). The world keeps drawing there; the static, the corrupted text and the
    // sound still play. A hold in progress winds down through the phases below, which restore
    // the clear flag.
    if (PortVr::OpenXRIsRunning()) {
      mFrameCount = 0;
    }
    // Uncapped, a frame begins every loop and a held frame neither presents nor waits on vsync,
    // so counting frames ended the freeze in milliseconds. Count only frames a game tick ran in:
    // the retail 60 Hz frames.
    if (mFrameCount != 0 && mPortTicked) {
      --mFrameCount;
    }
    mPortTicked = false;
#else
    if (mFrameCount != 0) {
      --mFrameCount;
    }
#endif

    if (mFrameCount == 0 && mFramePhase == kFP_PreservingFrame) {
      mFramePhase = kFP_RestoringFrame;
      CGraphics::SetIsBeginSceneClearFb(mSavedClearFramebuffer);
    } else if (mFrameCount != 0 && mFramePhase == kFP_Idle) {
      mFramePhase = kFP_AwaitingPreservedFrame;
      mSavedClearFramebuffer = CGraphics::IsBeginSceneClearFb();
      CGraphics::SetIsBeginSceneClearFb(false);
    } else if (mFramePhase == kFP_AwaitingPreservedFrame) {
      mFramePhase = kFP_PreservingFrame;
    } else if (mFramePhase == kFP_RestoringFrame) {
      mFramePhase = kFP_Idle;
    }
#ifdef TARGET_PC
    // Nothing draws while the frame is preserved or restored; the GameCube's uncleared EFB showed
    // the last image again, but Aurora clears every frame, which flashed the screen. Hold it.
    CGraphics::PortSetHoldFrame(!GetIsContinueDraw());
#endif
  }

  if (mExitRequested && mFramePhase == kFP_Idle) {
    return kMR_RemoveIOWin;
  }
  return CIOWin::OnMessage(msg, queue);
}

#ifdef TARGET_PC
CRezbitEffectIOWin::~CRezbitEffectIOWin() { CGraphics::PortSetHoldFrame(false); }
#else
CRezbitEffectIOWin::~CRezbitEffectIOWin() {}
#endif
