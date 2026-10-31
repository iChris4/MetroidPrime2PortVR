// The Ing REL's (module 29) CIngExitHostEffect, ported for behaviour rather than bytes: the swarm
// effect CIng's ExitHost state spawns (ctor fn_29_1105C, dtor fn_29_FF88, and the four vtable
// overrides Think fn_29_10F58, AcceptScriptMsg fn_29_10EB4, AddToRenderer fn_29_10DEC and Render
// fn_29_10E64 plus their helpers). The member map is docs/module-analysis/IngFamily.md section A.9.
//
// fn_29_11490 is a plain `CModelData()`, inlined below as the base class's model argument.
// fn_29_11438 is the copy-assignment of retail's inline [size][60 x CVector3f] trail container; the
// ctor only ever fills it with 60 copies of the actor's translation, so it is done in the ctor body
// rather than as a function.
// fn_29_10D50 is the child-particle-system builder both token members go through (once per asset).
// fn_29_BAB4 is CAABox's copy constructor, inlined at the one place the particle bounds are seeded.

#include "MetroidPrime/Enemies/CIngExitHostEffect.hpp"

#include "Collision/CMaterialList.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleGen.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/TCastTo.hpp"

namespace {

// The ctor's asset-to-token step, inlined twice in retail: the resource type comes from the loader,
// then the pool hands back a token. It stays a plain CToken because asset A is an SRSC and asset B a
// PART; ConstructEffect converts.
CToken LoadAssetToken(CAssetId asset) {
  return gpSimplePool->GetObj(SObjectTag(gpResourceFactory->GetResourceTypeById(asset), asset));
}

// Builds the effect from a token and a scale: CElementGen::ConstructChildParticleSystem with the
// token's own reference type (retail reads `token.mObjRef->mObjTag.type`: 0x10D60 `lwz r31, 0x0(r6)`
// and
// 0x10DC4 `lwz r4, 0xc(r31)`), seed 0 (0x10D9C), flags 0x1 (0x10DA8), no model
// lights, emitting, the actor's
// translation (retail's r9, the caller's 5th argument at 0x10D8C), identity orientation, zero
// global translation, the scale (retail's r4, 0x10D78/0x10DB0), white and a unit local scale.
// The body has no branch at all (asm 0x10D50-0x10DC8: one CColor::White call, one
// ConstructChildParticleSystem call, one return), so the `HasReference` test below is the host's
// only: retail dereferences the token's reference with no null test and would fault on an empty
// token, and the host returns a null effect instead (the Think, the destructor and Render all test
// the pointers). Its false arm is the only one a run can take, since the ctor's two tokens come
// from the pool.
// @ fn_29_10D50
CParticleGen* ConstructEffect(CToken& token, const CVector3f& scale, const CVector3f& translation) {
  if (!token.HasReference()) {
    return nullptr;
  }
  return CElementGen::ConstructChildParticleSystem(
      token, token.GetReferenceType(), 0, CElementGen::kOSF_One, false, true, translation,
      CTransform4f::Identity(), CVector3f::Zero(), CTransform4f::Identity(), scale,
      CColor::White(), CVector3f::One());
}

} // namespace

// @ fn_29_1105C
// Base: CActor(uid, "IngExitHostSwarmFx", info, 0, xf, CModelData(), CMaterialList(),
// CActorParameters(), kInvalidUniqueId) - retail's material list is empty (two zero words), unlike
// the mini portal's single kMT_Projectile. The name is rodata lbl_29_rodata_7C8.
CIngExitHostEffect::CIngExitHostEffect(TUniqueId uid, const CEntityInfo& info,
                                       const CTransform4f& xf, const CVector3f& dest,
                                       const CVector3f& scale, CAssetId assetA, CAssetId assetB,
                                       float x2c4, float x290, TUniqueId target, float x298,
                                       float x2a0, ushort sound)
: CActor(uid, rstl::string_l("IngExitHostSwarmFx"), info, 0, xf, CModelData(), CMaterialList(),
         CActorParameters(), kInvalidUniqueId)
, mXf(xf) // 0x158: a copy of the ctor's transform, CTransform4f's copy ctor
// 0x188: (null area, flags 3, index 0, 1.0f, 1.0f, no avoidance filter, kRP_Center); rodata_64,
// which the ctor loads for both radii, is 1.0f.
, mPathFindSearch(nullptr, 3, 0, 1.f, 1.f, 0, CPFRegion::kRP_Center)
, mDest(dest)               // 0x274
, mScale(scale)             // 0x280
, mX28c(0.f)                // 0x28c, asm 0x111DC (rodata_798 = 0.0f)
, mX290(x290)               // 0x290, asm 0x111E0 (f2)
, mTarget(target)           // 0x294, asm 0x111E8 (the target id, lhz/strh)
, mX298(x298)               // 0x298, asm 0x111EC (f3)
, mX29c(x298)               // 0x29c, asm 0x111F0 (f3 again)
, mX2a0(x2a0)               // 0x2a0, asm 0x111F4 (f4)
, mTokenA(LoadAssetToken(assetA)) // 0x2a4, asm 0x11238 (the pool + CToken copy)
, mEffectA(nullptr)               // 0x2b0, asm 0x11274
, mTokenB(LoadAssetToken(assetB)) // 0x2b4, asm 0x11394
, mEffectB(nullptr)               // 0x2c0, asm 0x11300
, mX2c4(x2c4)                     // 0x2c4, asm 0x11308 (f1)
, mBezierCount(0)                 // 0x2c8, asm 0x1130C
, mX2fc(0.f)                      // 0x2fc, asm 0x11310 (rodata_798 = 0.0f)
, mX300(1.f)                      // 0x300, asm 0x11314 (rodata_7A8 = 1.0f)
, mTrailIndex(0)                  // 0x5d8, asm 0x1131C
, mSwarmMoveHandle()              // 0x5dc, null, asm 0x11320
, mSound(sound)                   // 0x5e0, asm 0x11324
, mFlags(0)                       // 0x5e2, asm 0x11328-0x11330 (bit 7 cleared)
{
  // 0x2cc..0x2f8: the four bezier control points. Retail's ctor never writes them (no store between
  // 0x2c8 at asm 0x1130C and 0x2fc at asm 0x11310), so they stay uninitialised there; the host
  // zeroes them (the array member cannot be value-initialised in the initialiser list).
  mBezier[0] = CVector3f::Zero();
  mBezier[1] = CVector3f::Zero();
  mBezier[2] = CVector3f::Zero();
  mBezier[3] = CVector3f::Zero();

  // The two effects, A then B, both from the actor's translation and the model scale (asm 0x11248
  // and 0x112C8: fn_29_10D50(this, the scale, 0x54, the token)).
  mEffectA = ConstructEffect(mTokenA, scale, GetTranslation());
  mEffectB = ConstructEffect(mTokenB, scale, GetTranslation());

  // fn_29_1105C ends by calling retail slot 0x34 on mEffectA only - CParticleGen::SetGeneratorRate
  // (asm 0x11334-0x11344, with f1 = 1.0f from rodata_7A8).
  if (mEffectA) {
    mEffectA->SetGeneratorRate(1.f);
  }

  // 0x5e2: retail clears bit 7 and leaves the other bits as they were, i.e. read from uninitialised
  // memory, which the host does not do (zero above); the effect's Think sets bit 7 once it has
  // arrived, and ExitHost's Update watcher reads it. The clear is `rlwimi r0, r6, 7, 24, 24` with
  // r6 = 0 (asm 0x1132C): bit 24 of the word = 1u<<(31-24) = 0x80 of the byte.
  //
  // 0x304/0x308: fn_29_11438 is retail's copy-assignment for the inline trail container - the ctor
  // builds 60 copies of the actor's translation (retail 0x54) in a temporary and copies them in
  // (asm 0x11348-0x113F0 fills the temporary, asm 0x113F8 copies it into 0x304; the container's
  // size word at 0x304 is the 0x3c = 60 of asm 0x11424, and the host keeps the count in the array).
  const CVector3f translation = GetTranslation();
  for (int i = 0; i < 60; ++i) {
    mTrail[i] = translation;
  }
}

// @ fn_29_FF88
// Retail resets the vtable (asm 0xFFA8-0xFFB4), deletes each effect through CParticleGen's deleting
// destructor (slot 0x08, with the deleting flag 1) when its pointer is non-null, then destroys the
// two tokens and the CActor base in reverse. The two "created" bytes (0x2ac/0x2bc) only gate those
// deletes (asm 0xFFBC, 0xFFF4), so the host's null check on the pointers is the same test.
CIngExitHostEffect::~CIngExitHostEffect() {
  delete mEffectB;
  delete mEffectA;
}

// @ fn_29_1077C
// One step of the swarm's flight. The path is a cubic bezier rebuilt every leg; mX28c is the time
// spent on the current leg and mX2fc its length, so mX28c / (mX2fc / speed) is the curve parameter.
// Retail samples the *clamped* parameter before bumping mX28c, and measures the arrival distance
// from the pre-move translation; only the arrival sets the 0x5e2 bit 7 flag CIng::ExitHost's Update
// watches.
void CIngExitHostEffect::Fly(float dt, CStateManager& mgr) {
  if (mBezierCount == 0) {
    BuildBezier(mgr);
  }
  const CVector3f pos = GetTranslation();
  const float speed = mX300 * mX290;
  CVector3f point = pos;
  if (mBezierCount == 4 && speed > 0.f) {
    const float t = mX2fc / speed;
    if (t > 0.f) {
      const float along = mX28c / t;
      point = CMath::GetBezierPoint(mBezier[0], mBezier[1], mBezier[2], mBezier[3],
                                    1.f < along ? 1.f : along); // 0x10838 bge: NaN keeps it
      if (along >= 1.f) {
        BuildBezier(mgr); // the next leg
      }
    }
  }

  mX28c += dt;

  const CVector3f delta = mDest - pos;
  const float reach = dt * mX290;
  if (!(delta.MagSquared() > reach * reach)) { // 0x108D4 ble to the arrival (a NaN too)
    SetTranslation(mDest);
    mFlags |= 0x80;
  } else {
    SetTranslation(point);
  }
}

// @ fn_29_1065C
// A small random offset across the actor's own up/right axes: Range(0, 4) up, then Range(-2, 2)
// right (retail reads the transform columns at 0x2c and 0x24, and draws the two in that order).
CVector3f CIngExitHostEffect::RandomOffset(CStateManager& mgr) const {
  const CTransform4f& xf = GetTransform();
  const float up = mgr.Random()->Range(0.f, 4.f);
  const float right = mgr.Random()->Range(-2.f, 2.f);
  return up * xf.GetUp() + right * xf.GetRight();
}

// @ fn_29_1046C
// The length of the bezier, as the sum of its four quarter chords.
float CIngExitHostEffect::BezierLength() const {
  if (mBezierCount != 4) {
    return 0.f;
  }
  const CVector3f p0 = mBezier[0];
  const CVector3f p1 = CMath::GetBezierPoint(mBezier[0], mBezier[1], mBezier[2], mBezier[3], 0.25f);
  const CVector3f p2 = CMath::GetBezierPoint(mBezier[0], mBezier[1], mBezier[2], mBezier[3], 0.5f);
  const CVector3f p3 =
      CMath::GetBezierPoint(mBezier[0], mBezier[1], mBezier[2], mBezier[3], 0.75f);
  const CVector3f p4 = mBezier[3];
  return (p1 - p0).Magnitude() + (p2 - p1).Magnitude() + (p3 - p2).Magnitude() +
         (p4 - p3).Magnitude();
}

// @ fn_29_10154
// Builds the swarm's next leg: start is the current translation, first is the destination when it is
// within 5 units (squared: 25), else the path's spline point 5 units ahead, else a point 5 units
// along the straight line when the path search fails. The two inner control points pull a quarter of
// the way to the ends with a random offset each; retail calls the two Range draws in RandomOffset.
void CIngExitHostEffect::BuildBezier(CStateManager& mgr) {
  mBezierCount = 0;
  mX28c = 0.f;

  const CVector3f start = GetTranslation();
  const CVector3f delta = mDest - start;
  CVector3f first = mDest;
  if (delta.MagSquared() > 25.f) {
    if (mPathFindSearch.Search(start, mDest) == CPathFindSearch::kR_Success) {
      mPathFindSearch.GetSplinePointWithLookahead(first, start, 5.f);
    } else if (delta.IsMagnitudeSafe()) {
      first = start + delta.AsNormalized() * 5.f;
    } else {
      first = start;
    }
  }

  const CVector3f quarter = (first - start) * 0.25f;
  mBezier[0] = start;
  mBezier[1] = start + quarter + RandomOffset(mgr);
  mBezier[2] = first - quarter + RandomOffset(mgr);
  mBezier[3] = first;
  mBezierCount = 4;

  mX2fc = BezierLength();
  mX300 = 1.f - mgr.Random()->Range(0.f, 0.5f);
}

// @ fn_29_1006C
// Drops the swarm's move emitter (kSM_Delete).
void CIngExitHostEffect::StopSwarmSfx() {
  if (mSwarmMoveHandle) {
    CSfxManager::RemoveEmitter(mSwarmMoveHandle);
    mSwarmMoveHandle = CSfxHandle();
  }
}

// @ fn_29_100B8
// The swarm's move emitter: started on the first Think at the actor's translation (volume 0x7f, the
// current area, acoustics on, looped, medium priority), then followed along its forward.
void CIngExitHostEffect::StartSwarmSfx() {
  if (!mSwarmMoveHandle) {
    mSwarmMoveHandle = CSfxManager::AddEmitter(mSound, GetTranslation(), 0x7f,
                                               GetCurrentAreaId().Value(), true, true,
                                               CSfxManager::kMedPriority);
  } else {
    CSfxManager::UpdateEmitter(mSwarmMoveHandle, GetTranslation(), GetTransform().GetForward(),
                               0x7f);
  }
}

// @ fn_29_10944
// The two particle effects. The swarm's own position is the actor's, blended towards the target's
// aim point while mX29c (the ctor's x298, the homing time) runs down: t = mX29c / mX298 weights the
// aim point, so it starts on the aim point and ends on the actor's translation. Effect A sits there;
// effect B follows the trail ring mX2c4 * 60 frames behind. Then the actor's render and other bounds
// become effect A's own, grown by B's when B has any.
void CIngExitHostEffect::UpdateParticles(float dt, CStateManager& mgr) {
  if (mEffectA == nullptr || mEffectB == nullptr) {
    return;
  }

  CVector3f pos = GetTranslation();
  if (mX29c > 0.f) {
    const float left = mX29c - dt;
    mX29c = left > 0.f ? left : 0.f;
    CActor* target = TCastToPtr< CActor >(mgr.ObjectById(mTarget));
    if (target != nullptr) {
      const CVector3f aim = target->GetAimPosition(mgr, 0.f);
      const float t = mX29c / mX298;
      const float w = 1.f - t;
      pos = w * pos + t * (aim + mX2a0 * target->GetTransform().GetForward());
    }
  }

  mTrail[mTrailIndex] = pos;
  mTrailIndex = (mTrailIndex + 1) % 60;
  mEffectA->SetTranslation(pos);
  mEffectA->Update(dt);

  // Retail multiplies 60 by the frame quotient first, then by mX2c4 (asm 0x10B38-0x10B54).
  const int lag = static_cast< int >(mX2c4 * (60.f * (dt / 0.016666668f)));
  int idx = (mTrailIndex - lag) % 60;
  if (idx < 0) {
    idx += 60;
  }
  mEffectB->SetTranslation(mTrail[idx]);
  mEffectB->Update(dt);

  // Retail's optional-object dance: A's box is the seed, B's grows it, and only a valid A writes the
  // actor's bounds (the same six floats to 0xcc and 0xe4). Both invalid leaves the bounds alone.
  const rstl::optional_object< CAABox > a = mEffectA->GetBounds();
  const rstl::optional_object< CAABox > b = mEffectB->GetBounds();
  if (a) {
    CAABox box = *a;
    if (b) {
      box.AccumulateBounds(b->GetMinPoint());
      box.AccumulateBounds(b->GetMaxPoint());
    }
    SetRenderBounds(box);
    SetOtherBounds(box);
  }
}

// @ fn_29_10F58
// Retail's Think (vtable 0x14) - and it does not chain to CActor's. Once the swarm has arrived both
// generators are switched off and, as soon as neither effect has a particle left, the actor asks to
// be deleted; that is how ExitHost's swarm disappears.
void CIngExitHostEffect::Think(float dt, CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }

  Fly(dt, mgr);
  UpdateParticles(dt, mgr);
  StartSwarmSfx();

  if (IsDone()) {
    if (mEffectA != nullptr) {
      mEffectA->SetGeneratorRate(0.f);
    }
    if (mEffectB != nullptr) {
      mEffectB->SetGeneratorRate(0.f);
    }
    const int countA = mEffectA != nullptr ? mEffectA->GetParticleCount() : 0;
    const int countB = mEffectB != nullptr ? mEffectB->GetParticleCount() : 0;
    if (countA == 0 && countB == 0) {
      mgr.DeleteObjectRequest(GetUniqueId());
    }
  }
}

// @ fn_29_10EB4
// vtable 0x18: CEntity's handler (not CActor's), then kSM_Delete drops the move emitter and
// kSM_AreaLoaded (0x10EF8: 'XALD') tells the path search which area's paths to use.
void CIngExitHostEffect::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CEntity::AcceptScriptMsg(mgr, msg);
  switch (msg.GetMessage()) {
  case kSM_Delete:
    StopSwarmSfx();
    break;
  case kSM_AreaLoaded:
    mPathFindSearch.SetArea(
        mgr.GetWorld()->GetAreaAlways(GetCurrentAreaId()).GetPostConstructed()->mPathArea);
    break;
  default:
    break;
  }
}

// @ fn_29_10DEC
// vtable 0x28: B then A, and no CActor::AddToRenderer.
void CIngExitHostEffect::AddToRenderer(const CStateManager&) const {
  if (mEffectB != nullptr) {
    gpRender->AddParticleGen(*mEffectB);
  }
  if (mEffectA != nullptr) {
    gpRender->AddParticleGen(*mEffectA);
  }
}

// fn_29_10E64 (0x50, asm 0x00010E64-0x00010F40): vtable 0x2C, effect B (0x2c0) then A (0x2b0), each
// through CParticleGen's render (slot 0x10, asm 0x10E80/0x10E94), and no CActor::Render. Like
// fn_29_10D50 it has no branch in retail - the two weak-pointer bytes 0x2ac/0x2bc are *not* tested
// here (contrast fn_29_10DEC, which tests both at asm 0x10E04/0x10E2C), so the null tests below are
// the host's, standing in for the fault retail would take on an effect its ctor could not build.
//
// Dead on the host, so no arm can be marked: an actor's Render runs only from
// CStateManager::RenderActorQueue, whose queues are filled by `CActor`-derived classes that call
// mgr.RenderLast* - and CActor::AddToRenderer (src/MetroidPrime/CActor.cpp:387) reaches Render only
// when HasModelData(). This class is built with CModelData() (ctor, asm 0x11200) and its own
// AddToRenderer (fn_29_10DEC) never calls the base: it submits the two particle gens and nothing
// else. A coverage run that spawns the effect (fsm ExitHost) confirms it: AddToRenderer runs,
// Render does not.
void CIngExitHostEffect::Render(const CStateManager&) const {
  if (mEffectB != nullptr) {
    mEffectB->Render();
  }
  if (mEffectA != nullptr) {
    mEffectA->Render();
  }
}
