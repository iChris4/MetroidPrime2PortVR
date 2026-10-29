#include "GuiSys/CGuiCamera.hpp"
#include "GuiSys/CGuiFrame.hpp"
#include "GuiSys/CGuiWidget.hpp"
#include "GuiSys/CGuiWidgetDrawParms.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CVector3f.hpp"

#include "Kyoto/Streams/CInputStream.hpp"
#ifdef TARGET_PC
#include <math.h>
#include "port_overlay.h"
#endif

const char* const eCamTypePerspective = "eCamTypePerspective";
const char* const eCamTypeOrthogonal = "eCamTypeOrthogonal";

CGuiWidget* CGuiCamera::Create(CGuiFrame* frame, CInputStream& in, CSimplePool* sp, uint version) {
  CGuiWidgetParms parms = ReadWidgetHeader(frame, in);
  EProjection proj = static_cast< EProjection >(in.ReadInt32());
  CGuiCamera* camera = nullptr;
  if (proj == kProjection_Perspective) {
    const float fov = in.ReadFloat();
    const float aspect = in.ReadFloat();
    const float znear = in.ReadFloat();
    const float zfar = in.ReadFloat();
    camera = rs_new CGuiCamera(parms, fov, aspect, znear, zfar);
  } else if (proj == kProjection_Orthographic) {
    const float left = in.ReadFloat();
    const float right = in.ReadFloat();
    const float top = in.ReadFloat();
    const float bottom = in.ReadFloat();
    const float znear = in.ReadFloat();
    const float zfar = in.ReadFloat();
    camera = rs_new CGuiCamera(parms, left, right, top, bottom, znear, zfar);
  }

  frame->SetFrameCamera(camera);
  camera->ParseBaseInfo(frame, in, parms, version);
  return camera;
}

CGuiCamera::CGuiCamera(const CGuiWidgetParms& parms, float fov, float aspect, float znear,
                       float zfar)
: CGuiWidget(parms) {
  mProjection = kProjection_Perspective;
  CVector3f(1.f, 0.f, 0.f).Normalize();
  mCameraParms.mPerspective.mFov = fov;
  mCameraParms.mPerspective.mAspect = aspect;
  mCameraParms.mPerspective.mNear = znear;
  mCameraParms.mPerspective.mFar = zfar;
}

CGuiCamera::CGuiCamera(const CGuiWidgetParms& parms, float left, float right, float top,
                       float bottom, float znear, float zfar)
: CGuiWidget(parms) {
  mProjection = kProjection_Orthographic;
  mCameraParms.mOrthographic.mLeft = left;
  mCameraParms.mOrthographic.mRight = right;
  mCameraParms.mOrthographic.mTop = top;
  mCameraParms.mOrthographic.mBottom = bottom;
  mCameraParms.mOrthographic.mNear = znear;
  mCameraParms.mOrthographic.mFar = zfar;
}

void CGuiCamera::Draw(const CGuiWidgetDrawParms& parms) const {
#ifdef TARGET_PC
  // Port: HUD scale (`MP2_HUD_SCALE`, F1 > HUD). Only CSamusHud::Draw sets the flag, so the pause
  // screen, the inventory, the helmet and memo screens keep the size the game authored.
  const int portHudScale = PortOverlay::HudScale();
  const bool portHudScaled =
      PortOverlay::HudScalingActive() && portHudScale != PortOverlay::kHudScaleMax;
  // Scaling the projection about its centre draws the HUD smaller around the middle of the screen
  // without moving the camera.
  const float portHudInvScale = portHudScaled ? 100.f / static_cast< float >(portHudScale) : 1.f;
  // Port: the aspect setting (PortOverlay::ApplyAspect). The frames are authored for the game's own
  // 640x448; on a wider framebuffer the projection is widened by the same factor, as the Prime 1
  // port's aspect-matched GUI cameras are, so the GUI keeps its proportions instead of stretching.
  const float portWiden = PortOverlay::AspectWiden();
#endif
  if (mProjection == kProjection_Perspective) {
#ifdef TARGET_PC
    // A wider field of view shrinks the content about the view axis, the perspective counterpart of
    // the orthographic branch below.
    float fov = mCameraParms.mPerspective.mFov;
    if (portHudScaled) {
      const float halfTan = tanf(0.5f * fov * 3.14159265f / 180.f) * portHudInvScale;
      fov = 2.f * atanf(halfTan) * 180.f / 3.14159265f;
    }
    float aspect = mCameraParms.mPerspective.mAspect * portWiden;
    if (portWiden < 1.f) {
      // Below the authored aspect the GUI fits the width and gains height rather than losing its
      // sides (the Prime 1 port's fit-width cameras): same horizontal extent, taller view.
      const float halfTan = tanf(0.5f * fov * 3.14159265f / 180.f) / portWiden;
      fov = 2.f * atanf(halfTan) * 180.f / 3.14159265f;
    }
    mDrawTanY = tanf(0.5f * fov * 3.14159265f / 180.f);
    mDrawTanX = mDrawTanY * aspect;
    CGraphics::SetPerspective(fov, aspect, mCameraParms.mPerspective.mNear,
                              mCameraParms.mPerspective.mFar);
#else
    CGraphics::SetPerspective(mCameraParms.mPerspective.mFov, mCameraParms.mPerspective.mAspect,
                              mCameraParms.mPerspective.mNear, mCameraParms.mPerspective.mFar);
#endif
  } else {
#ifdef TARGET_PC
    mDrawTanX = 0.f;
    mDrawTanY = 0.f;
    if (portHudScaled || portWiden != 1.f) {
      const float left = mCameraParms.mOrthographic.mLeft;
      const float right = mCameraParms.mOrthographic.mRight;
      const float top = mCameraParms.mOrthographic.mTop;
      const float bottom = mCameraParms.mOrthographic.mBottom;
      const float midX = 0.5f * (left + right);
      const float midY = 0.5f * (top + bottom);
      // Below the authored aspect: fit the width, extend the height (see the perspective branch).
      const float fitWidth = portWiden < 1.f ? 1.f : portWiden;
      const float fitHeight = portWiden < 1.f ? 1.f / portWiden : 1.f;
      const float scaleX = portHudInvScale * fitWidth;
      const float scaleY = portHudInvScale * fitHeight;
      CGraphics::SetOrtho(midX + (left - midX) * scaleX, midX + (right - midX) * scaleX,
                          midY + (top - midY) * scaleY, midY + (bottom - midY) * scaleY,
                          mCameraParms.mOrthographic.mNear, mCameraParms.mOrthographic.mFar);
    } else {
      CGraphics::SetOrtho(mCameraParms.mOrthographic.mLeft, mCameraParms.mOrthographic.mRight,
                          mCameraParms.mOrthographic.mTop, mCameraParms.mOrthographic.mBottom,
                          mCameraParms.mOrthographic.mNear, mCameraParms.mOrthographic.mFar);
    }
#else
    CGraphics::SetOrtho(mCameraParms.mOrthographic.mLeft, mCameraParms.mOrthographic.mRight,
                        mCameraParms.mOrthographic.mTop, mCameraParms.mOrthographic.mBottom,
                        mCameraParms.mOrthographic.mNear, mCameraParms.mOrthographic.mFar);
#endif
  }

  CGraphics::SetViewPointMatrix(CTransform4f::Translate(parms.GetCameraOffset()) *
                                GetWorldTransform());
#ifdef TARGET_PC
  // Port: the projection above is widened by portWiden; the widgets of a spreadable camera (the
  // in-game HUD) then move out by the same factor so they reach the wide view's edges, each keeping
  // its shape and anchoring (CGuiFrame::Draw). 1 = no spread.
  mSpread = mSpreadable && PortOverlay::HudWide() && portWiden > 1.f
                  ? (portWiden < PortOverlay::HudSpreadCap() ? portWiden : PortOverlay::HudSpreadCap())
                  : 1.f;
  mSpreadView = CTransform4f::Translate(parms.GetCameraOffset()) * GetWorldTransform();
  mSpreadY = mSpreadable && PortOverlay::HudWide() && portWiden < 1.f ? 1.f / portWiden : 1.f;
  if (mProjection == kProjection_Perspective) {
    mSpreadCenterX = 0.f;
    mSpreadCenterZ = 0.f;
    mSpreadHalfHeight = tanf(0.5f * mCameraParms.mPerspective.mFov * 3.14159265f / 180.f);
    mSpreadHalfWidth = tanf(0.5f * mCameraParms.mPerspective.mFov * 3.14159265f / 180.f) *
                       mCameraParms.mPerspective.mAspect;
  } else {
    mSpreadCenterZ = 0.5f * (mCameraParms.mOrthographic.mTop + mCameraParms.mOrthographic.mBottom);
    mSpreadHalfHeight =
        0.5f * (mCameraParms.mOrthographic.mTop - mCameraParms.mOrthographic.mBottom);
    mSpreadCenterX = 0.5f * (mCameraParms.mOrthographic.mLeft + mCameraParms.mOrthographic.mRight);
    mSpreadHalfWidth =
        0.5f * (mCameraParms.mOrthographic.mRight - mCameraParms.mOrthographic.mLeft);
  }
  mSpreadScreenHalfWidth = mSpreadHalfWidth * (portWiden > 1.f ? portWiden : 1.f) * portHudInvScale;
#endif
  CGuiWidget::Draw(parms);
}

#ifdef TARGET_PC
// The curve band ends before the housings and side struts (about 0.62), so they slide whole with the
// compact clusters they hold. It starts past the visor frame's glass U brackets beside the energy bar
// (their outer edge is at about 0.54), so they keep their authored width: the band is only the
// frame's straight ledge running to the side struts, which stretches without showing (a band from
// 0.46 smeared the brackets into wide glass bars at 20:9).
const float CGuiCamera::kSpreadSliceInner = 0.46f;
const float CGuiCamera::kSpreadSliceOuter = 0.585f;
const float CGuiCamera::kSpreadCurveInner = 0.55f;
const float CGuiCamera::kSpreadCurveOuter = 0.62f;

float CGuiCamera::GetSpreadSliceOffset(float t, bool curved) const {
  const float inner = (curved ? kSpreadCurveInner : kSpreadSliceInner) * mSpreadHalfWidth;
  const float outer = (curved ? kSpreadCurveOuter : kSpreadSliceOuter) * mSpreadHalfWidth;
  const float a = t < 0.f ? -t : t;
  float along = a <= inner ? 0.f : a >= outer ? 1.f : (a - inner) / (outer - inner);
  if (curved) {
    // Smoothstep: no change of slope where the stretch starts and ends.
    along = along * along * (3.f - 2.f * along);
  }
  const float moved = (mSpread - 1.f) * mSpreadHalfWidth * along;
  return t < 0.f ? -moved : moved;
}

void CGuiCamera::SetHudO2WTransform(const CTransform4f& xf, const CVector3f& idlePos) {
  SetO2WTransform(xf);
  SetSpreadLag(xf.GetQuickInverse() * CTransform4f::Translate(idlePos));
}

float CGuiCamera::GetLaggedTangent(float tangent, float depth) const {
  if (mProjection != kProjection_Perspective) {
    return tangent;
  }
  const CVector3f view = mSpreadLag * CVector3f(tangent * depth, depth, 0.f);
  return view.GetY() > 0.f ? view.GetX() / view.GetY() : tangent;
}

CTransform4f CGuiCamera::GetSpreadTransform(const CVector3f& worldAnchor, float weight) const {
  if (mSpread == 1.f && mSpreadY != 1.f && weight != 0.f) {
    // The vertical spread: each compact widget keeps its shape and moves to where the image remap
    // puts its anchor (screen-up scaled by mSpreadY about the centre).
    const CTransform4f invViewY = mSpreadView.GetInverse();
    const CVector3f eyeY = invViewY * worldAnchor;
    if (mProjection == kProjection_Perspective && eyeY.GetY() <= 0.f) {
      return CTransform4f::Identity();
    }
    const float dz = weight * (mSpreadY - 1.f) * (eyeY.GetZ() - mSpreadCenterZ);
    return mSpreadView * CTransform4f(1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, dz) *
           invViewY;
  }
  if (mSpread == 1.f || weight == 0.f) {
    return CTransform4f::Identity();
  }
  // Every compact widget past the middle belongs to a side cluster (radar, threat bar, selectors,
  // missile readout, minimap), held by a strut that moves by the whole offset; one offset per side
  // keeps each cluster's pieces registered with each other and with the strut.
  const float inner = kSpreadSliceInner * mSpreadHalfWidth;
  const float full = weight * (mSpread - 1.f) * mSpreadHalfWidth;
  const CTransform4f invView = mSpreadView.GetInverse();
  const CVector3f eye = invView * worldAnchor;
  if (mProjection != kProjection_Perspective) {
    const float t = eye.GetX() - mSpreadCenterX;
    const float slide = t < -inner ? -full : t > inner ? full : 0.f;
    return CTransform4f::Translate(mSpreadView.Rotate(CVector3f(slide, 0.f, 0.f)));
  }
  if (eye.GetY() <= 0.f) {
    return CTransform4f::Identity();
  }
  // x += offset * depth: an exact image translation, so a widget with depth keeps its authored
  // shape off axis (a rigid slide widened the selectors). Decided in the unlagged view and carried
  // through the HUD lag: the point is taken back, slid there, and lagged again.
  const CVector3f idle = mSpreadLagInv * eye;
  const float t = idle.GetY() > 0.f ? idle.GetX() / idle.GetY() : eye.GetX() / eye.GetY();
  float offset = t < -inner ? -full : t > inner ? full : 0.f;
  if (idle.GetY() > 0.f && offset != 0.f) {
    const CVector3f moved = mSpreadLag * (idle + CVector3f(offset * idle.GetY(), 0.f, 0.f));
    if (moved.GetY() > 0.f) {
      offset = moved.GetX() / moved.GetY() - eye.GetX() / eye.GetY();
    }
  }
  return mSpreadView * CTransform4f(1.f, offset, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f) *
         invView;
}
#endif

CVector3f CGuiCamera::ConvertToScreenSpace(const CVector3f& point) const {
  CVector3f rotated = RotateTranslateW2O(point);

  if (rotated.IsNonZero() && mProjection == kProjection_Perspective) {
    CMatrix4f xf = CGraphics::CalculatePerspectiveMatrix(
        mCameraParms.mPerspective.mFov, mCameraParms.mPerspective.mAspect,
        mCameraParms.mPerspective.mNear, mCameraParms.mPerspective.mFar);

    return xf.MultiplyOneOverW(rotated);
  }

  return CVector3f(-1.f, -1.f, 1.f);
}
