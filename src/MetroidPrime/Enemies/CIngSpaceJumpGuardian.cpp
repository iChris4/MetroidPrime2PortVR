#include "MetroidPrime/Enemies/CIngSpaceJumpGuardian.hpp"

#include "Kyoto/Animation/CPOINode.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"
#include "MetroidPrime/BodyState/CBodyStateCmdMgr.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CCollisionActorManager.hpp"
#include "MetroidPrime/CGameLight.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CSafeZoneManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CValidEntityPredicate.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Cameras/CCameraShakerManager.hpp"
#include "MetroidPrime/Enemies/CIngMiniPortalAttack.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "MetroidPrime/ScriptObjects/CScriptAIWaypoint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptAiJumpPoint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptCameraShaker.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWaypoint.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Weapons/CShockWave.hpp"
#include "MetroidPrime/Weapons/CWeapon.hpp"
#include "REL/REL_Setup.h"

#include "Kyoto/Streams/CInputStream.hpp"
#include "MetroidPrime/CCollisionActor.hpp"
#include "MetroidPrime/CKnockBackInfo.hpp"
#include "MetroidPrime/Enemies/CCollisionTracker.hpp"
#include "MetroidPrime/ScriptLoader/SLdrIngSpaceJumpGuardian.hpp"

namespace {
struct SJointSphere {
  const char* mName;
  float mRadius;
};
} // namespace

static CPatterned::StateMachine::STriggerFunction skTriggers[] = {
    {"StateOver",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::StateOver)},
    {"HasIntroPattern",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::HasIntroPattern)},
    {"IntroPatternOver", static_cast< CPatterned::StateMachine::TriggerFunc >(
                             &CIngSpaceJumpGuardian::IntroPatternOver)},
    {"ShouldJump",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::ShouldJump)},
    {"HasJumpTarget",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::HasJumpTarget)},
    {"IsFacingTarget",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::IsFacingTarget)},
    {"IsFacingWaypoint", static_cast< CPatterned::StateMachine::TriggerFunc >(
                             &CIngSpaceJumpGuardian::IsFacingWaypoint)},
    {"ShouldTaunt",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::ShouldTaunt)},
    {"ShouldMiniPortalAttack", static_cast< CPatterned::StateMachine::TriggerFunc >(
                                   &CIngSpaceJumpGuardian::ShouldMiniPortalAttack)},
    {"FoundMiniPortalAttack", static_cast< CPatterned::StateMachine::TriggerFunc >(
                                  &CIngSpaceJumpGuardian::FoundMiniPortalAttack)},
    {"SkipToNextJumpPoint", static_cast< CPatterned::StateMachine::TriggerFunc >(
                                &CIngSpaceJumpGuardian::SkipToNextJumpPoint)},
    {"PathOver",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::PathOver)},
    {"PathShagged",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::PathShagged)},
    {"PathExists",
     static_cast< CPatterned::StateMachine::TriggerFunc >(&CIngSpaceJumpGuardian::PathExists)},
};

static CPatterned::StateMachine::SStateFunction skStates[] = {
    {"Start", static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::Start)},
    {"Dead", static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::Dead)},
    {"FollowIntroPattern", static_cast< CPatterned::StateMachine::StateFunc >(
                               &CIngSpaceJumpGuardian::FollowIntroPattern)},
    {"FaceTarget",
     static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::FaceTarget)},
    {"Jump", static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::Jump)},
    {"Taunt", static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::Taunt)},
    {"MiniPortalAttack",
     static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::MiniPortalAttack)},
    {"PathFind",
     static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::PathFind)},
    {"SteerToDest",
     static_cast< CPatterned::StateMachine::StateFunc >(&CIngSpaceJumpGuardian::SteerToDest)},
};

static CPatterned::StateMachine::SCodeFunction skCodeFuncs[] = {
    {"EnableEnergyBar",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::EnableEnergyBar)},
    {"SelectTarget",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::SelectTarget)},
    {"SelectAttackAction",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::SelectAttackAction)},
    {"SelectJumpTarget",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::SelectJumpTarget)},
    {"FindBestMiniPortals", static_cast< CPatterned::StateMachine::CodeFunc >(
                                &CIngSpaceJumpGuardian::FindBestMiniPortals)},
    {"SetIntroJumpTarget",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::SetIntroJumpTarget)},
    {"SetFaceJumpTarget",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::SetFaceJumpTarget)},
    {"SetFaceAttackTarget", static_cast< CPatterned::StateMachine::CodeFunc >(
                                &CIngSpaceJumpGuardian::SetFaceAttackTarget)},
    {"SetFaceWaypoint",
     static_cast< CPatterned::StateMachine::CodeFunc >(&CIngSpaceJumpGuardian::SetFaceWaypoint)},
};

// Guessed name; the joints that get a sphere collision actor, each with its radius.
static const SJointSphere skJointSpheres[] = {
    {"head", 1.5f},    {"Pelvis_SDK", 1.5f}, {"L_knee", 1.5f},  {"R_knee", 1.5f},
    {"F_elbow", 1.5f}, {"L_elbow", 1.5f},    {"R_elbow", 1.5f},
};

static const char* const skEyesName = "eyes";     // Guessed name
static const char* const skShieldName = "Shield"; // Guessed name

// Guessed name; the angles, in degrees around the head, tried for a mini portal position.
static const float skPortalAngles[] = {180.f, 0.f, 135.f, 45.f, -135.f, -45.f, 90.f, -90.f};

namespace {
// Guessed name; accepts only jump points.
class CJumpPointPredicate : public CValidEntityPredicate {
public:
  ~CJumpPointPredicate() override {}

  bool IsValid(const CStateManager& mgr, TUniqueId id) const override {
    return TCastToConstPtr< CScriptAiJumpPoint >(mgr.GetObjectById(id)) != nullptr;
  }
};
} // namespace

// @ fn_34_5E58
CIngSpaceJumpGuardian::CIngSpaceJumpGuardian(TUniqueId uid, const rstl::string& name,
                                             const CEntityInfo& info, const CTransform4f& xf,
                                             const CModelData& modelData,
                                             const CActorParameters& actorParams,
                                             const CPatternedInfo& patternedInfo,
                                             const SLdrIngSpaceJumpGuardianData& data)
: CPatterned(kPAI_IngSpaceJumpGuardian, uid, name, kFT_Zero, info, xf, modelData, patternedInfo,
             kMT_Ground, kCT_One, kBT_BiPedal, actorParams)
, mData(data)
, mMiniPortalDamage(LdrToDamageInfo(data.miniPortalProjectileDamage))
, mCollisionManager(nullptr)
, mPathFindSearch(nullptr, 0x301, patternedInfo.GetPathfindingIndex(), 1.f, 1.f, 0,
                  CPFRegion::kRP_Center)
, mDefaultSpeed(mSpeed)
, mDeathTimer(2.f)
, mLightId(kInvalidUniqueId)
, mLightIntensity(0.f)
, mHeadSeg(0xff)
, mCollarSeg(0xff)
, mAttackAction(kAA_None)
, mPlayerId(kInvalidUniqueId)
, mFaceTarget(kInvalidUniqueId)
, mIntroWaypoint(kInvalidUniqueId)
, mLastJumpPoint(kInvalidUniqueId)
, mBlobId(kInvalidUniqueId)
, mJumpTarget(kInvalidUniqueId)
, mJumpApexHeight(0.f)
, mJumpMode(kJM_None)
, mJumpTimer(0.f)
, mShieldTimer(0.f)
, mJumpDuration(0.f)
, mJumpSfx()
, mMiniPortalEffect()
, mPortalCount(0)
, mPortalIndex(-1)
, mJumping(false)
, mHasJumpPath(false)
, mHeardNoise(false)
, mLastJumpToWaypoint(false)
, mDamageable(true)
, mBlobActive(false)
, mRender(true) {
  if (data.miniPortalEffect != kInvalidAssetId) {
    mMiniPortalEffect = TLockedToken< CGenDescription >(
        gpSimplePool->GetObj(SObjectTag('PART', data.miniPortalEffect)));
  }
  mHeadSeg = GetAnimationData()->GetLocatorSegId(rstl::string_l("head"));
  mCollarSeg = GetAnimationData()->GetLocatorSegId(rstl::string_l("Collar_SDK"));
  mKnockBackController.EnableKnockBackPhysics(false);
  mKnockBackController.EnableAnimReaction(CKnockBackMgr::kAR_KnockBack, false);
  mKnockBackController.EnableAnimReaction(CKnockBackMgr::kAR_Hurled, false);
  SetDrawShadow(false);
}

// @ fn_34_5B30
CIngSpaceJumpGuardian::~CIngSpaceJumpGuardian() {}

// @ fn_34_5760
void CIngSpaceJumpGuardian::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const TUniqueId senderId = msg.GetSenderId();
  const EScriptObjectMessage message = msg.GetMessage();
  CPatterned::AcceptScriptMsg(mgr, msg);
  switch (message) {
  case kSM_Create:
    BodyController()->Activate(mgr, pas::kAS_Invalid);
    SetupCollision(mgr);
    CreateLight(mgr);
    break;
  case kSM_Delete:
    mgr.DeleteObjectRequest(mBlobId);
    mCollisionManager->Destroy(mgr);
    mgr.DeleteObjectRequest(mLightId);
    if (mgr.GetBossId() == GetUniqueId()) {
      mgr.SetBossParams(kInvalidUniqueId, 0.f, 0);
    }
    break;
  case kSM_Activate:
    mCollisionManager->SetActive(mgr, true);
    if (CEntity* light = mgr.ObjectById(mLightId)) {
      light->SetActive(true);
    }
    break;
  case kSM_Deactivate:
    mCollisionManager->SetActive(mgr, false);
    if (CEntity* light = mgr.ObjectById(mLightId)) {
      light->SetActive(false);
    }
    break;
  case kSM_AreaLoaded:
    mPathFindSearch.SetArea(
        mgr.GetWorld()->GetAreaAlways(GetCurrentAreaId()).GetPostConstructed()->mPathArea);
    break;
  case kSM_OffGround:
#ifdef TARGET_PC
    // 0x59EC-0x59F8: stfs to +0x1c0/+0x1c4/+0x1c8 = CPhysicsActor::mMomentum.
    SetMomentumWR(CVector3f(0.f, 0.f, -(GetMass() * GetGravityConstant())));
#else
    SetConstantForceWR(CVector3f(0.f, 0.f, -(GetMass() * GetGravityConstant())));
#endif
    break;
  case kSM_Launching:
#ifdef TARGET_PC
    // 0x5A10 lhz r7,0x38(r1); 0x5A38 sth r7,0x44(r1): preserve incoming sender.
    CPatterned::AcceptScriptMsg(mgr, CScriptMsg(senderId, GetUniqueId(), kSM_OffGround));
#else
    CPatterned::AcceptScriptMsg(mgr, CScriptMsg(GetUniqueId(), GetUniqueId(), kSM_OffGround));
#endif
#ifdef TARGET_PC
    // 0x5A84-0x5A8C: stfs to +0x1c0/+0x1c4/+0x1c8 = CPhysicsActor::mMomentum.
    SetMomentumWR(CVector3f(0.f, 0.f, -(GetMass() * GetGravityConstant())));
#else
    SetConstantForceWR(CVector3f(0.f, 0.f, -(GetMass() * GetGravityConstant())));
#endif
    ComputeJumpVelocity(mgr);
    break;
  case kSM_Landed:
    mJumping = false;
    break;
  case kSM_Damage:
    CollisionDamage(mgr, senderId);
    mHitByPlayerProjectile = true;
    break;
  case kSM_ResistedDamage:
    mHitByPlayerProjectile = true;
    break;
  case kSM_HitObject:
    TouchDamage(mgr, senderId);
    break;
  }
}

// @ fn_34_56C8
void CIngSpaceJumpGuardian::Think(float dt, CStateManager& mgr) {
  if (GetActive()) {
    CPatterned::Think(dt, mgr);
    mCollisionManager->Update(dt, mgr, CCollisionActorManager::kUO_ObjectSpace);
    UpdateLight(dt, mgr);
    if (mAlive) {
      mSpeed = GetCurrentStruct()->locomotionSpeed;
    }
  }
}

// @ fn_34_5274
void CIngSpaceJumpGuardian::DoUserAnimEvent(CStateManager& mgr, const CInt32POINode& node,
                                            EUserEventType type, float dt) {
  bool handled = false;
  switch (type) {
  case kUE_BreakLockOn:
    if (mJumpMode != kJM_None) {
      mgr.Player(0)->SetOrbitRequestForTarget(GetUniqueId(), CPlayer::kOR_ActivateOrbitSource, mgr);
    }
    handled = true;
    break;
  case kUE_DamageOff:
    mDamageable = false;
    handled = true;
    break;
  case kUE_DamageOn:
    mDamageable = true;
    handled = true;
    break;
  case kUE_Projectile:
    if (mMiniPortalEffect && mPortalIndex >= 0 && mPortalIndex < mPortalCount) {
      CTransform4f xf = GetTransform();
      xf.SetTranslation(mPortalPositions[mPortalIndex++]);
      CDamageInfo damage(mMiniPortalDamage);
      damage.MultiplyDamage(dt);
      damage.SetNoImmunity(true);
      const CIngMiniPortalInfo portalInfo(mPlayerId, 1.f, 2.f, *mMiniPortalEffect,
                                          mData.sound_MiniPortal, 150.f, 1.f, damage,
                                          mData.miniPortalBeamInfo);
      CIngMiniPortalAttack* portal = rs_new CIngMiniPortalAttack(
          mgr.AllocateUniqueId(), rstl::string_l("Ing Mini Portal Attack"),
          CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, true), xf, GetUniqueId(),
          GetModelData()->GetScale(), portalInfo);
      mgr.AddObject(portal);
    }
    handled = true;
    break;
  case kUE_ScreenShake:
    CameraShake(mgr, node.GetLocatorName());
    break;
  case kUE_EventStart:
#ifdef TARGET_PC
    // 0x5578 addi r5, r30, 0x30: the node's locator name goes to SpawnShockWave.
    SpawnShockWave(mgr, node.GetLocatorName());
#else
    SpawnShockWave(mgr);
#endif
    break;
  case kUE_EffectOn:
    AnimationData()->SetEffectState(rstl::string_l(skEyesName), true, mgr);
    handled = true;
    break;
  case kUE_EffectOff:
    AnimationData()->SetEffectState(rstl::string_l(skEyesName), false, mgr);
    handled = true;
    break;
  case kUE_Activate:
    mBlobActive = true;
    handled = true;
    break;
  }
  if (!handled) {
    CPatterned::DoUserAnimEvent(mgr, node, type, dt);
  }
}

// @ fn_34_5248
void CIngSpaceJumpGuardian::Render(const CStateManager& mgr) const {
  if (mRender) {
    CPatterned::Render(mgr);
  }
}

// @ fn_34_5228
void CIngSpaceJumpGuardian::PreRender(CStateManager& mgr) { CPatterned::PreRender(mgr); }

// @ fn_34_5208
void CIngSpaceJumpGuardian::AddToRenderer(const CStateManager& mgr) const {
  CPatterned::AddToRenderer(mgr);
}

// @ fn_34_51E8
const CDamageVulnerability* CIngSpaceJumpGuardian::GetDamageVulnerability() const {
  return &CDamageVulnerability::PassThroughVulnerabilty();
}

// @ fn_34_500C
CVector3f CIngSpaceJumpGuardian::GetAimPosition(const CStateManager& mgr, float dt) const {
  CVector3f position = CVector3f::Zero();
  if (dt > 0.f) {
    position = PredictMotion(dt).GetTranslation();
  }
  if (mCollarSeg != 0xff) {
    const CTransform4f xf =
        GetTransform() * GetAnimationData()->GetLocatorTransform(mCollarSeg, nullptr);
    const CVector3f& scale = GetModelData()->GetScale();
    return position + CVector3f(scale.GetX() * xf.GetTranslation().GetX(),
                                scale.GetY() * xf.GetTranslation().GetY(),
                                scale.GetZ() * xf.GetTranslation().GetZ());
  }
  return CPatterned::GetAimPosition(mgr, dt);
}

// @ fn_34_4F98 forced (noise)
bool CIngSpaceJumpGuardian::Listen(CStateManager& mgr, const CVector3f& position,
                                   EListenNoiseType type) {
  bool heard = false;
  if (mAlive) {
    switch (type) {
    case 4: {
      const CVector3f delta = position - GetTranslation();
      if (delta.MagSquared() < 1600.f) {
        mHeardNoise = true;
        heard = true;
      }
      break;
    }
    }
  }
  return heard;
}

// @ fn_34_4F0C
void CIngSpaceJumpGuardian::SetupStateMachine(CStateManager& mgr) {
  StateMachine* stateMachine = mStateMachine.get();
  CPatterned::SetupStateMachine(mgr);
  stateMachine->SetTriggerFunctions(skTriggers, 14);
  stateMachine->SetStateFunctions(skStates, 9);
  stateMachine->SetCodeFunctions(skCodeFuncs, 9);
}

// @ fn_34_4EF8
bool CIngSpaceJumpGuardian::StateOver(CStateManager& mgr, const CTriggerData& data) const {
  return mAnimationState.GetState() == CAnimationState::kAS_Over;
}

// @ fn_34_4EA0
bool CIngSpaceJumpGuardian::HasIntroPattern(CStateManager& mgr, const CTriggerData& data) const {
  return GetConnectedObject(mgr, kSS_Attack, kSM_Follow) != kInvalidUniqueId;
}

// @ fn_34_4E84
bool CIngSpaceJumpGuardian::IntroPatternOver(CStateManager& mgr, const CTriggerData& data) const {
  return mWaypointNavigation.GetDestination() == kInvalidUniqueId;
}

// @ fn_34_4E18
bool CIngSpaceJumpGuardian::ShouldJump(CStateManager& mgr, const CTriggerData& data) const {
  if (mJumpTarget != mWaypointNavigation.GetLastDestination()) {
    const CScriptAIWaypoint* waypoint = TCastToConstPtr< CScriptAIWaypoint >(
        mgr.GetObjectById(mWaypointNavigation.GetLastDestination()));
    bool result = false;
    if (waypoint != nullptr) {
      if (waypoint->GetFlags() & 2) {
        result = true;
      }
    }
    return result;
  }
  return false;
}

// @ fn_34_4DF8
bool CIngSpaceJumpGuardian::HasJumpTarget(CStateManager& mgr, const CTriggerData& data) const {
  return mJumpTarget != kInvalidUniqueId;
}

// @ fn_34_4D48
bool CIngSpaceJumpGuardian::IsFacingTarget(CStateManager& mgr, const CTriggerData& data) const {
  if (const CActor* target = static_cast< const CActor* >(mgr.GetObjectById(mFaceTarget))) {
    const CVector3f delta = target->GetTranslation() - GetTranslation();
    const CVector3f direction(delta.GetX(), delta.GetY(), 0.f);
    return CVector3f::GetAngleDiff(direction, GetTransform().GetForward()) < 20.f * 0.017453292f;
  }
  return true;
}

// @ fn_34_4C84
bool CIngSpaceJumpGuardian::IsFacingWaypoint(CStateManager& mgr, const CTriggerData& data) const {
  if (!mWaypointNavigation.IsInPosition()) {
    if (const CScriptWaypoint* waypoint = TCastToConstPtr< CScriptWaypoint >(
            mgr.GetObjectById(mWaypointNavigation.GetDestination()))) {
      const CVector3f delta = waypoint->GetTranslation() - GetTranslation();
      const CVector3f direction(delta.GetX(), delta.GetY(), 0.f);
      return CVector3f::GetAngleDiff(direction, GetTransform().GetForward()) < 60.f * 0.017453292f;
    }
  }
  return true;
}

// @ fn_34_4C70 forced (trigcall)
bool CIngSpaceJumpGuardian::ShouldTaunt(CStateManager& mgr, const CTriggerData& data) const {
  return mAttackAction == kAA_Taunt;
}

// @ fn_34_4C5C
bool CIngSpaceJumpGuardian::ShouldMiniPortalAttack(CStateManager& mgr,
                                                   const CTriggerData& data) const {
  return mAttackAction == kAA_MiniPortal;
}

// @ fn_34_4C48
bool CIngSpaceJumpGuardian::FoundMiniPortalAttack(CStateManager& mgr,
                                                  const CTriggerData& data) const {
  return mPortalCount != 0;
}

// @ fn_34_4C38
bool CIngSpaceJumpGuardian::SkipToNextJumpPoint(CStateManager& mgr,
                                                const CTriggerData& data) const {
  return mAttackAction == kAA_Jump;
}

// @ fn_34_4BF0 forced (trigcall)
bool CIngSpaceJumpGuardian::PathOver(CStateManager& mgr, const CTriggerData& data) const {
  bool result = false;
  if (!mHasJumpPath || CPatterned::PathOver(mgr, data)) {
    result = true;
  }
  return result;
}

// @ fn_34_4BA8
bool CIngSpaceJumpGuardian::PathShagged(CStateManager& mgr, const CTriggerData& data) const {
  return mHeardNoise || CPatterned::PathShagged(mgr, data);
}

// @ fn_34_4B38
bool CIngSpaceJumpGuardian::PathExists(CStateManager& mgr, const CTriggerData& data) const {
  return mPathFindSearch.PathExists(GetTranslation() + CVector3f::Up(),
                                    mPathFindNavigation.GetDestinationPosition()) ==
         CPathFindSearch::kR_Success;
}

// @ fn_34_4B34
void CIngSpaceJumpGuardian::Start(CStateManager& mgr, EStateMsg msg, float dt) {}

// @ fn_34_4864
void CIngSpaceJumpGuardian::Dead(CStateManager& mgr, EStateMsg msg, float dt) {
  CPatterned::Dead(mgr, msg, dt);
  switch (msg) {
  case kStateMsg_Activate:
    mSpeed = mDefaultSpeed;
    SpawnBlobEffect(mgr, TLockedToken< CGenDescription >(
                             gpSimplePool->GetObj(SObjectTag('PART', mData.ingSpotBlobEffect))));
    mDeathTimer = 2.f;
    break;
  case kStateMsg_Update:
    UpdateBlob(mgr, dt);
    if (BodyController()->GetCurrentStateId() == pas::kAS_Death) {
      if (mgr.GetBossId() == GetUniqueId()) {
        mgr.SetBossParams(kInvalidUniqueId, 0.f, 0);
      }
      const CVector3f scale = GetModelData()->GetScale();
      const float newZ = scale.GetZ() - dt / 0.75f;
      ModelData()->SetScale(CVector3f(scale.GetX(), scale.GetY(), CMath::Max(0.f, newZ)));
      if (newZ <= 0.f) {
        mDeathTimer -= dt;
        mRender = false;
        mBlobActive = false;
      }
      if (mDeathTimer <= 0.f) {
        DeathDelete(mgr);
      }
    }
    break;
  }
}

// @ fn_34_46D0
void CIngSpaceJumpGuardian::FollowIntroPattern(CStateManager& mgr, EStateMsg msg, float dt) {
  mWaypointNavigation.Patrol(mgr, msg, dt, *this);
  switch (msg) {
  case kStateMsg_Activate: {
    if (mIntroWaypoint == kInvalidUniqueId) {
      mIntroWaypoint = GetConnectedObject(mgr, kSS_Attack, kSM_Follow);
    }
    mWaypointNavigation.SetDestination(mIntroWaypoint);
    if (const CScriptWaypoint* waypoint =
            TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(mIntroWaypoint))) {
      if (CVector3f::Dot(waypoint->GetTranslation() - GetTranslation(),
                         GetTransform().GetForward()) <= 0.f) {
        mWaypointNavigation.SetInPosition(true);
      }
    }
    const float runSpeed = BodyController()->GetBodyStateInfo().GetLocomotionSpeed(pas::kLA_Run);
    const float walkSpeed = BodyController()->GetBodyStateInfo().GetLocomotionSpeed(pas::kLA_Walk);
    const float ratio = walkSpeed / runSpeed;
    BodyController()->CommandMgr().SetSteeringBlendMode(kSBM_FullSpeed);
    BodyController()->CommandMgr().SetSteeringSpeedRange(ratio, ratio);
    break;
  }
  case kStateMsg_Deactivate:
    mIntroWaypoint = mWaypointNavigation.GetDestination();
    break;
  }
}

// @ fn_34_4574
void CIngSpaceJumpGuardian::FaceTarget(CStateManager& mgr, EStateMsg msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate:
    mAnimationState.SetState(CAnimationState::kAS_Ready);
    break;
  case kStateMsg_Update:
    if (mAnimationState.CanIssueCommand(*BodyController(), pas::kAS_Turn)) {
      if (const CActor* target = static_cast< const CActor* >(mgr.GetObjectById(mFaceTarget))) {
        const CVector3f direction = target->GetTranslation() - GetTranslation();
        if (direction.IsMagnitudeSafe()) {
          BodyController()->CommandMgr().DeliverCmd(
              CBCLocomotionCmd(CVector3f::Zero(), direction.AsNormalized(), 1.f));
        } else {
          mAnimationState.SetState(CAnimationState::kAS_Over);
        }
      } else {
        mAnimationState.SetState(CAnimationState::kAS_Over);
      }
    }
    break;
  case kStateMsg_Deactivate:
    mAnimationState.SetState(CAnimationState::kAS_NotReady);
    break;
  }
}

// @ fn_34_3DA8
void CIngSpaceJumpGuardian::Jump(CStateManager& mgr, EStateMsg msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate:
    mAnimationState.SetState(CAnimationState::kAS_Ready);
    Stop();
    mJumpMode = TCastToConstPtr< CScriptAIWaypoint >(mgr.GetObjectById(mJumpTarget)) != nullptr
                    ? kJM_Waypoint
                    : kJM_Location;
    mJumpTimer = 0.f;
    mShieldTimer = 1.f;
    mJumpSfx = CSfxHandle();
    AnimationData()->SetEffectState(rstl::string_l(skShieldName), true, mgr);
    SetCollisionVulnerability(mgr, CDamageVulnerability::ReflectVulnerabilty());
    break;
  case kStateMsg_Update:
    if (mAnimationState.CanIssueCommand(*BodyController(), pas::kAS_Jump)) {
      if (const CActor* target = static_cast< const CActor* >(mgr.GetObjectById(mJumpTarget))) {
        pas::EJumpType type = pas::kJT_Normal;
        if (mJumpMode == kJM_Waypoint) {
          type = mLastJumpToWaypoint ? pas::kJT_Three : pas::kJT_Ambush;
        }
        BodyController()->CommandMgr().DeliverCmd(CBCJumpCmd(
            target->GetTranslation(), type, pas::kJS_IntoJump, 0, CBCJumpCmd::kFF_AmbushJump));
      } else {
        mAnimationState.SetState(CAnimationState::kAS_Over);
      }
    } else if (!mJumpSfx && mJumping) {
      mJumpTimer += dt;
      CMaterialFilter filter = GetMaterialFilter();
      if (mJumpTimer > mJumpDuration - 0.5f) {
        mJumpSfx = CSfxManager::AddEmitter(static_cast< ushort >(mData.sound), GetTranslation(),
                                           127, GetCurrentAreaId().Value(), false, false,
                                           CSfxManager::kMedPriority);
        filter.ExcludeList().Remove(
            CMaterialList(kMT_Wall, kMT_Floor, kMT_Platform, kMT_Player, kMT_Immovable));
      } else {
        filter.ExcludeList().Add(
            CMaterialList(kMT_Wall, kMT_Floor, kMT_Platform, kMT_Player, kMT_Immovable));
      }
      SetMaterialFilter(filter);
    } else if (mJumpSfx) {
      CSfxManager::UpdateEmitter(mJumpSfx, GetTranslation(), GetTransform().GetForward(), 127);
    }
    if (mJumping) {
      mShieldTimer -= dt;
      if (mShieldTimer <= 0.f) {
        AnimationData()->SetEffectState(rstl::string_l(skShieldName), false, mgr);
        SetCollisionVulnerability(mgr, *CPatterned::GetDamageVulnerability());
      }
    }
    break;
  case kStateMsg_Deactivate: {
    mAnimationState.SetState(CAnimationState::kAS_NotReady);
    mLastJumpToWaypoint = mJumpMode == kJM_Waypoint;
    mJumpMode = kJM_None;
    mJumpSfx = CSfxHandle();
    mDamageable = true;
    AnimationData()->SetEffectState(rstl::string_l(skShieldName), false, mgr);
    SetCollisionVulnerability(mgr, *CPatterned::GetDamageVulnerability());
    CMaterialFilter filter = GetMaterialFilter();
    filter.ExcludeList().Remove(
        CMaterialList(kMT_Wall, kMT_Floor, kMT_Platform, kMT_Player, kMT_Immovable));
    SetMaterialFilter(filter);
    Stop();
    break;
  }
  }
}

// @ fn_34_3C6C
void CIngSpaceJumpGuardian::Taunt(CStateManager& mgr, EStateMsg msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate:
    mAnimationState.SetState(CAnimationState::kAS_Ready);
    break;
  case kStateMsg_Update:
    if (mAnimationState.CanIssueCommand(*BodyController(), pas::kAS_Taunt)) {
      BodyController()->CommandMgr().DeliverCmd(CBCTauntCmd(pas::kTT_Two));
    }
    break;
  case kStateMsg_Deactivate:
    mAnimationState.SetState(CAnimationState::kAS_NotReady);
    break;
  }
}

// @ fn_34_3A3C
void CIngSpaceJumpGuardian::MiniPortalAttack(CStateManager& mgr, EStateMsg msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate:
    mAnimationState.SetState(CAnimationState::kAS_Ready);
    mPortalIndex = 0;
    break;
  case kStateMsg_Update:
    if (mAnimationState.CanIssueCommand(*BodyController(), pas::kAS_ProjectileAttack)) {
      BodyController()->CommandMgr().DeliverCmd(CBCProjectileAttackCmd(
          pas::kS_One, GetTranslation() + GetTransform().GetForward(), false));
    }
    break;
  case kStateMsg_Deactivate:
    mAnimationState.SetState(CAnimationState::kAS_NotReady);
    if (BodyController()->GetCurrentStateId() == pas::kAS_ProjectileAttack) {
      CBodyStateCmd cmd(kBSC_AbortScripted);
      BodyController()->CommandMgr().DeliverCmd(cmd);
    }
    mPortalIndex = -1;
    break;
  }
}

// @ fn_34_3888
void CIngSpaceJumpGuardian::PathFind(CStateManager& mgr, EStateMsg msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate:
    BodyController()->CommandMgr().SetSteeringBlendMode(kSBM_FullSpeed);
    BodyController()->CommandMgr().SetSteeringSpeedRange(1.f, 1.f);
    if (mHasJumpPath) {
      mPathFindNavigation.PathFind(mgr, msg, dt, *this);
    }
    mHeardNoise = false;
    break;
  case kStateMsg_Update:
    if (mHasJumpPath) {
      mPathFindNavigation.PathFind(mgr, msg, dt, *this);
    }
    break;
  }
  if (!mPathFindSearch.IsOver()) {
    const CVector3f moveVec = BodyController()->CommandMgr().GetMoveVector();
    if (CVector3f::Dot(GetTransform().GetForward(), moveVec) < 0.f && moveVec.IsMagnitudeSafe()) {
      BodyController()->CommandMgr().ClearLocomotionCmds();
      BodyController()->CommandMgr().DeliverCmd(
          CBCLocomotionCmd(CVector3f::Zero(), moveVec.AsNormalized(), 1.f));
    }
  }
}

// @ fn_34_36B8
void CIngSpaceJumpGuardian::SteerToDest(CStateManager& mgr, EStateMsg msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate: {
    mAnimationState.SetState(CAnimationState::kAS_Ready);
    const float runSpeed = BodyController()->GetBodyStateInfo().GetLocomotionSpeed(pas::kLA_Run);
    const float walkSpeed = BodyController()->GetBodyStateInfo().GetLocomotionSpeed(pas::kLA_Walk);
    const float ratio = walkSpeed / runSpeed;
    BodyController()->CommandMgr().SetSteeringBlendMode(kSBM_FullSpeed);
    BodyController()->CommandMgr().SetSteeringSpeedRange(ratio, ratio);
    break;
  }
  case kStateMsg_Update:
    if (mgr.GetObjectById(mJumpTarget) != nullptr) {
      const CVector3f delta = mPathFindNavigation.GetDestinationPosition() - GetTranslation();
      if (delta.IsMagnitudeSafe() && CVector3f::Dot(delta, GetTransform().GetForward()) > 0.f) {
        BodyController()->CommandMgr().DeliverCmd(
            CBCLocomotionCmd(CVector3f::Zero(), delta.AsNormalized(), 1.f));
      } else {
        mAnimationState.SetState(CAnimationState::kAS_Over);
      }
    } else {
      mAnimationState.SetState(CAnimationState::kAS_Over);
    }
    break;
  case kStateMsg_Deactivate:
    mAnimationState.SetState(CAnimationState::kAS_NotReady);
    break;
  }
}

// @ fn_34_3624
void CIngSpaceJumpGuardian::EnableEnergyBar(CStateManager& mgr, float dt) {
  mgr.SetBossParams(GetUniqueId(), GetHealthInfo()->GetInitialHP(),
                    gpStringTable->GetStringIndex("BossSpaceJumpGuardian"));
}

// @ fn_34_3614
void CIngSpaceJumpGuardian::SelectTarget(CStateManager& mgr, float dt) {
  mPlayerId = mgr.GetPlayer(0)->GetUniqueId();
}

// @ fn_34_34CC
void CIngSpaceJumpGuardian::SelectJumpTarget(CStateManager& mgr, float dt) {
  const rstl::vector< TUniqueId > points = FindJumpPoints(mgr);
  const rstl::vector< TUniqueId > candidates = FilterJumpPoints(mgr, points);
  mLastJumpPoint = kInvalidUniqueId;
  mJumpTarget = kInvalidUniqueId;
  mHasJumpPath = false;
  if (!candidates.empty()) {
    const int index = mgr.Random()->Range(0, static_cast< int >(candidates.size()) - 1);
    if (const CScriptAiJumpPoint* jumpPoint =
            static_cast< const CScriptAiJumpPoint* >(mgr.GetObjectById(candidates[index]))) {
      mLastJumpPoint = candidates[index];
#ifdef TARGET_PC
      // 0x35BC lhz r0, 0x180(r7): the jump point's +0x180 (GetJumpPoint), not +0x182.
      mJumpTarget = jumpPoint->GetJumpPoint();
#else
      mJumpTarget = jumpPoint->GetJumpTarget();
#endif
      mHasJumpPath = true;
      mPathFindNavigation.SetDestination(jumpPoint->GetTranslation());
      mPathFindNavigation.SetFaceTarget(kInvalidUniqueId);
    }
  }
}

// @ fn_34_342C
void CIngSpaceJumpGuardian::SelectAttackAction(CStateManager& mgr, float dt) {
  const SLdrIngSpaceJumpGuardianStruct* current = GetCurrentStruct();
  const float roll = mgr.Random()->Range(0.f, 100.f);
  if (roll <= current->tauntChance) {
    mAttackAction = kAA_Taunt;
  } else if (roll <= current->tauntChance + current->attackChance) {
    mAttackAction = kAA_MiniPortal;
  } else {
    mAttackAction = kAA_Jump;
  }
}

// @ fn_34_30E4 forced (codecall)
void CIngSpaceJumpGuardian::FindBestMiniPortals(CStateManager& mgr, float dt) {
  mPortalCount = 0;
  if (const CActor* target = static_cast< const CActor* >(mgr.GetObjectById(mPlayerId))) {
    const CVector3f aim = target->GetAimPosition(mgr, 0.f);
    const CVector3f origin = GetLctrTransform(mHeadSeg).GetTranslation();
    const CVector3f& scale = GetModelData()->GetScale();
    const CVector3f start = origin + scale.GetY() * (3.f * GetTransform().GetForward());
    const CMaterialFilter filter = CMaterialFilter::MakeIncludeExclude(
        CMaterialList(kMT_Solid),
        CMaterialList(kMT_Character, kMT_Player, kMT_CollisionActor, kMT_ProjectilePassthrough));
    for (int i = 0; i < 8; ++i) {
      if (mPortalCount >= 3) {
        break;
      }
      const float angle = skPortalAngles[i] * 0.017453292f;
      const float c = CMath::FastCosR(angle);
      const float s = CMath::FastSinR(angle);
      const CVector3f offset =
          2.f * CVector3f(scale.GetX() * c, scale.GetY() * 0.f, scale.GetZ() * s);
      const CVector3f end = start + GetTransform().Rotate(offset);
      if (mgr.RayCollideWorld(end, aim, filter, this)) {
        mPortalPositions[mPortalCount++] = end;
      }
    }
  }
}

// @ fn_34_30A8
void CIngSpaceJumpGuardian::SetIntroJumpTarget(CStateManager& mgr, float dt) {
  mJumpTarget = mWaypointNavigation.GetDestination();
  mJumpApexHeight = 10.f;
  BodyController()->CommandMgr().Reset();
}

// @ fn_34_309C
void CIngSpaceJumpGuardian::SetFaceJumpTarget(CStateManager& mgr, float dt) {
  mFaceTarget = mJumpTarget;
}

// @ fn_34_3090
void CIngSpaceJumpGuardian::SetFaceAttackTarget(CStateManager& mgr, float dt) {
  mFaceTarget = mPlayerId;
}

// @ fn_34_3084
void CIngSpaceJumpGuardian::SetFaceWaypoint(CStateManager& mgr, float dt) {
  mFaceTarget = mWaypointNavigation.GetDestination();
}

// @ fn_34_2ED0
void CIngSpaceJumpGuardian::ComputeJumpVelocity(CStateManager& mgr) {
  if (!mJumping && mJumpTarget != kInvalidUniqueId) {
    if (const CActor* target = static_cast< const CActor* >(mgr.GetObjectById(mJumpTarget))) {
      CVector3f velocity = CVector3f::Zero();
      const float dx = target->GetTranslation().GetX() - GetTranslation().GetX();
      const float dy = target->GetTranslation().GetY() - GetTranslation().GetY();
      const float startZ = GetTranslation().GetZ();
      const float targetZ = target->GetTranslation().GetZ();
      const float gravity = GetGravityConstant();
#ifdef TARGET_PC
      // 0x2FA8 fcmpo f28, f27 / bge: plain bge keeps startZ on NaN.
      const float apex = mJumpApexHeight + (startZ < targetZ ? targetZ : startZ);
#else
      const float apex = mJumpApexHeight + (startZ >= targetZ ? startZ : targetZ);
#endif
      const float vz = CMath::SqrtF(2.f * gravity * (apex - startZ));
      velocity.SetZ(vz);
      const float time = vz / gravity + CMath::SqrtF(2.f * (apex - targetZ) / gravity);
      const float inverseTime = 1.f / time;
      velocity.SetX(inverseTime * dx);
      velocity.SetY(inverseTime * dy);
      SetVelocityWR(velocity);
      mJumping = true;
      mJumpDuration = time;
    }
  }
}

// @ fn_34_29EC
void CIngSpaceJumpGuardian::SetupCollision(CStateManager& mgr) {
  rstl::vector< CJointCollisionDescription > descriptions;
  descriptions.reserve(7);
  for (int i = 0; i < 7; ++i) {
    descriptions.push_back(CJointCollisionDescription::SphereCollision(
        GetAnimationData()->GetLocatorSegId(rstl::string_l(skJointSpheres[i].mName)),
        CVector3f::Zero(), skJointSpheres[i].mRadius, rstl::string_l(skJointSpheres[i].mName),
        1000.f));
  }
  mCollisionManager =
      rs_new CCollisionActorManager(mgr, GetUniqueId(), GetCurrentAreaId(), descriptions, true);
  UpdateCollisionVulnerabilities(mgr);
  SetMaterialFilter(CMaterialFilter::MakeIncludeExclude(
      CMaterialList(kMT_Solid), CMaterialList(kMT_CollisionActor, kMT_AIPassthrough, kMT_Player)));
}

// @ fn_34_2874
void CIngSpaceJumpGuardian::UpdateCollisionVulnerabilities(CStateManager& mgr) {
  const CHealthInfo health = *GetHealthInfo();
  for (uint i = 0; i < mCollisionManager->GetNumCollisionActors(); ++i) {
    const CJointCollisionDescription& desc = mCollisionManager->GetCollisionDescFromIndex(i);
    if (CCollisionActor* colAct =
            TCastToPtr< CCollisionActor >(mgr.ObjectById(desc.GetCollisionActorId()))) {
      colAct->SetDamageVulnerability(*CPatterned::GetDamageVulnerability());
      *colAct->HealthInfo() = health;
    }
  }
}

// @ fn_34_2744
void CIngSpaceJumpGuardian::TouchDamage(CStateManager& mgr, TUniqueId senderId) {
  if (const CCollisionActor* colAct =
          TCastToConstPtr< CCollisionActor >(mgr.GetObjectById(senderId))) {
    const TUniqueId touched = colAct->GetLastTouchedObject();
    const CPlayer* player = mgr.GetPlayer(0);
    if (touched == player->GetUniqueId() && mCurDamageRemTime <= 0.f) {
      mgr.ApplyDamage(
          GetUniqueId(), player->GetUniqueId(), GetUniqueId(), GetContactDamage(),
          CMaterialFilter::MakeIncludeExclude(CMaterialList(kMT_Solid), CMaterialList()),
          CVector3f::Zero());
      mCurDamageRemTime = mDamageWaitTime;
    }
  }
}

// @ fn_34_2534
void CIngSpaceJumpGuardian::CollisionDamage(CStateManager& mgr, TUniqueId senderId) {
  if (mAlive) {
    if (CCollisionActor* colAct = TCastToPtr< CCollisionActor >(mgr.ObjectById(senderId))) {
      const TUniqueId touched = colAct->GetLastTouchedObject();
      CHealthInfo* colHealth = colAct->HealthInfo();
      CHealthInfo* health = HealthInfo();
      const float initialHP = health->GetInitialHP();
      const float hp = health->GetHP();
      if (const CWeapon* weapon = TCastToConstPtr< CWeapon >(mgr.GetObjectById(touched))) {
        const float damage = initialHP - colHealth->GetHP();
#ifdef TARGET_PC
        // 0x25F8-0x2624 lfs 0x28/0x38/0x48(r29): the weapon transform's forward column.
        CVector3f position = weapon->GetTransform().GetForward();
#else
        CVector3f position = weapon->GetTranslation();
#endif
        TakeDamage(position, damage);
        if (hp > damage || (mDamageable && mLastJumpToWaypoint)) {
          health->SetHP(hp - damage);
        }
        if (hp <= 0.f) {
          Death(mgr, position, kSS_DeathRattle);
          mgr.RecordDamageSource(*this, weapon->GetUniqueId(), weapon->GetCurrentDamageInfo(), true,
                                 false);
        }
#ifdef TARGET_PC
        // 0x26BC lbz 0xa34 / extrwi. r0, r0, 1, 28: bit 0x08 = mDamageable.
        if (mDamageable) {
#else
        if (mLastJumpToWaypoint) {
#endif
          CKnockBackInfo knockBack(position, weapon->GetOwnerId(), touched,
                                   weapon->GetCurrentDamageInfo(), true);
          KnockBack(mgr, knockBack);
        }
      }
      colHealth->SetHP(initialHP);
    }
  }
}

// @ fn_34_2498
void CIngSpaceJumpGuardian::SetCollisionVulnerability(CStateManager& mgr,
                                                      const CDamageVulnerability& vulnerability) {
  for (uint i = 0; i < mCollisionManager->GetNumCollisionActors(); ++i) {
    const CJointCollisionDescription& desc = mCollisionManager->GetCollisionDescFromIndex(i);
    if (CCollisionActor* colAct =
            TCastToPtr< CCollisionActor >(mgr.ObjectById(desc.GetCollisionActorId()))) {
      colAct->SetDamageVulnerability(vulnerability);
    }
  }
}

// @ fn_34_2324
void CIngSpaceJumpGuardian::CreateLight(CStateManager& mgr) {
  AnimationData()->SetEffectState(rstl::string_l(skEyesName), true, mgr);
  mLightId = mgr.AllocateUniqueId();
  CGameLight* light =
      rs_new CGameLight(mLightId, GetAreaIdForPersistence(), GetActive(),
                        rstl::string_l("Ing Light"), CTransform4f::Identity(), GetUniqueId(),
                        CLight::BuildPoint(CVector3f::Zero(), CColor::Black()), 0, 0, 0.f);
  mgr.AddObject(light);
}

// @ fn_34_216C
void CIngSpaceJumpGuardian::UpdateLight(float dt, CStateManager& mgr) {
  if (mAlive) {
    mLightIntensity = CMath::Min(1.f, mLightIntensity + dt);
  }
  if (CGameLight* light = TCastToPtr< CGameLight >(mgr.ObjectById(mLightId))) {
    const CColor color = CColor::Lerp(CColor::Black(), mData.lightColor, mLightIntensity);
    const CVector3f position = GetLctrTransform(mHeadSeg).GetTranslation();
    CLight lightData = CLight::BuildPoint(position, color);
#ifdef TARGET_PC
    // Retail 0x2238-0x2258 (fn_34_216C): constant 0, 1 / [0x81C], 0 (the linear term).
    lightData.SetAttenuation(0.f, 1.f / mData.lightAttenuation, 0.f);
#else
    lightData.SetAttenuation(1.f / mData.lightAttenuation, 0.f, 0.f);
#endif
    light->SetLight(lightData);
  }
}

// @ fn_34_1BFC
void CIngSpaceJumpGuardian::CameraShake(CStateManager& mgr, const rstl::string& locatorName) {
#ifdef TARGET_PC
  // 0x1C60 bl FindConnectedObject__7CEntity: first matching connection, no RNG draw.
  const TUniqueId shakerId =
      FindConnectedObject(mgr, mJumpMode == kJM_Waypoint ? kSS_Retreat : kSS_Play, kSM_Attach);
#else
  const TUniqueId shakerId =
      GetConnectedObject(mgr, mJumpMode == kJM_Waypoint ? kSS_Retreat : kSS_Play, kSM_Attach);
#endif
  if (const CScriptCameraShaker* shaker =
          TCastToConstPtr< CScriptCameraShaker >(mgr.ObjectById(shakerId))) {
    CCameraShakerData data = shaker->GetShakeData();
    data.SetPosition(GetLctrTransform(locatorName).GetTranslation());
    mgr.CameraManager(0)->CameraShakerManager()->AddCameraShaker(data, mgr, false, false);
  }
}

void CIngSpaceJumpGuardian::SpawnShockWave(CStateManager& mgr) {
  if (mJumpMode == kJM_Waypoint) {
    const CVector3f origin = GetLctrTransform(rstl::string_l("Collar_SDK")).GetTranslation();
    const CShockWaveInfo shockWaveInfo(mData.shockWaveInfo);
    CShockWave* shockWave = rs_new CShockWave(
        mgr.AllocateUniqueId(), rstl::string_l("Shock Wave"),
        CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, true),
        CTransform4f::Translate(origin), GetUniqueId(), shockWaveInfo, 0.f, 1.75f);
    mgr.AddObject(shockWave);
  }
}
#ifdef TARGET_PC

// 0x1928-0x1930: retail passes the caller's locator name (r5) to GetLctrTransform.
// 0x1A0C/0x1A10 lfs f2 rodata_68 (0.0), lfs f1 rodata_B4 (1.75): fn_80171970 gets
// minActiveTime=1.75, knockback=0.0. The overload above has them swapped, so its wave never
// expires (CShockWave::Think deletes only when minActiveTime > 0).
// @ fn_34_1904
void CIngSpaceJumpGuardian::SpawnShockWave(CStateManager& mgr, const rstl::string& locatorName) {
  if (mJumpMode == kJM_Waypoint) {
    const CVector3f origin = GetLctrTransform(locatorName).GetTranslation();
    const CShockWaveInfo shockWaveInfo(mData.shockWaveInfo);
    CShockWave* shockWave = rs_new CShockWave(
        mgr.AllocateUniqueId(), rstl::string_l("Shock Wave"),
        CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, true),
        CTransform4f::Translate(origin), GetUniqueId(), shockWaveInfo, 1.75f, 0.f);
    mgr.AddObject(shockWave);
  }
}
#endif

// @ fn_34_187C
const SLdrIngSpaceJumpGuardianStruct* CIngSpaceJumpGuardian::GetCurrentStruct() const {
  const float hp = GetHealthInfo()->GetHP();
  if (hp <= mData.ingSpaceJumpGuardianStruct_0xd0db5f7a.unknown_0x3e370622) {
    return &mData.ingSpaceJumpGuardianStruct_0xd0db5f7a;
  }
  if (hp <= mData.ingSpaceJumpGuardianStruct_0xf223aa76.unknown_0x3e370622) {
    return &mData.ingSpaceJumpGuardianStruct_0xf223aa76;
  }
  if (hp <= mData.ingSpaceJumpGuardianStruct_0x6b08e2e5.unknown_0x3e370622) {
    return &mData.ingSpaceJumpGuardianStruct_0x6b08e2e5;
  }
  return &mData.ingSpaceJumpGuardianStruct;
}

// @ fn_34_1660
rstl::vector< TUniqueId >
CIngSpaceJumpGuardian::FilterJumpPoints(CStateManager& mgr,
                                        const rstl::vector< TUniqueId >& points) {
  if (!points.empty()) {
    rstl::vector< TUniqueId > waypointLinked;
    rstl::vector< TUniqueId > others;
    waypointLinked.reserve(points.size());
    others.reserve(points.size());
    for (rstl::vector< TUniqueId >::const_iterator it = points.begin(); it != points.end(); ++it) {
      if (const CScriptAiJumpPoint* jumpPoint =
              static_cast< const CScriptAiJumpPoint* >(mgr.GetObjectById(*it))) {
        if (const CScriptWaypoint* waypoint =
#ifdef TARGET_PC
                // 0x16F0 lhz r0, 0x180(r23): +0x180 = GetJumpPoint.
                TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(jumpPoint->GetJumpPoint()))) {
#else
                TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(jumpPoint->GetJumpTarget()))) {
#endif
          if (!mgr.GetSafeZoneManager()->PointIsInSafeZone(mgr, waypoint->GetTranslation())) {
#ifdef TARGET_PC
            // 0x172C lhz r0, 0x180(r23): +0x180 = GetJumpPoint.
            if (TCastToConstPtr< CScriptAIWaypoint >(mgr.GetObjectById(jumpPoint->GetJumpPoint()))) {
#else
            if (TCastToConstPtr< CScriptAIWaypoint >(
                    mgr.GetObjectById(jumpPoint->GetJumpTarget()))) {
#endif
              waypointLinked.push_back(*it);
            } else {
              others.push_back(*it);
            }
          }
        }
      }
    }
    const SLdrIngSpaceJumpGuardianStruct* current = GetCurrentStruct();
    if (mgr.Random()->Range(0.f, 100.f) <= current->unknown_0x03698c10 && !waypointLinked.empty()) {
      return waypointLinked;
    }
    if (!others.empty()) {
      return others;
    }
  }
  return points;
}

// @ fn_34_1198
rstl::vector< TUniqueId > CIngSpaceJumpGuardian::FindJumpPoints(CStateManager& mgr) {
  rstl::vector< TUniqueId > result;
  if (const CEntity* current = mgr.GetObjectById(mJumpTarget)) {
    result = current->FindConnectedObjects_if(mgr, kSS_Arrived, kSM_Next, CJumpPointPredicate());
  }
  if (result.empty()) {
    result.reserve(10);
    const CVector3f up = GetTranslation() + CVector3f::Up();
    const CObjectList& list = mgr.GetObjectListById(kOL_AiWaypoint);
    for (int i = list.GetFirstObjectIndex(); i != -1; i = list.GetNextObjectIndex(i)) {
      if (const CScriptAiJumpPoint* jumpPoint = TCastToConstPtr< CScriptAiJumpPoint >(list[i])) {
        if (jumpPoint->GetActive() && !jumpPoint->GetInUse(GetUniqueId()) &&
            jumpPoint->GetType() == 0 && jumpPoint->GetUniqueId() != mLastJumpPoint &&
            jumpPoint->GetCurrentAreaId() == GetCurrentAreaId()) {
          GetSearchPath()->SetFlags(1);
          if (GetSearchPath()->PathExists(up, jumpPoint->GetTranslation()) ==
              CPathFindSearch::kR_Success) {
            result.push_back(jumpPoint->GetUniqueId());
          }
          GetSearchPath()->SetFlags(0x301);
        }
      }
      if (result.size() == result.capacity()) {
        break;
      }
    }
  }
  return result;
}

// @ fn_34_1024
void CIngSpaceJumpGuardian::SpawnBlobEffect(CStateManager& mgr,
                                            const TLockedToken< CGenDescription >& desc) {
  mBlobId = mgr.AllocateUniqueId();
  CCollisionTracker* effect = rs_new CCollisionTracker(
      desc, mBlobId, GetCurrentAreaId(), true, rstl::string_l("IngBlobEffect"),
      CTransform4f::Translate(GetTranslation()), GetUniqueId(), 0,
      CCollisionTracker::skDefaultExtents);
  if (effect != nullptr) {
    effect->SetParticleEmissionRateScalar(0.f);
#ifdef TARGET_PC
    // 0x1118 lhz r0, 0x8(r30) / 0x112C sth r0, 0x12a(r29): the blob draws after its owner.
    effect->SetNextDrawNode(GetUniqueId());
#endif
    mgr.AddObject(effect);
  }
}

// @ fn_34_ED4
void CIngSpaceJumpGuardian::UpdateBlob(CStateManager& mgr, float dt) {
  if (CCollisionTracker* effect = static_cast< CCollisionTracker* >(mgr.ObjectById(mBlobId))) {
    const float intensity = mBlobActive ? 1.f : 0.f;
    if (intensity > 0.f) {
      const CVector3f position = GetTranslation();
      const CVector3f& up =
          CMath::AbsF(CVector3f::Up().GetZ() < 0.95f) != 0.f ? CVector3f::Up() : CVector3f::Right();
      effect->SetTransform(CTransform4f::LookAt(position, position + CVector3f::Up(), up));
    }
    effect->SetParticleEmissionRateScalar(intensity);
  }
}

// @ fn_34_170
CEntity* LoadIngSpaceJumpGuardian(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SLdrIngSpaceJumpGuardian sldrThis;
#include "MetroidPrime/ScriptLoader/SLdrIngSpaceJumpGuardian.inc"

  rstl::optional_object< CModelData > modelData(
      LdrToModelData(sldrThis.editorProperties.transform.scale, kInvalidAssetId,
                     sldrThis.patterned.animationInformation, true));
  if (!modelData) {
    return nullptr;
  }

  return rs_new CIngSpaceJumpGuardian(
      mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
      LdrToEntityInfo(info, sldrThis.editorProperties), LdrToTransform4f(sldrThis.editorProperties),
      *modelData, LdrToActorParameters(sldrThis.actorInformation),
      LdrToPatternedInfo(sldrThis.patterned, nullptr), sldrThis.ingSpaceJumpGuardianProperties);
}

#ifndef MONOLITHIC
// @ fn_34_140
static void SetFuncPtrs() {
  static SIngSpaceJumpGuardian_FuncPtrs funcPtrs;
  funcPtrs.mLoader = &LoadIngSpaceJumpGuardian;
  SetSIngSpaceJumpGuardian_FuncPtrs(&funcPtrs);
}

extern "C" void RELMain() { SetFuncPtrs(); }

extern "C" void RELExit() { SetSIngSpaceJumpGuardian_FuncPtrs(nullptr); }
#endif
