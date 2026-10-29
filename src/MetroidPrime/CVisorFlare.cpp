#include "MetroidPrime/CVisorFlare.hpp"

#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "dolphin/os/OSCache.h"
#include "rstl/math.hpp"
#ifdef TARGET_PC
#include "MetroidPrime/PortEfbCopy.hpp"
#include "vr/vr_view.h"
#include <dolphin/gx/GXCpu2Efb.h>
#include <string.h>

namespace {
// The occlusion test below costs two EFB copies per flare, and each copy ends Aurora's render
// pass: a full store and reload of colour and depth, which on a tiled mobile GPU at native
// resolution made rooms with many flares (Command Center: 18) crawl. Instead, the first flare of a
// frame snapshots the depth once (flares never write depth, so it is the depth every flare's test
// quad would meet) and each flare compares its own depth against the latest snapshot on the CPU,
// counting the same samples the copy would have. A frame or two late, which the running average
// absorbs; without a fresh snapshot (MSAA, offscreen, split screen) the copies run as before.
// Generous: a mobile GPU's read-back can lag several frames, and every flare falling back to its
// copies is the very cost this avoids. Staleness only shows for a frame or two after a camera cut.
const uint kFlareDepthMaxAgeMs = 500;
int sFlareDepthSnapshotFrame = -1;
uint sFlareDepth[64 * 64];

bool FlareRatioFromDepth(float screenZ, int left, int top, int width, int height, bool largeTest,
                         float& ratio) {
  const CViewport& viewport = CGraphics::GetViewport();
  const GXRenderModeObj& mode = CGraphics::GetRenderMode();
  if (viewport.mLeft != 0 || viewport.mTop != 0 || viewport.mWidth != mode.fbWidth ||
      viewport.mHeight != mode.xfbHeight) {
    return false;
  }
  if (sFlareDepthSnapshotFrame != CGraphics::GetFrameCounter()) {
    sFlareDepthSnapshotFrame = CGraphics::GetFrameCounter();
    GXSnapshotDepth();
  }
  if (!GXPeekZRegion(left, top, width, height, sFlareDepth, kFlareDepthMaxAgeMs)) {
    return false;
  }
  // The camera's projection takes view depth to z/w in [-1, 1]; the GX one to [0, 1], then the
  // viewport's depth range, then z24. The test quad is drawn at this depth with LEqual.
  const float z01 = rstl::max_val(0.f, rstl::min_val(1.f, 0.5f * (screenZ + 1.f)));
  const float depth =
      CGraphics::GetDepthNear() + z01 * (CGraphics::GetDepthFar() - CGraphics::GetDepthNear());
  // A few z24 steps of slack: GPU and CPU round a flare sitting on a surface differently.
  const uint flareZ = uint(depth * 16777215.f + 0.5f);
  const uint limit = flareZ > 16 ? flareZ - 16 : 0;
  int visible = 0;
  if (!largeTest) {
    // 8x4 R8 copy: one sample per pixel.
    for (int i = 0; i < width * height; ++i) {
      if (limit <= sFlareDepth[i]) {
        ++visible;
      }
    }
    ratio = float(visible) / float(width * height);
    return true;
  }
  // 64x64 copied to a 32x32 box-filtered R8 and read as 8-texel blocks: each block covers 16x2
  // pixels and counts when any of them is lit.
  for (int by = 0; by < height; by += 2) {
    for (int bx = 0; bx < width; bx += 16) {
      bool lit = false;
      for (int y = by; y < by + 2 && !lit; ++y) {
        for (int x = bx; x < bx + 16; ++x) {
          if (limit <= sFlareDepth[y * width + x]) {
            lit = true;
            break;
          }
        }
      }
      if (lit) {
        ++visible;
      }
    }
  }
  ratio = float(visible) / float((width / 16) * (height / 2));
  return true;
}
} // namespace
#endif

CVisorFlare::CFlareDef::CFlareDef(const TToken< CTexture >& tex, float pos, float scale, uint color)
: mTex(tex), mPos(pos), mScale(scale), mColor(color) {
  mTex.Lock();
}

CVisorFlare::CVisorFlare(EBlendMode blendMode, bool distanceScaled, float fadeTime,
                         float angularFalloff, float rotationScale, uint darkVisorMode,
                         uint combatVisorMode, const rstl::vector< CFlareDef >& flares,
                         const bool smallOcclusionTest, const bool noOcclusionTest)
: mBlendMode(blendMode)
, mFlareDefs(flares)
, mAngularFalloff(angularFalloff)
, mRotationScale(rotationScale)
, mIntensity(0.f)
, mDarkVisorMode(darkVisorMode)
, mCombatVisorMode(combatVisorMode)
, mDistanceScaled(distanceScaled)
, mSmallOcclusionTest(smallOcclusionTest)
, mOutsideFrustum(true)
, mNoOcclusionTest(noOcclusionTest)
, mOcclusionAverage(smallOcclusionTest ? 2 : 10)
, mOcclusionWarmupFrames(4)
, mSavedFramebuffer(kTF_RGBA8, smallOcclusionTest ? short(8) : short(64),
                    smallOcclusionTest ? short(4) : short(64), 1)
, mOcclusionTexture(kTF_I8, smallOcclusionTest ? short(8) : short(32),
                    smallOcclusionTest ? short(4) : short(32), 1) {
#ifdef TARGET_PC
  // The occlusion copy only reaches this buffer through the port's read-back, a frame or two after
  // the first test; until then it holds whatever the host heap left, which read as "visible".
  // Start fully occluded instead.
  memset(mOcclusionTexture.Lock(), 0, mOcclusionTexture.GetWidth() * mOcclusionTexture.GetHeight());
  mOcclusionTexture.UnLock();
#endif
}

CVisorFlare::~CVisorFlare() {
  mSavedFramebuffer.ScheduleDeletion();
  mOcclusionTexture.ScheduleDeletion();
}

void CVisorFlare::Update(float dt, const CVector3f& pos, const CActor* actor, CStateManager& mgr) {
  const CPlayerState::EPlayerVisor visor = mgr.GetPlayerState(0)->GetCurrentVisor();
  if ((visor == CPlayerState::kPV_Combat ||
       (mDarkVisorMode != 1 && visor == CPlayerState::kPV_Dark)) &&
      mgr.GetPlayer(0)->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed) {
    const rstl::optional_object< float > average = mOcclusionAverage.GetAverage();
    const float occlusion = average ? *average : 1.f;
    const CGameCamera& camera = *mgr.GetCameraManager(0)->GetCurrentCamera(mgr, true);
#ifdef TARGET_PC
    // PortVr: the flare fades with its angle from the head's facing, not the body's.
    const CTransform4f portCameraXf =
        PortVr::VrImmersive()
            ? PortVr::VrHeadViewTransform(mgr.GetCameraManager(0)->GetCurrentCameraTransform(mgr, true))
            : camera.GetTransform();
    const CVector3f forward = portCameraXf.GetForward();
    const CVector3f direction = pos - portCameraXf.GetTranslation();
#else
    const CVector3f forward = camera.GetTransform().GetForward();
    const CVector3f direction = pos - camera.GetTransform().GetTranslation();
#endif
    mIntensity = 1.f - occlusion;
    const float dot = CVector3f::Dot(direction.AsNormalized(), forward);
    mIntensity *= rstl::max_val(0.f, 1.f - mAngularFalloff * 4.f * (1.f - dot));
  }
}

void CVisorFlare::UpdateFrustum(const CStateManager& mgr, const CVector3f& pos) {
  mOutsideFrustum = !mgr.GetFrustumPlanes().PointInFrustumPlanes(pos);
}

void CVisorFlare::Render(const CVector3f& pos, const CActor& actor, const CStateManager& mgr) {
  if (mgr.GetWorld()->GetAreaAlways(actor.GetCurrentAreaId()).GetOcclusionState() !=
      CGameArea::kOS_Visible) {
    return;
  }
  switch (mgr.GetPlayerState()->GetActiveVisor(mgr)) {
  case CPlayerState::kPV_Combat:
    if (mCombatVisorMode != 0) {
      return;
    }
    break;
  case CPlayerState::kPV_Dark:
    if (mDarkVisorMode == 1) {
      return;
    }
    break;
  default:
    return;
  }
  const int playerIndex = mgr.GetCurrentRenderPlayerIndex();
  CGX::SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
  if (mgr.GetCurrentRenderPlayer()->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed) {
    UpdateOcclusion(pos, mgr, playerIndex);
    RenderFlares(pos, mgr);
  }
}

void CVisorFlare::UpdateOcclusion(const CVector3f& pos, const CStateManager& mgr, int playerIndex) {
  if (mOutsideFrustum) {
    mOcclusionAverage.AddValue(1.f);
    return;
  }
  if (mNoOcclusionTest) {
    mOcclusionAverage.AddValue(0.f);
    return;
  }

  const bool largeTest = !mSmallOcclusionTest;
  const short width = mSmallOcclusionTest ? 8 : 64;
  const short height = mSmallOcclusionTest ? 4 : 64;
  const CViewport& viewport = CGraphics::GetViewport();
  const int vpTop = viewport.mTop;
  const int vpWidth = viewport.mWidth;
  const int vpHeight = viewport.mHeight;
  const int framebufferWidth = CGraphics::GetRenderMode().fbWidth;
  const int framebufferHeight = CGraphics::GetRenderMode().xfbHeight;
  CGraphics::DisableAllLights();
  const CGameCamera& camera = *mgr.GetCameraManager(playerIndex)->GetCurrentCamera(mgr, true);
  const CVector3f right = CGraphics::GetViewMatrix().GetRight();
  const CVector3f up = CGraphics::GetViewMatrix().GetUp();
  const CVector3f screenPos = camera.ConvertToScreenSpace(pos);
  camera.ConvertToWorldSpace(screenPos);
  const float screenX = 0.5f * screenPos.GetX() + 0.5f;
  const float screenY = 0.5f * screenPos.GetY() + 0.5f;
  const int left = (int(framebufferWidth * screenX) - width / 2) & ~1;
  const int top = (int(framebufferHeight * screenY) + height / 2) & ~1;
  const int viewLeft = (int(vpWidth * screenX) - width / 2) & ~1;
  const int viewTop = (int(vpHeight * screenY) + height / 2) & ~1;
  if (left < 0 || left + width > framebufferWidth || top - height < vpTop ||
      top > framebufferHeight - vpTop) {
    mOcclusionAverage.AddValue(1.f);
    return;
  }

#ifdef TARGET_PC
  float depthRatio;
  if (FlareRatioFromDepth(screenPos.GetZ(), left, framebufferHeight - top, width, height,
                          largeTest, depthRatio)) {
    const float depthOcclusion = 1.f - depthRatio;
    mOcclusionAverage.AddValue(0.5f * (depthOcclusion * depthOcclusion + depthOcclusion));
    return;
  }
#endif

  const bool videoFilter = CGraphics::GetUseVideoFilter();
  CGraphics::SetUseVideoFilter(false);
  gpRender->SetDepthReadWrite(true, false);
  const CVector3f corner = camera.ConvertToWorldSpace(
      CVector3f(-1.f + 2.f * (float(viewLeft) / vpWidth), -1.f + 2.f * (float(viewTop) / vpHeight),
                screenPos.GetZ()));
  const CVector3f opposite = camera.ConvertToWorldSpace(
      CVector3f(-1.f + 2.f * (float(viewLeft + width) / vpWidth),
                -1.f + 2.f * (float(viewTop + height) / vpHeight), screenPos.GetZ()));
  const CVector3f extent = opposite - corner;
  const float worldWidth = CVector3f::Dot(extent, right);
  const float worldHeight = CVector3f::Dot(extent, up);

  GXSetTexCopySrc(left, framebufferHeight - top, width, height);
  GXSetTexCopyDst(mSavedFramebuffer.GetWidth(), mSavedFramebuffer.GetHeight(), GX_TF_RGBA8, false);
  GXCopyTex(mSavedFramebuffer.Lock(), true);
  mSavedFramebuffer.UnLock();

  GXFogType fogType;
  float fogStart, fogEnd, fogNear, fogFar;
  GXColor fogColor;
  CGX::GetFog(&fogType, &fogStart, &fogEnd, &fogNear, &fogFar, &fogColor);
  CGX::SetFog(GX_FOG_NONE, 0.f, 1.f, 0.f, 1.f, fogColor);
  gpRender->SetModelMatrix(CTransform4f::Identity());
  CGraphics::SetBlendMode(::kBM_Blend, kBF_One, kBF_One, kLO_Clear);
  CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvPassthru);
  CGraphics::StreamBegin(kP_TriangleFan);
  CGraphics::StreamColor(CColor::White());
  CGraphics::StreamVertex(corner);
  CGraphics::StreamVertex(corner - worldHeight * up);
  CGraphics::StreamVertex(corner - worldHeight * up + worldWidth * right);
  CGraphics::StreamVertex(corner + worldWidth * right);
  CGraphics::StreamEnd();
  CGX::SetFog(fogType, fogStart, fogEnd, fogNear, fogFar, fogColor);

  GXSetTexCopySrc(left, framebufferHeight - top, width, height);
  if (!largeTest) {
    GXSetTexCopyDst(width, height, GX_CTF_R8, false);
  } else {
    GXSetTexCopyDst(width / 2, height / 2, GX_CTF_R8, true);
  }
#ifdef TARGET_PC
  // Aurora resolves the copy into a texture and never writes this buffer, which the test below
  // reads as data: without the read-back every flare counted as unoccluded and drew through walls.
  PortEfbCopy::Request(mOcclusionTexture.GetBitMapData(0), mOcclusionTexture.GetWidth(),
                       mOcclusionTexture.GetHeight(), 1);
#endif
  GXCopyTex(mOcclusionTexture.Lock(), false);
  mOcclusionTexture.UnLock();
  GXPixModeSync();
  CGraphics::SetUseVideoFilter(videoFilter);
  gpRender->SetDepthReadWrite(false, false);
  CGraphics::SetBlendMode(::kBM_Blend, kBF_One, kBF_Zero, kLO_Clear);
  CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvModulate);
  CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
  CGraphics::Render2D(mSavedFramebuffer, left, top - vpTop, width, -height, CColor::White());

  if (mOcclusionWarmupFrames == 0) {
    const void* pixels = mOcclusionTexture.Lock();
#ifdef TARGET_PC
    PortEfbCopy::Fill(const_cast< void* >(pixels), mOcclusionTexture.GetWidth(),
                      mOcclusionTexture.GetHeight(), 1);
#endif
    DCInvalidateRange(const_cast< void* >(pixels),
                      mOcclusionTexture.GetWidth() * mOcclusionTexture.GetHeight());
    int visibleSamples = 0;
    int sampleCount;
    if (!largeTest) {
      sampleCount = width * height;
    } else {
      sampleCount = (width * height) >> 2;
    }
    float ratio;
    if (!largeTest) {
      const uchar* sample = static_cast< const uchar* >(pixels);
      for (int i = 0; i < sampleCount; ++i) {
        if (sample[i] != 0) {
          ++visibleSamples;
        }
      }
      ratio = float(visibleSamples) / float(sampleCount);
    } else {
      const int blockCount = sampleCount >> 3;
      // The original tests each eight-byte block as a double against zero.
#ifdef TARGET_PC
      // As integers: on a little-endian host a block whose only set byte is the last one is -0.0.
      const unsigned long long* sample = static_cast< const unsigned long long* >(pixels);
      for (int i = 0; i < blockCount; ++i) {
        if (sample[i] != 0) {
#else
      const double* sample = static_cast< const double* >(pixels);
      for (int i = 0; i < blockCount; ++i) {
        if (sample[i] != 0.0) {
#endif
          ++visibleSamples;
        }
      }
      ratio = float(visibleSamples) / float(blockCount);
    }
    const float occlusion = 1.f - ratio;
    mOcclusionTexture.UnLock();
    mOcclusionAverage.AddValue(0.5f * (occlusion * occlusion + occlusion));
  } else {
    --mOcclusionWarmupFrames;
  }
}

void CVisorFlare::RenderFlares(const CVector3f& pos, const CStateManager& mgr) const {
  CGraphics::SetDepthWriteMode(false, kE_LEqual, false);
  const CGameCamera& camera = *mgr.GetCurrentRenderCameraManager()->GetCurrentCamera(mgr, true);
#ifdef TARGET_PC
  // PortVr: the flare's sprites run along the line through the head's view centre and face the
  // head, as the eyes see the light; the body's camera off the headset.
  const bool portVrHead = PortVr::VrImmersive() && mgr.GetCurrentRenderPlayerIndex() == 0;
  const CTransform4f portCameraXf =
      portVrHead ? PortVr::VrHeadViewTransform(
                       mgr.GetCurrentRenderCameraManager()->GetCurrentCameraTransform(mgr, true))
                 : camera.GetTransform();
  const CVector3f cameraPos = portCameraXf.GetTranslation();
  const CVector3f flarePos = pos;
  const CTransform4f viewMatrix = portVrHead
                                      ? PortVr::VrHeadViewTransform(CGraphics::GetViewMatrix())
                                      : CGraphics::GetViewMatrix();
#else
  const CVector3f cameraPos = camera.GetTransform().GetTranslation();
  const CVector3f flarePos = pos;
  const CTransform4f viewMatrix = CGraphics::GetViewMatrix();
#endif
  CVector3f reflectedPos = viewMatrix.GetInverse() * flarePos;
  reflectedPos = CVector3f(-reflectedPos.GetX(), reflectedPos.GetY(), -reflectedPos.GetZ());
  const CVector3f reflectedWorldPos = viewMatrix * reflectedPos;
#ifdef TARGET_PC
  const CVector3f cameraForward = portCameraXf.GetForward();
#else
  const CVector3f cameraForward = camera.GetTransform().GetForward();
#endif
  const CVector3f toFlare = flarePos - cameraPos;
  if (!close_enough(mIntensity, 0.f)) {
    float angle = 0.f;
    if (!close_enough(mRotationScale, 0.f)) {
      const CVector3f flareDir = toFlare.DropZ().AsNormalized();
      const CVector3f cameraDir = cameraForward.DropZ().AsNormalized();
      float relativeAngle = CMath::ArcCosineR(CVector3f::Dot(flareDir, cameraDir));
      if (CVector3f::Cross(flareDir, cameraDir).GetZ() < 0.f) {
        relativeAngle = -relativeAngle;
      }
      angle = mRotationScale * relativeAngle;
    }

    SetupRenderState(mgr);
    for (int i = 0; i < mFlareDefs.size(); ++i) {
      const CFlareDef& flare = mFlareDefs[i];
      const CVector3f origin = CVector3f::Lerp(flarePos, reflectedWorldPos, flare.GetPosition());
      const CTransform4f modelMatrix = CTransform4f::LookAt(origin, cameraPos);
      gpRender->SetModelMatrix(modelMatrix);
      float scale = 0.5f * mIntensity * flare.GetScale();
      if (mDistanceScaled) {
        const CVector3f distance = origin - cameraPos;
        if (distance.CanBeNormalized()) {
          scale *= distance.Magnitude();
        }
      }
      TLockedToken< CTexture > texture(flare.GetTexture());
      if (!flare.GetTexture().IsLoaded()) {
        continue;
      }
      texture->Load(GX_TEXMAP0, CTexture::kCM_Repeat);
      float sinScale;
      float cosScale;
      if (close_enough(angle, 0.f)) {
        sinScale = 0.f;
        cosScale = scale;
      } else {
        sinScale = scale * sine(CRelAngle::FromRadians(angle));
        cosScale = scale * cosine(CRelAngle::FromRadians(angle));
      }
      DrawStreamed(flare.GetColor(), sinScale, cosScale);
    }
    ResetRenderState(mgr);
  }
}

void CVisorFlare::DrawStreamed(const CColor& color, float sinScale, float cosScale) const {
  CGraphics::StreamBegin(kP_TriangleStrip);
  CGraphics::StreamColor(color.WithAlphaModulatedBy(mIntensity));
  CGraphics::StreamTexcoord(0.f, 1.f);
  CGraphics::StreamVertex(sinScale - cosScale, 0.f, cosScale + sinScale);
  CGraphics::StreamTexcoord(1.f, 1.f);
  CGraphics::StreamVertex(sinScale + cosScale, 0.f, cosScale - sinScale);
  CGraphics::StreamTexcoord(0.f, 0.f);
  CGraphics::StreamVertex(-(sinScale + cosScale), 0.f, -(cosScale - sinScale));
  CGraphics::StreamTexcoord(1.f, 0.f);
  CGraphics::StreamVertex(-sinScale + cosScale, 0.f, -cosScale - sinScale);
  CGraphics::StreamEnd();
}

void CVisorFlare::SetupRenderState(const CStateManager& mgr) const {
  if (mBlendMode == kBM_Additive ||
      (mgr.GetPlayerState()->GetActiveVisor(mgr) == CPlayerState::kPV_Dark &&
       mDarkVisorMode == 2)) {
    gpRender->SetBlendMode_AdditiveAlpha();
  } else {
    gpRender->SetBlendMode_AlphaBlended();
  }
  CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvModulate);
  CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
  CGraphics::SetCullMode(kCM_None);
}

void CVisorFlare::ResetRenderState(const CStateManager& mgr) const {
  CGraphics::SetCullMode(kCM_Front);
}
