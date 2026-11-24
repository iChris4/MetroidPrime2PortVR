// The Boost Guardian's Ing half: the base-class overrides the Ing REL carries its own copies of.
//
// This module (IngBoostBallGuardian, REL module 30) is the Ing REL with the boost half added, so
// it carries its own copy of the Ing's code. The behaviour below is ported from the Ing host body
// (branch decomp/ing, src/MetroidPrime/Enemies/PortCIng.cpp, lines 2791..3113).
//
// Every `fn_29_` address in the comments below is the ING REL's address, not this module's: the
// names and the code are the Ing's, and the module's own addresses are in
// docs/module-analysis/BoostGuardian_fsm.tsv (and docs/module-analysis/BoostGuardian_functions.tsv
// for the rest). The function-level addresses of the Ing bodies are given in the leading comment
// of each function, in the same wording as the Ing file.
//
// The parts the Ing has and this module does not are marked "[this module]": the lure safe zone,
// the safe-zone count, the grapple block (there is no CIngGrappleData in this module's data block)
// and the mini-portal attack class. The comments of the Ing body are kept as they are.
#include "MetroidPrime/Enemies/CIngBoostBallGuardian.hpp"

#include "Collision/CMaterialFilter.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Particles/CGenDescription.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"
#include "MetroidPrime/BodyState/CBodyStateCmdMgr.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CAxisAngle.hpp"
#include "MetroidPrime/CCollisionActor.hpp"
#include "MetroidPrime/CCollisionActorManager.hpp"
#include "MetroidPrime/CExplosion.hpp"
#include "MetroidPrime/CGameLight.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "MetroidPrime/Enemies/PortCBoostBallGuardianBall.hpp"
#include "MetroidPrime/Enemies/CCollisionTracker.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CKnockBackInfo.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/Weapons/CWeapon.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "Kyoto/Math/CPlane.hpp"
#include "MetroidPrime/CSafeZoneManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Enemies/CGenericFSM2.hpp"
#include "MetroidPrime/Enemies/CIngMiniPortalAttack.hpp"
#include "MetroidPrime/Enemies/CPatternedInfo.hpp"
#include "MetroidPrime/Enemies/CTeamAiRole.hpp"
#include "MetroidPrime/Enemies/PortBoostGuardianShared.hpp"
#include "MetroidPrime/HUD/CHUDMemoParms.hpp"
#include "MetroidPrime/HUD/CSamusHud.hpp"
#include "MetroidPrime/PathFinding/CPathFindArea.hpp"
#include "MetroidPrime/Player/CEnvironmentVariable.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/SLdrIng.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "MetroidPrime/ScriptObjects/CHUDBillboardEffect.hpp"
#include "MetroidPrime/ScriptObjects/CScriptAIHint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptAIWaypoint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSafeZone.hpp"
#include "MetroidPrime/ScriptObjects/CScriptTeamAiMgr.hpp"
#include "MetroidPrime/TCastTo.hpp"

#include <stdio.h>
#include <stdlib.h>

// MP2_BG_FSM logs the state machine; read once, these run every frame.
static bool BgFsmLog() {
  static const bool enabled = getenv("MP2_BG_FSM") != nullptr;
  return enabled;
}

namespace {

// The Ing's own file-scope solid filter helper (the Ing host body's helper) is shared with the
// other files of this module: PortBoostGuardianShared.hpp has SolidFilter().

} // namespace

// The ing-spot form pulls itself onto the aligned surface: unless its material list has
// kMT_GroundCollider (0x0000DC50..0x0000DC74: the high word at 0x68 & 0x20, bit 37), a last
// surface move (0x1110) that leaves the surface plane by more than rodata_E8 (0.95 of the normal)
// eases the actor 0.6 * dt (rodata_120) of the way to 0.1 (rodata_124) above the plane; the move
// is then cleared.
// @ fn_30_DC10
void CIngBoostBallGuardian::PreThink(float dt, CStateManager& mgr) {
  EnsureBodyActive(mgr); // host guard, not in retail (see EnsureBodyActive)
  // 0x0000DC3C: CEntity 0x20 0x80; 0x0000DC48: form 3.
  if (GetActive() && mForm == kForm_IngSpot &&
      !GetMaterialList().HasMaterial(kMT_GroundCollider)) {
    const CPlane plane = mSurfaceAlignment.GetSurface().GetPlane(); // 0x0000DC80: 0xD68
    const CVector3f normal = plane.GetNormal();
    if (CVector3f::Dot(mSurfaceMoveDir, normal) < 0.95f) { // 0x0000DCBC
      const float t = 0.6f * dt;                             // 0x0000DCE4
      const CVector3f pos = GetTranslation();
      const float dist = CVector3f::Dot(normal, pos) - plane.GetConstant() - 0.1f; // 0x0000DD10
      const CVector3f onPlane = pos - normal * dist;
      SetTranslation(pos * (1.f - t) + onPlane * t); // 0x0000DD50
    }
    mSurfaceMoveDir = CVector3f::Zero(); // 0x0000DD5C
  }
  CPatterned::PreThink(dt, mgr); // 0x0000DD7C
}

// The module's own Think. The form picks the base think (the host forms, the ing-spot forms'
// fn_30_55D8, or CPatterned's), then the managers, the line of sight, the timers, the surface
// alignment, the puddle effects, the target, the loop sound, the touch box, the hit light, the
// actor behind 0x1092 and the phase advance (0x0000DB18): the damage taken (1 - hp / initial hp)
// picks one of the three phases (the count at 0xF2C) in equal steps, and a new phase sets the
// speed (0x3C8) to its locomotion speed scale (+0x30).
// @ fn_30_D94C
void CIngBoostBallGuardian::Think(float dt, CStateManager& mgr) {
  // The probe's own activation: the Boost Guardian is an inactive actor until the fight's script
  // starts it (the console's `objs` lists it as inactive), so the probe wakes it up the way the
  // Sand Boss probe does. Without this the state machine is never set up and the probe's dump
  // never runs.
  if (BgFsmLog() && !GetActive()) {
    mgr.SendScriptMsg(this, GetUniqueId(), kSM_Activate);
  }
  if (!GetActive()) {
    return;
  }
  // [port] the host body is activated before the first think (see EnsureBodyActive).
  EnsureBodyActive(mgr);
  switch (mForm) {
  case kForm_Host:
  case kForm_ControllingHost:
    // 0x0000D9B0: the host forms run only the actor and the state machine.
    CActor::Think(dt, mgr);
    mStateMachine->Update(mgr, *this, dt);
    SetDrawShadow(false);
    return;
  case kForm_IngSpot:
  case kForm_Evaporate:
    CActor::Think(dt, mgr); // 0x0000D9F0
    UpdateIngSpotBody(dt, mgr);
    SetDrawShadow(false);
    UpdateHitDamageTime(dt); // vtable +0xEC
    break;
  default:
    CPatterned::Think(dt, mgr); // 0x0000DA38
    SetDrawShadow(mAlive);
    break;
  }
  // 0x0000DA58: both managers, guest 0xD60 and 0xD64.
  mCollisionActorManager0->Update(dt, mgr, CCollisionActorManager::kUO_ObjectSpace);
  mCollisionActorManager1->Update(dt, mgr, CCollisionActorManager::kUO_ObjectSpace);
  mLineOfSight.Update(dt, mgr);
  UpdateTimers(dt, mgr);
  mSurfaceAlignment.Update(*this, mgr, dt);
  UpdatePuddleEffects(dt, mgr);
  UpdateValidTarget(mgr);
  UpdateLoopSfx();
  UpdateTouchBox();
  UpdateHitLight(dt, mgr);
  if (mgr.GetObjectById(mD76Id) == nullptr) { // 0x0000DB00: no kInvalidUniqueId guard
    mD76Id = kInvalidUniqueId;
  }
  const CHealthInfo* health = GetHealthInfo();
  const float damage = 1.f - health->GetHP() / health->GetInitialHP();
  const int count = 3;
  const float step = 1.f / static_cast< float >(count);
  for (int i = 0; i < count; ++i) {
    const float lo = step * static_cast< float >(i);
    if (damage >= lo && damage <= lo + step) {
      if (mPhaseIndex != i) {
        mPhaseIndex = i;
        mSpeed = CurrentPhase().mLocomotionSpeedScale;
        if (DebugBg()) {
          fprintf(stderr, "[ibbg] %04x phase %d (damage %.2f) speed %.2f\n",
                  GetUniqueId().Value(), i, damage, mSpeed);
        }
      }
      break;
    }
  }
  // Once-a-second position poll: the off-navmesh Ing repeats Lurk -> SetRecoverDest ->
  // FailSafeMode, and only a position trace says whether the fail-safe teleport moves it.
  if (DebugBg() && (mDebugPoll++ % 60) == 0) {
    const CVector3f pos = GetTranslation();
    fprintf(stderr, "[ibbg] %04x pos (%.1f, %.1f, %.1f) form %d state %s\n", GetUniqueId().Value(),
            pos.GetX(), pos.GetY(), pos.GetZ(), mForm, mLastLoggedState ? mLastLoggedState : "?");
  }
}

// @ fn_30_55D8: the ing-spot forms' think. 0x11BC counts the time in the locomotion body state;
// the body and the animation run only for its first 2.5 s (rodata_E0).
void CIngBoostBallGuardian::UpdateIngSpotBody(float dt, CStateManager& mgr) {
  if (mBodyController->GetCurrentStateId() != pas::kAS_Locomotion) {
    mBodyState5Time = 0.f;
  } else {
    mBodyState5Time += dt;
  }
  if (mBodyState5Time < 2.5f) {
    mBodyController->Update(dt, mgr);
    UpdateAnimation(dt, mgr, true);
  }
  if (!mStateMachine->HasState()) {
    InitializeStateMachine(mgr);
  } else {
    mStateMachine->Update(mgr, *this, dt);
  }
  mWaypointNavigation.Update(dt);
}

// @ fn_30_56E0: an axis-aligned box on the translation, 2.75 across (rodata_E4) and 1.0 up and
// down (rodata_64).
void CIngBoostBallGuardian::UpdateTouchBox() {
  const CVector3f pos = GetTranslation();
  const CVector3f max(pos.GetX() + 2.75f, pos.GetY() + 2.75f, pos.GetZ() + 1.f);
  const CVector3f min(pos.GetX() - 2.75f, pos.GetY() - 2.75f, pos.GetZ() - 1.f);
  mTouchBox = CAABox(min, max);
}

static bool BgTrackerEmitting(CCollisionTracker* tracker) {
  return tracker != nullptr && tracker->GetParticleSystem() != nullptr &&
         tracker->GetParticleSystem()->GetParticleCount() > 0;
}

// @ fn_30_6108
// The two blob trackers (ing 0x1074, boost 0x1076): one is driven on the surface under the guardian
// and the other is turned off. The driven one is the ing tracker in the boost puddle form while it
// still emits, else the ing tracker unless the boost tracker still emits.
void CIngBoostBallGuardian::UpdatePuddleEffects(float, CStateManager& mgr) {
  CCollisionTracker* ing = static_cast< CCollisionTracker* >(mgr.ObjectById(mIngBlobTrackerId));
  const bool ingEmitting = BgTrackerEmitting(ing);
  CCollisionTracker* boost =
      static_cast< CCollisionTracker* >(mgr.ObjectById(mBoostBlobTrackerId));
  const bool boostEmitting = BgTrackerEmitting(boost);
  const bool useIng = mCurrentlyInBoostPuddleForm ? ingEmitting : !boostEmitting; // 0x11EF 0x80
  CCollisionTracker* driven = useIng ? ing : boost;
  CCollisionTracker* other = useIng ? boost : ing;
  if (driven != nullptr) {
    float rate = 0.f;
    switch (mForm) {
    case kForm_IngSpot: // 0x00006274
      rate = 1.f;
      if (CBoostBallGuardianBall* ball =
              TCastToPtr< CBoostBallGuardianBall >(mgr.ObjectById(mBoostChildId))) {
        switch (ball->GetState()) { // ball 0x2D4
        case 0:
          rate *= ball->GetShrink(); // ball 0x530
          break;
        case 1:
          rate = 0.f;
          break;
        case 2:
          rate = 1.f;
          break;
        default:
          break;
        }
      }
      break;
    case kForm_BecomingCorporeal:
    case kForm_BecomingIngSpot:
    case kForm_BecomingBodyProjectile:
    case kForm_BecomingWallProjectile: // 0x000062EC
      if (mIngSpotBlobVisible) {           // 0x11ED 0x40
        rate = 1.f;
      }
      break;
    case kForm_Corporeal: // 0x00006304
      if (!GetAlive() && mIngSpotBlobVisible) {
        rate = 1.f;
      }
      break;
    default:
      break;
    }
    if (rate > 0.f) {
      const CVector3f normal = mSurfaceAlignment.GetSurface().GetNormal(); // 0xD68
      const CVector3f pos = GetTranslation();
      const CVector3f& up = normal.GetZ() < 0.95f ? CVector3f::Up() : CVector3f::Right();
      driven->SetTransform(CTransform4f::LookAt(pos, pos + normal, up));
      driven->SetModelFlags(CModelFlags(CModelFlags::kT_Two, 0, CModelFlags::EFlags(3),
                                        CColor(mColor.GetRedu8(), mColor.GetGreenu8(),
                                               mColor.GetBlueu8(), 0xff)));
    }
    driven->SetParticleEmissionRateScalar(rate);
  }
  if (other != nullptr) {
    other->SetParticleEmissionRateScalar(0.f);
  }
}

// @ fn_30_65C8
// The "Ing Light" (0x1084) on the head locator (0x10AD): it fades in (0x11C4) while the guardian is
// alive and corporeal and out otherwise.
void CIngBoostBallGuardian::UpdateHitLight(float dt, CStateManager& mgr) {
  if (GetAlive() && mForm == kForm_Corporeal) { // 0x420 0x40, 0xAE4
    mLightBlend = CMath::Min(1.f, mLightBlend + dt);
  } else {
    mLightBlend = CMath::Max(0.f, mLightBlend - dt);
  }
  if (CGameLight* light = TCastToPtr< CGameLight >(mgr.ObjectById(mHitLightId))) {
    const CColor color = CColor::Lerp(CColor::Black(), mData.mLightColor, mLightBlend); // 0x8F8
    CLight lightData = CLight::BuildPoint(GetLctrTransform(mEyeSegId).GetTranslation(), color);
    // 0x000066D0-0x000066E8: f1 = f3 = rodata_70 (0), f2 = rodata_64 (1) / 0x8FC, i.e. the linear term.
    lightData.SetAttenuation(0.f, 1.f / mData.mLightAttenuation, 0.f); // 0x8FC
    light->SetLight(lightData);
  }
}


// fn_30_68F0: the kSM_Create body. The form is 2 (corporeal) or 3 (ing spot) from the data
// byte's kFlag_StartAsIngSpot (0x0000690C/0x00006914), not the guest bit 0x11ED 0x40; the same
// bit picks the locomotion type (0x00006928): crouch (0x00006934) with the surface alignment
// (0x00006954) or relaxed (0x00006960). The flag byte's 0x80 is never set - the loader's
// fn_30_10C24 leaves it clear (0x00010D9C) - so the ing-spot arm cannot run in the shipped data.
// @ fn_30_68F0
void CIngBoostBallGuardian::OnCreate(CStateManager& mgr) {
  const bool startAsIngSpot = (mData.mFlags210 & kFlag_StartAsIngSpot) != 0;
  mForm = startAsIngSpot ? kForm_IngSpot : kForm_Corporeal; // 0x00006910/0x0000691C/0x00006920
  if (startAsIngSpot) {
    BodyController()->SetLocomotionType(pas::kLT_Crouch); // 0x00006934: li r4, 0
    mSurfaceAlignment.AlignNearPosition(*this, mgr, GetTranslation(), 1.f); // 0x00006954
  } else {
    BodyController()->SetLocomotionType(pas::kLT_Relaxed); // 0x00006960: li r4, 1
  }
}


// fn_30_DDA0 (this module).
// The Ing's AcceptScriptMsg (the Ing REL's fn_29_C9A4): the message switch that creates the body
// and the collision actors, joins the team, relays the hits and cleans up on delete.
// @ fn_30_DDA0
void CIngBoostBallGuardian::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // fn_30_DDA0 (0x0000DDC4..0x0000DE08): the active bit (CEntity 0x20 bit 7) before the base
  // handles the message, as a float compared with 0 (f31 vs lbl_30_rodata_70). ACTV acts only on
  // an inactive entity, DCTV only on an active one.
  const bool wasActive = GetActive();
  CPatterned::AcceptScriptMsg(mgr, msg);
  const TUniqueId sender = msg.GetSenderId();
  if (DebugBg()) {
    // Which damage messages reach the Ing, and from whom: the collider relay is the only route a
    // projectile has to a corporeal Ing (pool this with MP2_DEBUG_BG=1).
    switch (msg.GetMessage()) {
    case kSM_Damage:          // XDMG in retail's naming
    case kSM_HitObject:
    case kSM_ResistedDamage:  // XRDG
      fprintf(stderr, "[ibbg] %04x msg %d from %04x\n", GetUniqueId().Value(),
              static_cast< int >(msg.GetMessage()), sender.Value());
      break;
    default:
      break;
    }
  }
  switch (msg.GetMessage()) {
  case kSM_Create:
    OnCreate(mgr); // fn_30_68F0
    // 0x0000DF0C: the body is activated after the form picks its locomotion (kAS_Invalid). The
    // [port] guard skips it when a think already activated the body (see EnsureBodyActive).
    EnsureBodyActive(mgr);
    CreateCollisionActorManager(mgr); // 0x0000DF18: fn_30_4E90
    CreateIngSpotManager(mgr);        // 0x0000DF24: fn_30_4A38
    mIngBlobTrackerId = CreateBlobTracker( // 0x0000DF90: fn_30_648C
        mgr, TLockedToken< CGenDescription >(
                 gpSimplePool->GetObj(SObjectTag('PART', mData.mIngSpot.GetBlobEffect()))));
    mBoostBlobTrackerId = CreateBlobTracker( // 0x0000E01C: fn_30_648C
        mgr, TLockedToken< CGenDescription >(
                 gpSimplePool->GetObj(SObjectTag('PART', mBoost.mBlobEffect))));
    CreateHitLight(mgr); // 0x0000E048: fn_30_67B4
    // 0x0000E06C: the (kSS_Modify, kSM_Activate) connections, which the boost ball gets.
    // 0x0000E074/0x0000E080: the vector's assignment and the temporary's destructor
    mModifyTargets = FindConnectedObjects(mgr, kSS_Modify, kSM_Activate); // fn_30_E430, fn_30_E4EC
    break;
  case kSM_Delete:
    mgr.DeleteObjectRequest(mIngBlobTrackerId);   // 0x0000E098
    mgr.DeleteObjectRequest(mBoostBlobTrackerId); // 0x0000E0AC
    RemoveSfxEmitters();                          // 0x0000E0B4: fn_30_3A60
    mCollisionActorManager0->Destroy(mgr); // 0x0000E0C0
    mCollisionActorManager1->Destroy(mgr); // 0x0000E0CC
    mgr.DeleteObjectRequest(mHitLightId);  // 0x0000E0E0
    if (mD76Id != kInvalidUniqueId) {      // 0x0000E0E8..0x0000E108: the exit host effect
      mgr.DeleteObjectRequest(mD76Id);
    }
    ReleaseOwnCoverHint(mgr);              // 0x0000E114: fn_30_6064
    LeaveTeam(mgr);                        // 0x0000E120: fn_30_510C
    break;
  case kSM_AreaLoaded:
    mPathArea =
        mgr.GetWorld()->GetAreaAlways(GetCurrentAreaId()).GetPostConstructed()->mPathArea;
    mPathFindSearch.SetArea(mPathArea);
    mPointPathFindSearch = CPathFindPointSearch(mPathArea);
    break;
  case kSM_Alert:
    // 0x0000E2F4: ALRT sets CPatterned 0x420 bit 7; mAlert (0x11ED 0x08) is Lurk's (0x0000A12C).
    SetHitByPlayerProjectile(true);
    break;
  case kSM_Activate:
    // 0x0000E134: only when it was inactive; each manager is on for its own form.
    if (!wasActive) {
      mCollisionActorManager0->SetActive(mgr, mForm == kForm_Corporeal); // 0x0000E150
      mCollisionActorManager1->SetActive(mgr, mForm == kForm_IngSpot); // 0x0000E16C
      if (CEntity* light = mgr.ObjectById(mHitLightId)) {
        light->SetActive(true); // 0x0000E19C
      }
      mgr.SetBossParams(GetUniqueId(), GetHealthInfo()->GetInitialHP(), // 0x0000E1F0
                        gpStringTable->GetStringIndex("BossBoostBallGuardian"));
    }
    break;
  case kSM_Deactivate:
    // 0x0000E204: only when it was active.
    if (wasActive) {
      mCollisionActorManager0->SetActive(mgr, false); // 0x0000E214
      mCollisionActorManager1->SetActive(mgr, false); // 0x0000E224
      LeaveTeam(mgr);                                 // 0x0000E230: fn_30_510C
      if (CEntity* light = mgr.ObjectById(mHitLightId)) {
        light->SetActive(false); // 0x0000E260
      }
      if (CEntity* tracker = mgr.ObjectById(mIngBlobTrackerId)) {
        static_cast< CCollisionTracker* >(tracker)->SetParticleEmissionRateScalar(0.f); // 0x0000E288: fn_25_256C
      }
      if (CEntity* tracker = mgr.ObjectById(mBoostBlobTrackerId)) {
        static_cast< CCollisionTracker* >(tracker)->SetParticleEmissionRateScalar(0.f); // 0x0000E2B0: fn_25_256C
      }
      RemoveSfxEmitters(); // 0x0000E2B8: fn_30_3A60
    }
    break;
  case kSM_AIUpdateDisabled:
    if (mCollisionActorManager0.get() != nullptr) {
      mCollisionActorManager0->SetPhysicsActive(mgr, false);
    }
    if (mCollisionActorManager1.get() != nullptr) {
      mCollisionActorManager1->SetPhysicsActive(mgr, false); // 0x0000E2EC
    }
    break;
  case kSM_HitObject:
    CollisionActorHitPlayer(mgr, sender); // 0x0000E3AC: fn_30_47E8
    break;
  case kSM_Damage:
    ColliderDamage(mgr, sender);   // 0x0000E354: fn_30_479C
    mHitByPlayerProjectile = true; // 0x0000E364: 0x420 bit 7
    break;
  case kSM_ResistedDamage:
    if (mForm == kForm_IngSpot) {
      PuddleColliderResistedDamage(mgr, sender); // 0x0000E390: fn_30_4324
    }
    break;
  case kSM_XENZ:
    ++mSafeZoneCount; // 0x0000E3C4: 0x115C
    mHurtfulZone = mgr.GetSafeZoneManager()->IsObjectInHurtfulSafeZone(*this, mgr); // 0x11EE 0x40
    break;
  case kSM_XEXZ:
    --mSafeZoneCount; // 0x0000E3F0
    mHurtfulZone = mgr.GetSafeZoneManager()->IsObjectInHurtfulSafeZone(*this, mgr);
    break;
  default:
    break;
  }
  (void)sender;
}

namespace {
// The form gate PreRender and AddToRenderer share (0x0000D2F4..0x0000D324 and
// 0x0000D25C..0x0000D28C): forms 2, 4 and 6..9 always, 3 and 5 while the body-state-5 time
// (0x11BC) is under rodata_E0 (2.5), 0, 1 and 10 and up never.
bool BgFormDraws(int form, float bodyState5Time) {
  if (form == kForm_Corporeal || form == kForm_BodyProjectile ||
      (form >= kForm_BecomingCorporeal && form <= kForm_BecomingWallProjectile)) {
    return true;
  }
  if (form == kForm_IngSpot || form == kForm_Evaporate) {
    return bodyState5Time < 2.5f;
  }
  return false;
}
} // namespace

// @ fn_30_D2E0
void CIngBoostBallGuardian::PreRender(CStateManager& mgr) {
  if (BgFormDraws(mForm, mBodyState5Time)) {
    CPatterned::PreRender(mgr); // 0x0000D32C / 0x0000D34C
  }
  // 0x0000D350: 0x11ED 0x80 ORs 0x80 into the draw flags (0x102). Retail also stores an
  // uninitialised stack word into the flags' first word (0xFC); nothing reads it.
  if (mPortalPlane) {
    const CModelFlags flags = GetModelFlags();
    SetModelFlags(CModelFlags(flags, flags.GetOtherFlags() | 0x80));
  }
}

// @ fn_30_D250
void CIngBoostBallGuardian::AddToRenderer(const CStateManager& mgr) const {
  if (BgFormDraws(mForm, mBodyState5Time)) {
    CPatterned::AddToRenderer(mgr); // 0x0000D290 / 0x0000D2AC
  }
}

// The ing-spot and evaporating forms draw no model, only the particle systems of the model's
// particle database (the visible puddle is the blob tracker, fn_30_648C).
// @ fn_30_D384
void CIngBoostBallGuardian::Render(const CStateManager& mgr) const {
  if (mPortalPlane) { // 0x0000D3A0: 0x11ED 0x80; the plane is 0x1120 / 0x112C
    GetModelData()->SetupWorldSpacePortalPlane(
        GetTransform(), CPlane(mPortalPlaneConstant, CUnitVector3f(mPortalPlaneNormal)));
  }
  const int form = mForm;
  if (form == kForm_Corporeal || form == kForm_BodyProjectile ||
      (form >= kForm_BecomingCorporeal && form <= kForm_BecomingWallProjectile)) {
    if (form == kForm_BecomingWallProjectile && !mForm9Render) { // 0x0000D3F0: 0x11ED 0x20
      return;
    }
    if (mRenderModel) { // 0x0000D404: 0x11EC 0x01
      CPatterned::Render(mgr);
      return;
    }
    if (mDrawParticles) { // 0x0000D420: 0x421 0x01
      uint mask = 0;
      uint target = 0;
      mgr.GetCharacterRenderMaskAndTarget(mask, target); // 0x0000D444
      CPatterned::RenderSystemsToBeDrawnFirst(mgr, mask, target);
      CPatterned::RenderSystemsToBeDrawnLast(mgr, mask, target);
    }
    return;
  }
  // Forms 0/1 and 10+ draw nothing (the switch goes straight to 0x0000D4E0), as AddToRenderer skips them.
  if (form == kForm_IngSpot || form == kForm_Evaporate) {
    if (mBodyState5Time < 2.5f && mDrawParticles) { // 0x0000D474: 0x11BC < rodata_E0; 0x421 0x01
      uint mask = 0;
      uint target = 0;
      mgr.GetCharacterRenderMaskAndTarget(mask, target); // 0x0000D4AC
      const CParticleDatabase& db = GetModelData()->GetAnimationData()->GetParticleDB();
      db.RenderSystemsToBeDrawnFirstPOICheck(mask, target); // 0x0000D4C4
      db.RenderSystemsToBeDrawnLastPOICheck(mask, target);  // 0x0000D4DC
    }
  }
}

// Unlike the Ing's, this module's slot 14 never picks a form's vulnerability: the guardian's own
// hits go through its collision actors (fn_30_30E4 / fn_30_3190 give them theirs).
// @ fn_30_D230
const CDamageVulnerability* CIngBoostBallGuardian::GetDamageVulnerability() const {
  return &CDamageVulnerability::ImmuneVulnerabilty(); // 0x0000D23C: fn_800DBB60
}

// fn_30_CFC4: the module's GetAimPosition (CPhysicsActor's vtable slot 0x54), the Ing's
// fn_29_BCB4 with one difference: here the ctor fills the collar locator (0x10AC, Collar_SDK at
// 0x0000FFAC), so the locator path is live. The corporeal form and the form changes (6..8)
// predict their motion; every other form takes the base class's aim.
// @ fn_30_CFC4
CVector3f CIngBoostBallGuardian::GetAimPosition(const CStateManager& mgr, float dt) const {
  CVector3f aim = CVector3f::Zero(); // 0x0000D00C: sZeroVector
  // 0x0000D014..0x0000D040: form 2 and forms 6..8 predict, the rest (0, 1, 3..5, 9 and up) don't.
  const bool predict = mForm == kForm_Corporeal ||
                       (mForm >= kForm_BecomingCorporeal && mForm < kForm_BecomingWallProjectile);
  if (!predict) {
    return CPatterned::GetAimPosition(mgr, dt); // 0x0000D16C
  }
  if (dt > 0.f) { // 0x0000D04C: rodata_70 = 0.0
    aim = PredictMotion(dt).GetTranslation(); // 0x0000D060, fn_30_D1C4 copies it
  }
  if (mCollarSegId == CSegId::Invalid()) { // 0x0000D080: cmplwi 0xff
    // Unreachable here: the ctor always finds Collar_SDK in this module's model.
    return CPatterned::GetAimPosition(mgr, dt); // 0x0000D148
  }
  // 0x0000D0A0..0x0000D0BC: the locator's transform composed with the actor's (0x24); the model
  // scale (the model data's first three floats) times its translation is added to the prediction.
  const CTransform4f world =
      GetTransform() * GetModelData()->GetAnimationData()->GetLocatorTransform(mCollarSegId, nullptr);
  const CVector3f offset = world.GetTranslation();
  const CVector3f scale = GetModelData()->GetScale();
  aim += CVector3f(scale.GetX() * offset.GetX(), scale.GetY() * offset.GetY(),
                   scale.GetZ() * offset.GetZ());
  // 0x0000D0D0..0x0000D110: forms 6 and 7 fall back to the base class when the aim is below the
  // actor (0x5c).
  if ((mForm == kForm_BecomingCorporeal || mForm == kForm_BecomingIngSpot) &&
      aim.GetZ() < GetTranslation().GetZ()) {
    return CPatterned::GetAimPosition(mgr, dt); // 0x0000D124
  }
  return aim;
}

// The ing-spot and evaporating forms use the puddle box (0xDD0), the two host forms have none, the
// rest the patterned bounds (fn_30_CD10, the REL's copy of CPatterned::GetTouchBounds).
// @ fn_30_CC34
// @ fn_30_CD10
rstl::optional_object< CAABox > CIngBoostBallGuardian::GetTouchBounds() const {
  if (mForm == kForm_IngSpot || mForm == kForm_Evaporate) {
    return mTouchBox;
  }
  if (mForm == 0 || mForm == 1) {
    return rstl::optional_object< CAABox >();
  }
  return CPatterned::GetTouchBounds();
}

// The ing-spot form's contact damage: nothing at all (not even the base Touch) while a boost is
// running (0x11EE 0x20 clear); the target takes the boost block's ing-spot damage, the morph-ball
// one (0xEC4) when it is a morphed player, else the first-person one (0xEE0).
// @ fn_30_CE50
void CIngBoostBallGuardian::Touch(CActor& actor, CStateManager& mgr) {
  EnsureBodyActive(mgr); // host guard, not in retail (see EnsureBodyActive)
  if (!mBoostOver) { // 0x0000CE78
    return;
  }
  // 0x0000CE84: form 3; 0x0000CE98: the target (0x1088); 0x0000CEAC: 0x440 <= rodata_70 (0.0).
  if (mForm == kForm_IngSpot && actor.GetUniqueId() == mTargetId && mCurDamageRemTime <= 0.f) {
    bool firstPerson = true;
    if (const CPlayer* player = TCastToConstPtr< CPlayer >(mgr.GetObjectById(mTargetId))) {
      // 0x0000CEDC..0x0000CEF8: the morph state (0x38C) counts only when 0x390 is 0.
      const int state = player->GetSpawnedMorphballState() == CPlayer::kMS_Unmorphed
                            ? int(player->GetMorphballTransitionState())
                            : 0;
      if (state == CPlayer::kMS_Morphed) {
        firstPerson = false;
      }
    }
    const CDamageInfo& damage =
        firstPerson ? mBoost.mIngSpotDamageToFirstPerson : mBoost.mIngSpotDamageToMorphBall;
    // 0x0000CF14..0x0000CF88: include 1 << data_0 (0x3B, kMT_Solid), exclude none, type 3.
    mgr.ApplyDamage(GetUniqueId(), mTargetId, GetUniqueId(), damage, SolidFilter(),
                    CVector3f::Zero());
    mCurDamageRemTime = mDamageWaitTime; // 0x0000CF90: 0x444 -> 0x440
  }
  CPatterned::Touch(actor, mgr); // 0x0000CFA0
}

// @ fn_30_CA18
// The hit tint: the damage colour while the cooldown runs, else the hits-without-damage colour
// while a resisted bomb hit (0x1080) or a safe zone holds it. Retail sets no damage highlight and
// drives no echo emitter here, unlike CPatterned's.
void CIngBoostBallGuardian::UpdateHitDamageTime(float dt) {
  // 0xCA44/0xCA68 fcmpo 0.0,v; bge -> 0: NaN clamps to 0
  mDamageCooldownTimer = mDamageCooldownTimer - dt > 0.f ? mDamageCooldownTimer - dt : 0.f;
  mBoostHitTimer = mBoostHitTimer - dt > 0.f ? mBoostHitTimer - dt : 0.f;
  if (mSafeZoneCount > 0) {
    mBoostHitTimer = CPatterned::skDamageHitTime; // 0x0000CA90
  }
  if (mDamageCooldownTimer > 0.f) {
    const float t = mDamageCooldownTimer / CPatterned::skDamageHitTime;
    const CColor color = CColor::Lerp(CColor::Black(), CPatterned::skDamageColor, t);
    mColor.Set(color.GetRedu8(), color.GetGreenu8(), color.GetBlueu8(), mColor.GetAlphau8());
  } else if (mBoostHitTimer > 0.f) {
    const float t = mBoostHitTimer / CPatterned::skDamageHitTime;
    const CColor color = CColor::Lerp(CColor::Black(), CPatterned::skHitsWithoutDamageColor, t);
    mColor.Set(color.GetRedu8(), color.GetGreenu8(), color.GetBlueu8(), mColor.GetAlphau8());
  }
}

// fn_30_D4F8 (this module).
// The Ing's DoUserAnimEvent (the Ing REL's fn_29_C228). The base class's handler runs only for the
// events this function does not handle (retail's local `handled` flag, set by every case below).
// The area 57 run delivered kUE_LoopedSoundStop (2), kUE_Activate (29), kUE_EffectOff (34) and
// kUE_Unknown37 (37).
// @ fn_30_D4F8
void CIngBoostBallGuardian::DoUserAnimEvent(CStateManager& mgr, const CInt32POINode& node,
                                             EUserEventType type, float dt) {
  EnsureBodyActive(mgr); // host guard, not in retail
  bool handled = false;
  switch (type) {
  case kUE_Projectile: // 0
    // [this module] fn_30_D4F8 @0xD65C: one mini portal per event, as long as the effect is loaded
    // (0x1050, engaged flag 0x105C) and FindMiniPortals queued a point still unused (0x1158 <
    // 0x1130, points at 0x1134 + 12 * index). The damage is the mini-portal block's (0xA8C) scaled
    // by dt and made immunity-proof (bit 7 of damage+0x1a); sound 0x89C, beam info 0x8B0, the
    // target 0x1088. The spawn's name is rodata_12C + 0x608, "Ing Mini Portal Attack". Handled
    // all the same, so the base class does not fire a generic projectile.
    if (mMiniPortalEffect && mMiniPortalIndex >= 0 && mMiniPortalIndex < mMiniPortalCount) {
      CTransform4f xf = GetTransform();
      xf.SetTranslation(mMiniPortalPositions[mMiniPortalIndex++]);
      CDamageInfo damage(mData.mMiniPortal.mDamage);
      damage.MultiplyDamage(dt);
      damage.SetNoImmunity(true);
      // rodata_64 1.0, rodata_78 2.0, rodata_74 150.0, then 1.0 again.
      const CIngMiniPortalInfo portalInfo(mTargetId, 1.f, 2.f, *mMiniPortalEffect,
                                          mData.mMiniPortal.mSound, 150.f, 1.f, damage,
                                          mData.mMiniPortal.mBeamInfo);
      CIngMiniPortalAttack* portal = rs_new CIngMiniPortalAttack(
          mgr.AllocateUniqueId(), rstl::string_l("Ing Mini Portal Attack"),
          CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, true), xf, GetUniqueId(),
          GetModelData()->GetScale(), portalInfo);
      mgr.AddObject(portal);
    }
    handled = true;
    break;
  case kUE_TakeOff: // [this module] fn_30_D4F8 @0xD578
    mTakeOff = true; // 0x11EC 0x02
    handled = true;
    break;
  case kUE_BeginAction: // [this module] fn_30_D4F8 @0xD590: the form only; the Ing's reset of
                        // the body-projectile time is not in this module
    if (mAlive) {                   // CPatterned 0x420 bit 6
      mForm = kForm_BodyProjectile; // 0xAE4 = 4
    }
    handled = true;
    break;
  case kUE_Activate: // [this module] fn_30_D4F8 @0xD5AC
    mIngSpotBlobVisible = true; // 0x11ED 0x40
    handled = true;
    break;
  case kUE_Deactivate: // [this module] fn_30_D4F8 @0xD5C4
    mIngSpotBlobVisible = false; // 0x11ED 0x40
    handled = true;
    break;
  case kUE_EffectOn: // 0x21
    if (AnimationData() != nullptr) {
      AnimationData()->SetEffectState(rstl::string_l("eyes"), true, mgr); // rodata_40 -> rodata_68E
    }
    handled = true;
    break;
  case kUE_EffectOff: // 0x22
    if (AnimationData() != nullptr) {
      AnimationData()->SetEffectState(rstl::string_l("eyes"), false, mgr);
    }
    handled = true;
    break;
  case kUE_Unknown37: // [this module] fn_30_D4F8 @0xD548: body hits stop being lethal
    mBodyHitsLethal = false; // 0x11EF 0x10 = 0
    handled = true;
    break;
  case kUE_Unknown38: // [this module] fn_30_D4F8 @0xD560
    mBodyHitsLethal = true; // 0x11EF 0x10 = 1
    handled = true;
    break;
  default:
    break;
  }
  if (!handled) {
    CPatterned::DoUserAnimEvent(mgr, node, type, dt);
  }
}

// A death while becoming the ing spot dies corporeal.
// @ fn_30_76F4
void CIngBoostBallGuardian::Death(CStateManager& mgr, const CVector3f& direction,
                                  EScriptObjectState state) {
  EnsureBodyActive(mgr); // host guard, not in retail (see EnsureBodyActive)
  if (mForm == kForm_BecomingIngSpot) { // 0x00007704
    mForm = kForm_Corporeal;
  }
  CPatterned::Death(mgr, direction, state); // 0x00007714
}

// Only the corporeal form outside the knock-back body state takes the knock-back reaction; a light
// beam hit flinches with type 2, every other with type 1.
// @ fn_30_CB84
void CIngBoostBallGuardian::KnockBack(CStateManager& mgr, const CKnockBackInfo& info) {
  EnsureBodyActive(mgr); // host guard, not in retail (see EnsureBodyActive)
  // 0x0000CBAC: form 2; 0x0000CBBC: body state (0x48C -> 0x37C) 6.
  const bool react =
      mForm == kForm_Corporeal && BodyController()->GetCurrentStateId() != pas::kAS_KnockBack;
  KnockBackController().EnableAnimReaction(CKnockBackMgr::kAR_KnockBack, react); // 0x620
  // 0x0000CBE8: the damage's weapon type (info + 0x10) 2 -> 0x688 = 2, else 1.
  KnockBackController().SetFlinchType(
      info.GetDamageInfo().GetWeaponMode().GetType() == kWT_Light ? 2 : 1);
  CPatterned::KnockBack(mgr, info); // 0x0000CC14
}

// fn_30_CB54 (vtable slot 39; the Ing REL's fn_29_B890): the two host forms and the evaporating
// form take no damage (0x0000CB5C: form 5; 0x0000CB6C: forms 0..1); CPatterned's body is the
// cooldown store at 0x0000CB7C (lbl_8041AAB8, CPatterned's skDamageHitTime, into 0x448).
// @ fn_30_CB54
void CIngBoostBallGuardian::TakeDamage(const CVector3f& direction, float magnitude) {
  if (mForm == 0 || mForm == 1 || mForm == kForm_Evaporate) {
    return;
  }
  CPatterned::TakeDamage(direction, magnitude);
}

// A path obstruction inside 40 units asks for a path recalculation; a player shot inside the
// hearing radius (0x7C4) and, with a leash time (0x3D4), less than its square above or below
// counts as heard (0x11AC = 0). Other noises are ignored.
// @ fn_30_C758
bool CIngBoostBallGuardian::Listen(CStateManager&, const CVector3f& position,
                                   EListenNoiseType type) {
  bool heard = false;
  if (!GetAlive()) { // 0x0000C760: 0x420 0x40
    return heard;
  }
  const CVector3f delta = position - GetTranslation();
  switch (type) {
  case kLNT_PathObstruction: // 4
    if (delta.MagSquared() < 1600.f) { // 0x0000C7B8: rodata_114
      mRecalcPath = true;              // 0x0000C7C8: 0x11EC 0x08
      heard = true;
    }
    break;
  case kLNT_PlayerFire: { // 0
    const float dzSq = delta.GetZ() * delta.GetZ();
    if (delta.MagSquared() < mData.mHearingRadius * mData.mHearingRadius) { // 0x0000C810
      // 0x0000C828: leash 0 hears at any height; 0x0000C830: else dz^2 < leash^2.
      if (mCurPlayerLeashTime == 0.f || dzSq < mCurPlayerLeashTime * mCurPlayerLeashTime) {
        heard = true;
        mHeardShotTime = 0.f; // 0x0000C844
      }
    }
    break;
  }
  default:
    break;
  }
  return heard;
}
