#ifndef _CGUICAMERA
#define _CGUICAMERA

#include "GuiSys/CGuiWidget.hpp"

class CGuiCamera : public CGuiWidget {
public:
  enum EProjection { kProjection_Perspective, kProjection_Orthographic };

  union UCameraParms {
    struct {
      float mFov;
      float mAspect;
      float mNear;
      float mFar;
    } mPerspective;
    struct {
      float mLeft;
      float mRight;
      float mTop;
      float mBottom;
      float mNear;
      float mFar;
    } mOrthographic;
  };

  CGuiCamera(const CGuiWidgetParms& parms, float fov, float aspect, float znear, float zfar);
  CGuiCamera(const CGuiWidgetParms& parms, float left, float right, float top, float bottom,
             float znear, float zfar);

  // CGuiWidget
  FourCC GetWidgetTypeID() const override { return 'CAMR'; }
  EWidgetUsageFlags GetWidgetUsageFlags() const override { return kWUF_None; }
  void Draw(const CGuiWidgetDrawParms& parms) const override;

  static CGuiWidget* Create(CGuiFrame* frame, CInputStream& in, CSimplePool* pool, uint version);

  CVector3f ConvertToScreenSpace(const CVector3f& point) const;
#ifdef TARGET_PC
  // Port: widescreen HUD (PortOverlay::HudWide), as MP1's CGuiCamera. Only a camera marked
  // spreadable spreads its frame's widgets; the pause, map and memo frames keep the game's
  // proportions. Draw computes the factor, CGuiFrame::Draw moves each widget by it.
  void SetSpreadable(bool spreadable) { mSpreadable = spreadable; }
  float GetAspectSpread() const { return mSpread; }
  float GetSpreadCenterX() const { return mSpreadCenterX; }
  // The vertical spread, the same remap on the screen-up axis: 1 unless the window is narrower
  // than 4:3, where the frame keeps its authored width and gains height. Half the authored height
  // as a tangent per unit depth (perspective) or in world units (ortho), about GetSpreadCenterZ.
  float GetSpreadY() const { return mSpreadY; }
  float GetSpreadHalfHeight() const { return mSpreadHalfHeight; }
  float GetSpreadCenterZ() const { return mSpreadCenterZ; }
  // Half the authored width as a tangent per unit depth (perspective), or in world units (ortho).
  float GetSpreadHalfWidth() const { return mSpreadHalfWidth; }
  // Half the drawn view's width in the same units: widened by the aspect, larger by the HUD scale.
  float GetSpreadScreenHalfWidth() const { return mSpreadScreenHalfWidth; }
  bool IsPerspective() const { return mProjection == kProjection_Perspective; }
  const CTransform4f& GetSpreadView() const { return mSpreadView; }
  // The slices a screen-spanning frame model is warped in, in fractions of the authored half
  // width, and the warp's horizontal offset at `t` (view X per unit depth, or ortho units from the
  // centre).
  static const float kSpreadSliceInner;
  static const float kSpreadSliceOuter;
  // `curved`: the eased variant for the visor frame and helmet arcs, a smoothstep stretch over the
  // ledge only, so the glass brackets beside the energy bar keep their width.
  static const float kSpreadCurveInner;
  static const float kSpreadCurveOuter;
  float GetSpreadSliceOffset(float t, bool curved = false) const;
  // The spread of a compact widget or cluster anchored at `worldAnchor`: an exact screen-space
  // slide, the whole side offset past the middle slice, none inside it. `weight` fades it.
  CTransform4f GetSpreadTransform(const CVector3f& worldAnchor, float weight = 1.f) const;
  // HUD lag moves the frame's content on screen. `idleToView` maps the frame's unlagged view space
  // onto the drawn one (identity at rest). The spread is applied in the unlagged space and carried
  // through the lag, so content keeps its shape while it lags (MP1 sync: warp before lag).
  void SetSpreadLag(const CTransform4f& idleToView) {
    mSpreadLag = idleToView;
    mSpreadLagInv = idleToView.GetInverse();
  }
  // Places a camera at `xf` whose unlagged pose is unrotated at `idlePos`, and sets the lag to match.
  void SetHudO2WTransform(const CTransform4f& xf, const CVector3f& idlePos);
  // The drawn screen tangent of unlagged tangent `tangent` at view depth `depth`.
  float GetLaggedTangent(float tangent, float depth) const;
  // PortVr: the tangents of the half angles the perspective view the last Draw set up spans across
  // and up (after the aspect match); zero for an orthographic camera or before the first Draw. The
  // headset lays the HUD's 2D draws on a plane spanning the same angles (CInGameGuiManager::Draw).
  float GetDrawTanHalfWidth() const { return mDrawTanX; }
  float GetDrawTanHalfHeight() const { return mDrawTanY; }
#endif
  UCameraParms GetParms() const { return mCameraParms; }
  void SetParms(const UCameraParms& parms) { mCameraParms = parms; }

private:
  EProjection mProjection;
  UCameraParms mCameraParms;
#ifdef TARGET_PC
  bool mSpreadable = false;
  mutable float mSpread = 1.f;
  mutable float mSpreadCenterX = 0.f;
  mutable float mSpreadY = 1.f;
  mutable float mSpreadHalfHeight = 0.f;
  mutable float mSpreadCenterZ = 0.f;
  mutable float mSpreadHalfWidth = 0.f;
  mutable float mSpreadScreenHalfWidth = 0.f;
  mutable CTransform4f mSpreadView = CTransform4f::Identity();
  CTransform4f mSpreadLag = CTransform4f::Identity();
  CTransform4f mSpreadLagInv = CTransform4f::Identity();
  mutable float mDrawTanX = 0.f;
  mutable float mDrawTanY = 0.f;
#endif
};
CHECK_SIZEOF(CGuiCamera, 0xd8)

#endif // _CGUICAMERA
