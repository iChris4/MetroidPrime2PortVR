#include "MetroidPrime/Enemies/CWallCrawler.hpp"

#include "Collision/CCollidableSphere.hpp"
#include "Collision/CMaterialFilter.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CPlane.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CSphere.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"
#include "MetroidPrime/CGameCollision.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CPatternedInfo.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptAIWaypoint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWaypoint.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"
#include "WorldFormat/CAreaOctTree.hpp"
#include "WorldFormat/CMetroidAreaCollider.hpp"

// Read from the retail WallCrawler REL (G2ME01, asm/auto_00_00000000_text.s) for behaviour, not
// for code shape; docs/module-analysis/WallCrawler.md has the function map and the evidence.
//
// The six trailing ctor floats (f1..f6) land in x848_..x85c_; x838_/x844_ are the constraint
// plane's normal and constant (the header's names are offsets only):
//   x848_ (f1)  touch-bounds scale
//   x84c_ (f2)  floor-snap rate, per 1/60 s
//   x850_ (f3)  bend weight ramp-up divisor while tumbling     (kART_Six)
//   x854_ (f4)  bend weight ramp-down divisor while tumbling
//   x858_ (f5)  bend weight ramp-up divisor while re-orienting (kART_Five)
//   x85c_ (f6)  bend weight ramp-down divisor while re-orienting
// The 0x860/0x861 bits past mDisableMove (the masks are the retail's, asm 0x258C-0x25F0):
//   x860_28_  (0x08) tumbling: no align surface, rolling about the forward axis (set in AlignToFloor)
//   x860_29_  (0x04) on a surface but up is still > ~30 degrees off its normal (set in AlignToFloor)
//   x860_30_  (0x02) the kART_Six additive reaction is playing
//   x860_31_  (0x01) the kART_Five additive reaction is playing
//   x861_24_  (0x80) bending enabled (ctor sets it; nothing in this module clears it)

namespace {

// @ fn_83_1CB4
// @ fn_83_1F04
// @ fn_83_1E78
bool PointOnSurface(const CCollisionSurface& surface, const CVector3f& point) {
  const CVector3f normal = surface.GetNormal(); // asm 0x1D18
  // The projection onto the triangle's plane: point - dot(point - vert0, normal) * normal
  // (asm 0x1F50-0x1F64; vert0 is the plane's point, read at asm 0x1F08).
  const CVector3f projected = point - CVector3f::Dot(point - surface.GetVert(0), normal) * normal;
  for (int i = 0; i < 3; ++i) { // asm 0x1E14 (the loop runs three times)
    const CVector3f edge = surface.GetVert((i + 2) % 3) - surface.GetVert(i); // asm 0x1DAC-0x1DC4
    const CVector3f delta = projected - surface.GetVert(i); // asm 0x1D80-0x1D90
    // dot(normal, cross(delta, edge)) < 0 means outside (asm 0x1DE0-0x1E00; the cross product
    // is fn_83_1E78, asm 0x1E78-0x1EB4).
    if (CVector3f::Dot(normal, CVector3f::Cross(delta, edge)) < 0.f) {
      return false;
    }
  }
  return true;
}

// Plays or retunes the bend reaction while the weight is up, stops it once it reaches zero.
// The retail: the weight cmd is the body-state cmd 0x20 carrying the weight (asm 0x804-0x85C),
// the reaction cmd is 0x1D carrying (reaction type, weight, true) (asm 0x868-0x8E4 for the Six
// bend, asm 0x9C4-0xA40 for the Five one) and the stop is 0x1E (asm 0x8FC-0x938).
void DeliverBend(CBodyController* bc, float weight, bool& playing, pas::EAdditiveReactionType type) {
  if (weight > 0.0001f) { // asm 0x7EC-0x7F4 (rodata 0.0001)
    if (playing) {
      bc->CommandMgr().DeliverCmd(CBCAdditiveWeightCmd(weight));
    } else {
      bc->CommandMgr().DeliverCmd(CBCAdditiveReactionCmd(type, weight, true));
    }
    playing = true;
  } else {
    bc->CommandMgr().DeliverCmd(CBodyStateCmd(kBSC_StopReaction));
    playing = false;
  }
}

} // namespace

// @ fn_83_2374
CWallCrawler::CWallCrawler(EPatternedAI character, TUniqueId uid, const rstl::string& name,
                           EFlavorType flavor, CEntityInfo& info, const CTransform4f& xf,
                           const CModelData& mData, const CPatternedInfo& pInfo,
                           EMovementType moveType, EColliderType colType, EBodyType bodyType,
                           const CActorParameters& actParms, float sphereRadius,
                           float collisionCloseMargin, float alignAngVel, float advanceWpRadius,
                           float playerObstructionMinDist, EType type, bool disableMove, float f1,
                           float f2, float f3, float f4, float f5, float f6)
: CPatterned(character, uid, name, flavor, info, xf, mData, pInfo, moveType, colType, bodyType,
             actParms)
, mAlignSurface(CVector3f::Zero(), CVector3f::Right(), CVector3f::Forward(), ~u64(0)) // @ fn_83_26CC (0x54): the CCollisionSurface ctor (asm 0x24A0)
, mColSphere(CSphere(CVector3f::Zero(), sphereRadius), GetMaterialList())
, mCollisionCloseMargin(collisionCloseMargin)
, mAlignAngVel(alignAngVel)
, mTumbleAngle(0.f)
, mPatrolPauseRemTime(0.f)
, mAdvanceWpRadius(advanceWpRadius)
, mPlayerObstructionMinDist(playerObstructionMinDist)
, mBendingHackWeight(0.f)
, mType(type)
, mThinkCounter(0)
, x834_(0)
, x838_(CVector3f(0.f, 1.f, 0.f))
, x844_(0.f)
, x848_(f1)
, x84c_(f2)
, x850_(f3)
, x854_(f4)
, x858_(f5)
, x85c_(f6)
, mAlignToFloor(false)
, mHasAlignSurface(false)
, mPlayerObstructed(false)
, mDisableMove(disableMove)
, x860_28_(false)
, x860_29_(false)
, x860_30_(false)
, x860_31_(false)
, x861_24_(true)
, mConstraintPlane(0.f, CUnitVector3f(0.f, 1.f, 0.f, CUnitVector3f::kN_No)) {}

// @ fn_83_208C
CWallCrawler::~CWallCrawler() {}

// @ fn_83_1098
void CWallCrawler::PreThink(float dt, CStateManager& mgr) {
  CPatterned::PreThink(dt, mgr); // asm 0x10B8
  // The guard: the actor must be active (asm 0x10BC-0x10C4, the 0x80 bit at 0x20) and must not
  // be player-obstructed (asm 0x10C8-0x10D0, the 0x20 mask at 0x860 - the retail never sets it).
  if (!GetActive() || mPlayerObstructed) {
    return;
  }
  // The patrol pause must be over (asm 0x10D4-0x10E8), the move must not be disabled (asm
  // 0x10EC-0x10F0, the 0x10 mask) and the actor must not be frozen (asm 0x10F4-0x111C, the
  // frozen percentage below 0.00001).
  if (mPatrolPauseRemTime <= 0.f && !mDisableMove &&
      CMath::AbsF(BodyController()->GetPercentageFrozen()) < 0.00001f) {
    // The align-to-floor flag (asm 0x1120-0x1128, the 0x80 mask) is never set in the retail.
    if (!mAlignToFloor) {
      return;
    }
    // @ fn_83_129C (0x6C): the CMotionState copy the retail makes before AddMotionState
    // (asm 0x1148-0x115C) - the C++ copy of the predicted state is the same value.
    AddMotionState(PredictMotion(dt));
    ClearForcesAndTorques();
    if (mHasAlignSurface) { // asm 0x1174-0x117C (the 0x40 mask)
      // Ease towards one collision radius (+0.01) above the surface, at x84c_ per 1/60 s
      // (asm 0x1180-0x124C: the height is dot(normal, pos) - constant - radius - 0.01 and the
      // blend factor is 60 * x84c_ * dt).
      const CPlane plane = mAlignSurface.GetPlane();
      const CVector3f position = GetTranslation();
      const float height = plane.GetHeight(position) - mColSphere.GetSphere().GetRadius() - 0.01f;
      const float t = 60.f * x84c_ * dt;
      SetTranslation(position * (1.f - t) + (position - height * plane.GetNormal()) * t);
    }
    MoveCollisionPrimitive(CVector3f::Zero()); // asm 0x1250-0x125C
  } else if (mPatrolPauseRemTime > 0.f) {
    Stop(); // asm 0x1274-0x127C
  }
}

// @ fn_83_580
void CWallCrawler::Think(float dt, CStateManager& mgr) {
  // Solid follows the actor's movable flag (asm 0x5A0-0x5D0: the 0x80 bit at 0x168; kMT_Solid
  // is 0x3B).
  if (GetMovable()) {
    AddMaterial(kMT_Solid, mgr);
  } else {
    RemoveMaterial(kMT_Solid, mgr);
  }
  CPatterned::Think(dt, mgr); // asm 0x5E0

  if (mPatrolPauseRemTime > 0.f) { // asm 0x5E4-0x610
    const float remaining = mPatrolPauseRemTime - dt; // asm 0x6A4-0x6BC: NaN clamps to 0
    mPatrolPauseRemTime = remaining > 0.f ? remaining : 0.f;
  }

  CBodyController* bc = BodyController();
  if (bc->HasBodyState(pas::kAS_AdditiveReaction)) { // asm 0x614-0x620 (the state id 0x18)
    // The bend: an additive reaction whose weight follows how fast the enemy moves while it
    // tumbles (kART_Six) or turns onto a new surface (kART_Five).
    // The tumbling (0x08) and the re-orienting (0x04) flags survive only while the bending is
    // enabled (asm 0x628-0x67C, the 0x80 bit of 0x861).
    x860_28_ = x860_28_ && x861_24_;
    x860_29_ = x860_29_ && x861_24_;
    // The tumble side: the tumbling flag or the kART_Six flag (asm 0x680-0x690 and 0x7D4-0x7E4).
    const bool tumbleBend = x860_28_ || x860_30_;
    const float speed = GetVelocityWR().Magnitude(); // asm 0x6C0 and 0x740 (the velocity at 0x1A8)
    // The ramp up: (tumbling && !the Five bend) || (re-orienting && !the Six bend)
    // (asm 0x680-0x6A4).
    if ((x860_28_ && !x860_31_) || (x860_29_ && !x860_30_)) {
      if (mBendingHackWeight < 1.f) { // asm 0x6AC-0x6B8
        // The divisor is x850_ while tumbling or playing the Six bend, x858_ otherwise
        // (asm 0x6C4-0x708).
        mBendingHackWeight += dt * speed / (tumbleBend ? x850_ : x858_);
        if (mBendingHackWeight >= 1.f) { // asm 0x70C-0x724
          mBendingHackWeight = 1.f;
        }
      }
    } else if (mBendingHackWeight > 0.f) { // asm 0x72C-0x73C
      // The ramp down uses x854_ (tumbling) or x85c_ (asm 0x748-0x78C).
      mBendingHackWeight -= dt * speed / (tumbleBend ? x854_ : x85c_);
      if (mBendingHackWeight <= 0.f) { // asm 0x790-0x7A4
        mBendingHackWeight = 0.f;
      }
    }

    // Deliver while the weight is up or a bend is playing (asm 0x7AC-0x7D0).
    if (mBendingHackWeight > 0.f || x860_30_ || x860_31_) {
      bool playing;
      if (tumbleBend) { // asm 0x7D4-0x7E4 (the Six side, the reaction type 0x6)
        playing = x860_30_;
        DeliverBend(bc, mBendingHackWeight, playing, pas::kART_Six);
        x860_30_ = playing;
      } else { // asm 0x948-0x954 (the Five side, the reaction type 0x5)
        playing = x860_31_;
        DeliverBend(bc, mBendingHackWeight, playing, pas::kART_Five);
        x860_31_ = playing;
      }
    }
  }

  if (!mAlive) { // asm 0xAA0-0xAA8 (the 0x40 bit at 0x420)
    return;
  }
  CPlayer* player = mgr.Player(0); // asm 0xAAC
  const rstl::optional_object< CAABox > playerBox = player->GetTouchBounds(); // asm 0xAB0-0xAC4
  const rstl::optional_object< CAABox > box = GetTouchBounds(); // asm 0xB08-0xB1C
  if (!playerBox.valid() || !box.valid() || !playerBox->DoBoundsOverlap(*box)) { // asm 0xB60-0xB88
    return;
  }
  // The filter is IncludeExclude(kMT_Solid), built from the module's data_0 = {0x3B, 0x3B,
  // 0x01010000} (asm 0xBB0-0xC18 and 0xCAC-0xD20).
  const CMaterialFilter filter =
      CMaterialFilter::MakeIncludeExclude(CMaterialList(kMT_Solid), CMaterialList());
  if (player->GetMorphBall()->IsBoosting()) { // asm 0xB8C-0xB98
    // Boosting into it hurts the crawler instead; the contact timer is left alone.
    mgr.ApplyDamage(player->GetUniqueId(), GetUniqueId(), player->GetUniqueId(),
                    gpTweakBall->GetBoostBallDamage(), filter, CVector3f::Zero()); // asm 0xC70
  } else if (mCurDamageRemTime <= 0.f) { // asm 0xC7C-0xC8C (the timer at 0x440)
    CDamageInfo damage = GetContactDamage(); // asm 0xC90-0xCA4 (the vtable slot 0xE8)
    damage.SetNoImmunity(true); // asm 0xCCC (the 0x80 bit of the damage info)
    mgr.ApplyDamage(GetUniqueId(), player->GetUniqueId(), GetUniqueId(), damage, filter,
                    CVector3f::Zero()); // asm 0xD78
    mCurDamageRemTime = mDamageWaitTime; // asm 0xD7C-0xD80 (0x444 -> 0x440)
  }
}

// @ fn_83_1FD0
void CWallCrawler::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // The retail reacts to kSM_Create (0x58435254) only (asm 0x2020-0x202C).
  if (msg.GetMessage() == kSM_Create) {
    AddMaterial(kMT_Immovable, mgr); // asm 0x2038 (0x2B)
    RemoveMaterial(kMT_Solid, mgr); // asm 0x2048 (0x3B)
    AddMaterial(kMT_NonSolidDamageable, mgr); // asm 0x2058 (0x32)
  }
  CPatterned::AcceptScriptMsg(mgr, msg); // asm 0x206C
}

// @ fn_83_EE4
void CWallCrawler::Render(const CStateManager& mgr) const { CPatterned::Render(mgr); }

// @ fn_83_4C8
rstl::optional_object< CAABox > CWallCrawler::GetTouchBounds() const {
  // Retail calls the CPhysicsActor vtable slot at 0x80 for the transform; GetPrimitiveTransform is
  // the virtual there (inferred from the slot, not from a symbol). The scale is x848_ (asm 0x4E8)
  // and the box is the base one (asm 0x51C-0x52C).
  return GetBaseBoundingBox().GetTransformedAABox(GetPrimitiveTransform() *
                                                  CTransform4f::Scale(x848_));
}

// @ fn_83_1308
const CCollisionPrimitive* CWallCrawler::GetCollisionPrimitive() const { return &mColSphere; }

// @ fn_83_D8
TUniqueId CWallCrawler::GetNextWaypoint(CStateManager& mgr, const CScriptWaypoint* waypoint,
                                        bool reverse) {
  // Retail ignores `reverse` here (asm 0x0E0-0x0E4); subclasses override the slot.
  return waypoint->NextWaypoint(mgr); // asm 0x0F0-0x0FC (the vtable slot 0x7C)
}

// @ fn_83_114
void CWallCrawler::SetConstraint(CStateManager& mgr, int constraint) {
  x834_ = constraint; // asm 0x124 (the store at 0x834)
  if (constraint != 0) { // asm 0x11C-0x128
    UpdateConstraintPlane(mgr); // asm 0x12C
  }
}

// @ fn_83_148
void CWallCrawler::UpdateConstraintPlane(CStateManager& mgr) {
  // The constraint plane runs through the next three non-collinear patrol waypoints (up to 30 are
  // walked). With only two, it is the vertical-ish plane through them and a point one unit above
  // the second; with fewer, the old plane stays.
  TUniqueId dest = GetDestObj(); // asm 0x194
  if (dest == kInvalidUniqueId) { // asm 0x1A0-0x1A4
    dest = GetConnectedObject(mgr, kSS_Patrol, kSM_Follow); // asm 0x1A8-0x1C4 ("PTRL"/"FOLW")
  }
  const CScriptWaypoint* waypoint = TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(dest));
  CVector3f points[3];
  int count = 0;
  for (int i = 0; count < 3 && i < 30 && waypoint != nullptr; ++i) { // asm 0x34C-0x35C
    const CVector3f pos = waypoint->GetTranslation(); // asm 0x218-0x224
    bool collinear = false;
    if (count == 2) { // asm 0x214
      const CVector3f a = (points[0] - points[1]).AsNormalized(); // asm 0x230-0x264
      const CVector3f b = (pos - points[1]).AsNormalized(); // asm 0x268-0x2A0
      collinear = CMath::AbsF(CVector3f::Dot(a, b)) > 0.984f; // asm 0x2B0-0x2D4 (rodata 0.984)
    }
    if (!collinear) {
      points[count++] = pos;
    }
    waypoint = TCastToConstPtr< CScriptWaypoint >(
        mgr.GetObjectById(GetNextWaypoint(mgr, waypoint, false)));
  }

  CPlane plane(0.f, CUnitVector3f(x838_, CUnitVector3f::kN_No));
  if (count == 3) { // asm 0x360-0x364
    plane = CPlane(points[0], points[1], points[2]); // asm 0x374
  } else if (count == 2 && !(CMath::AbsF(points[0].GetX() - points[1].GetX()) < 0.00001f) && // asm 0x3A8-0x3C8
             !(CMath::AbsF(points[0].GetY() - points[1].GetY()) < 0.00001f)) { // asm 0x3CC-0x3E4
    plane = CPlane(points[0], points[1], points[1] + CVector3f(0.f, 0.f, 1.f)); // asm 0x404-0x424
  } else {
    return; // the old plane stays (asm 0x448)
  }
  x838_ = plane.GetNormal(); // asm 0x37C-0x390 and 0x428-0x43C (the normal at 0x838)
  x844_ = plane.GetConstant(); // asm 0x394 and 0x440 (the constant at 0x844)
}

// @ fn_83_F04
bool CWallCrawler::UpdateWPDestination(CStateManager& mgr) {
  CScriptWaypoint* waypoint = TCastToPtr< CScriptWaypoint >(mgr.ObjectById(GetDestObj())); // asm 0xF2C-0xF40
  if (waypoint == nullptr) {
    return false; // asm 0xF48
  }
  bool advanced = false;
  const CVector3f position = waypoint->GetTranslation(); // asm 0xF4C-0xF60
  // The advance test: the squared distance against the advance radius (asm 0xF78-0xFA8).
  if ((position - GetTranslation()).MagSquared() < mAdvanceWpRadius * mAdvanceWpRadius) {
    mDestObj = GetNextWaypoint(mgr, waypoint, true); // asm 0xFAC-0xFDC (reverse = true)
    advanced = true;
    if (const CScriptAIWaypoint* aiWaypoint = TCastToConstPtr< CScriptAIWaypoint >(waypoint)) { // asm 0xFE0
      if (!(CMath::AbsF(aiWaypoint->GetPause()) < 0.00001f)) { // NaN proceeds (blt) // asm 0xFF4-0x1010
        mPatrolPauseRemTime = aiWaypoint->GetPause(); // asm 0x1014
        // Retail tests the EType word for zero here (no named type is 0) (asm 0x1018-0x1020).
        if (mType == 0) {
          BodyController()->SetLocomotionType(pas::kLT_Relaxed); // asm 0x1024-0x102C (the type 1)
        }
      }
    }
    mgr.SendScriptMsg(waypoint, GetUniqueId(), kSM_Arrived); // asm 0x1030-0x1064 ("ARRV")
  }
  SetDestPos(position); // asm 0x1068-0x1070
  return advanced;
}

// @ fn_83_1310
void CWallCrawler::AlignToFloor(CStateManager& mgr, float radius, const CVector3f& newPos, float dt) {
  // The retail's search box is radius+margin wide (asm 0x13A4-0x13F4: f7 = the radius plus the
  // margin at 0x810) and its best-distance threshold is that same radius+margin: f24 starts as
  // the radius argument (asm 0x1390) and the margin is added to it at asm 0x141C, before the
  // first comparison (asm 0x159C).
  const float extent = radius + mCollisionCloseMargin;
  const CVector3f half(extent, extent, extent);
  CAreaCollisionCache cache(CAABox(newPos - half, newPos + half));
  CGameCollision::BuildAreaCollisionCache(mgr, cache);
  float best = radius + mCollisionCloseMargin;

  // The surface the crawler stands on is re-tested every frame (asm 0x1418-0x1438): the flag is
  // the byte 0x860's 0x40 mask (mHasAlignSurface). The retail never sets the 0x20 mask
  // (mPlayerObstructed) in this module - only PreThink reads it (asm 0x10CC).
  if (mHasAlignSurface) { // asm 0x1418
    mHasAlignSurface = PointOnSurface(mAlignSurface, newPos); // asm 0x142C
  }
  // The search runs when there is no surface, or on every fourth think (asm 0x1440-0x1450).
  if (!mHasAlignSurface || (mThinkCounter & 3) == 0) {
    bool found = false;
    for (int i = 0; i < static_cast< int >(cache.GetNumCaches()); ++i) { // asm 0x159C-0x15A4
      const CMetroidAreaCollider::COctreeLeafCache& leaf = cache.GetOctreeLeafCache(i);
      for (int j = 0; j < leaf.GetNumLeaves(); ++j) {
        const CAreaOctTree::Node& node = leaf.GetLeaf(j);
        const CAreaOctTree::TriListReference triangles = node.GetTriangleArray();
        const CAreaOctTree& tree = node.GetOwner();
        for (int k = 0; k < triangles.GetSize(); ++k) {
          const CCollisionSurface surface = tree.GetTriangle(triangles.GetAt(k));
          float dist;
          if (x834_ == 0) { // asm 0x1454-0x145C
            dist = CMath::AbsF(surface.GetPlane().GetHeight(newPos)); // asm 0x14CC-0x14EC
          } else {
            // Constrained: AI-passthrough triangles are skipped, and the distance of a triangle
            // whose normal agrees with the constraint plane (> 0.5) is scaled up by 1 + that
            // agreement. Constraint 1 compares with the normal, 2 with its reverse, 3 with either.
            // The constraint is the PillBug's `planarConstraint` init property (0xde7e9f94, set
            // through SetConstraint): the shipped placements use 1 and 2 only (Agon Wastes
            // 0b_sand_hall, 0d_sand_hall and 05_portalpuzzle, Sanctuary Fortress
            // 06_cliff_spiderballguardian), so the constraint-3 arm has no shipped instance.
            if ((surface.GetSurfaceFlags() & (static_cast< u64 >(1) << kMT_AIPassthrough)) != 0) {
              continue; // asm 0x1610-0x1630 (the 0x10000000 mask on the flags' low word)
            }
            // The agreement is the full dot with the constraint normal: x, y and z are held in
            // f31/f30/f29 (asm 0x15AC-0x15BC) and summed at asm 0x1674-0x1680 (the `lis r29`
            // beside them is the integer flag mask, not f29).
            float agree = CVector3f::Dot(surface.GetNormal(), x838_);
            if (x834_ == 2) {
              agree = -agree;
            } else if (x834_ == 3) {
              agree = CMath::AbsF(agree);
            }
            float scale = 1.f; // asm 0x16A8-0x16BC (rodata 1.0 and 0.5)
            if (agree > 0.5f) {
              scale += agree;
            }
            dist = scale * CMath::AbsF(surface.GetPlane().GetHeight(newPos)); // asm 0x16C0-0x16E4
          }
          if (dist < best && PointOnSurface(surface, newPos)) { // asm 0x159C-0x1508 and 0x16E8-0x1700
            best = dist; // asm 0x1514 and 0x1708
            // @ fn_83_19C8 (0x5C): the CCollisionSurface copy (asm 0x14BC and 0x160C).
            mAlignSurface = surface; // asm 0x1520-0x1568 (the member at 0x7C0)
            found = true;
          }
        }
      }
    }
    mHasAlignSurface = found; // asm 0x17A0-0x17A4 (the 0x40 mask)
  }

  if (mHasAlignSurface) { // asm 0x17A8-0x17B2
    const CVector3f normal = mAlignSurface.GetNormal(); // asm 0x17B4-0x17BC
    AlignToPlane(CUnitVector3f(normal, CUnitVector3f::kN_No), mAlignAngVel * dt); // asm 0x17E0-0x17E8
    mTumbleAngle = 0.f; // asm 0x17F4-0x1804
    x860_28_ = false; // asm 0x1808-0x1810 (the 0x08 mask)
    x860_29_ = CVector3f::Dot(GetTransform().GetUp(), normal) < 0.86f; // asm 0x1814-0x1854 (rodata 0.86)
  } else {
    // Nothing to stand on: roll forward about the sphere at its rolling speed (degrees/s).
    const CVector3f forward = GetTransform().GetForward(); // asm 0x1878-0x1880
    const CVector3f& velocity = GetVelocityWR();
    const float angVel = 57.295776f * velocity.Magnitude() / mColSphere.GetSphere().GetRadius(); // asm 0x188C-0x18B8 (rodata 57.295776)
    const CVector3f target = CVector3f::Dot(velocity, forward) > 0.f ? forward : -forward; // asm 0x18B0-0x18C4
    AlignToPlane(CUnitVector3f(target, CUnitVector3f::kN_No), angVel * dt); // asm 0x18C8-0x190C
    x860_28_ = true; // asm 0x1910-0x1920 (the 0x08 mask)
    x860_29_ = false; // asm 0x1924-0x192C (the 0x04 mask)
    mTumbleAngle += angVel * dt; // asm 0x1930-0x1938
  }
}

// `degrees` is the most the up vector may turn this call.
// @ fn_83_1B50
void CWallCrawler::AlignToPlane(const CUnitVector3f& normal, float degrees) {
  // The retail's up vector is the transform's third column: (m02, m12, m22) at 0x2C, 0x3C and
  // 0x4C (asm 0x1B70-0x1BA0), which is what GetTransform().GetUp() returns.
  const CVector3f up = GetTransform().GetUp();
  const float dot = CVector3f::Dot(up, normal); // asm 0x1B80-0x1BA0
  // Nothing to do when the up vector already is the normal (|dot - 1| < 0.00001, asm 0x1B98-
  // 0x1BB8) or when it is the opposite one (dot < -0.999, asm 0x1BC0-0x1BC8).
  if (CMath::AbsF(dot - 1.f) < 0.00001f || dot < -0.999f) {
    return;
  }
  // The arc is clamped to the angle in radians (asm 0x1BCC-0x1C08, rodata 0.017453292).
  const CQuaternion rotation = CQuaternion::ShortestRotationArcClamped(
      up, normal, CRelAngle::FromRadians(degrees * 0.017453292f));
  // The arc is applied in the actor's local space (asm 0x1C10-0x1C38).
  const CQuaternion local(rotation.GetScalar(), GetTransform().TransposeRotate(rotation.GetVector()));
  // The retail builds the transform and calls SetTransform (asm 0x1C40-0x1C9C); the port has
  // only SetRotation, which keeps the translation.
  SetRotation((CQuaternion::FromMatrix(GetTransform()) * local).BuildNormalized());
}

// GetPlane() builds the plane by value; returning it directly bound the reference to a temporary,
// which GCC compiles to a null reference (PillBug's patrol then faulted in CPlane::GetClosestPoint).
// Retail returns the constraint plane stored at 0x838 (x838_ normal, x844_ constant).
// @ fn_83_D0
const CPlane& CWallCrawler::GetConstraintPlane() const {
  // The retail returns the plane stored at 0x838 (asm 0xD0-0xD4: the address of the member).
  mConstraintPlane = CPlane(x844_, CUnitVector3f(x838_, CUnitVector3f::kN_No));
  return mConstraintPlane;
}

// @ fn_83_1EB8
CVector3f CWallCrawler::ProjectVectorToPlane(const CVector3f& vec, const CVector3f& planeDir) {
  return vec - CVector3f::Dot(vec, planeDir) * planeDir; // asm 0x1EC4-0x1EF0
}

// @ fn_83_1F7C
// Dead in the retail: nothing in the module calls it (the scan in section g of the module note)
// and the symbol is not global, so no other module can reach it either.
CVector3f CWallCrawler::GetClosestPointOnPlane(const CVector3f& point, const CPlane& plane) {
  return plane.GetClosestPoint(point); // asm 0x1F88-0x1FC8 (the plane's point and normal)
}

// The module's entry points. Retail's are four bytes each (a bare `blr`), because the module's
// constructors do any registration; `module prolog` in platform/compiled_modules.cpp is what the
// port calls instead.
// @ fn_83_C8
void RELMain() {}
// @ fn_83_CC
void RELExit() {}
