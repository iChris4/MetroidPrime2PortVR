// Retail Sandworm REL (module 56): the FSM triggers and states. Bodies land here per
// docs/module-analysis/Sandworm_functions.tsv.
#include "MetroidPrime/Enemies/CSandworm.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"
#include "MetroidPrime/BodyState/CBodyStateCmdMgr.hpp"
#include "MetroidPrime/CCollisionActor.hpp"
#include "MetroidPrime/CCollisionActorManager.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptLoader/SLdrSandworm.hpp"
#include "MetroidPrime/ScriptObjects/CScriptCoverPoint.hpp"
#include "MetroidPrime/TCastTo.hpp"

// WP3: locomotion and lifecycle states, their triggers, path/cover helpers and the move/vocal
// sounds. Several retail WP3 helpers have signatures the header does not match yet; those are
// file-local functions taking the worm (all members are public) and are listed in the report.

namespace {

bool IsBombGuardian(const CSandworm& self) { return (self.x8C0_flags & 0x10) != 0; }

const CVector3f& PlayerPos(const CStateManager& mgr) { return mgr.Player(0)->GetTranslation(); }

// Wobble period shared by Patrol and PursueHelper.
float WobblePeriod(const CSandworm& self) {
  float period = self.x1510_cycleRate * (1.2f - 0.4f) + 0.4f;
  if (self.x1514_ == 1) {
    period *= 0.25f;
  }
  return period;
}

float WobbleStrength(const CSandworm& self) {
  return self.x1510_cycleRate * (0.5f - 0.1f) + 0.1f;
}

} // namespace

// @ fn_56_3EF4
void CSandworm::UpdateEmitter(CSfxHandle& handle) {
  if (!handle) { // 0x3F18: no emitter, nothing to update
    return;
  }
  if (!CSfxManager::IsPlaying(handle) && !CSfxManager::IsQueued(handle)) {
    CSfxManager::RemoveEmitter(handle);
    handle = CSfxHandle();
  } else {
    CSfxManager::UpdateEmitter(handle, x1388_, GetTransform().GetColumn(kDY), 0x7f);
  }
}

// @ fn_56_403C
void CSandworm::StopMoveSfx() {
  CSfxHandle& handle = x1828_sfx.x18_;
  if (handle) {
    CSfxManager::RemoveEmitter(handle);
  }
  handle = CSfxHandle();
}

// class T (fn_56_534C): the number of 'GRNT'/'NEXT' connections that resolve to an actor.
// Unreferenced in retail - no 'bl fn_56_534C' in the module's asm and no table entry or
// relocation for it, so nothing calls it. No marker (Sandworm_functions.tsv, class T).
int CSandworm::CountScriptActors(CStateManager& mgr) const {
  int count = 0;
  const rstl::vector< SConnection >& conns = GetConnectionList();
  for (rstl::vector< SConnection >::const_iterator it = conns.begin(); it != conns.end(); ++it) {
    if (it->state != kSS_GRNT || it->msg != kSM_Next) {
      continue;
    }
    if (TCastToPtr< CActor >(mgr.ObjectById(mgr.GetIdForScript(it->objId)))) {
      ++count;
    }
  }
  return count;
}

// Blends the move vector toward its perpendicular.
// @ fn_56_6D74
CVector3f CSandworm::PursueHelper(const CVector3f& v) const {
  const float period = WobblePeriod(*this);
  CVector3f perp;
  if (x150C_cycle > 0.5f * period) {
    perp = CVector3f(v.GetY(), -v.GetX(), v.GetZ());
  } else {
    perp = CVector3f(-v.GetY(), v.GetX(), v.GetZ());
  }
  const float t = WobbleStrength(*this);
  return v * (1.f - t) + perp * t;
}

// @ fn_56_7A2C
CVector3f CSandworm::FindCoverPoint(CStateManager& mgr) {
  if (x13AC_ < 6.f && x1384_ < 8.f && x184C_ == 0) {
    if (IsPossessedBombGuardian()) {
      if (x19FC_ + 1.f < x8B0_activeTime) {
        x19F0_ = PlayerPos(mgr);
        x19FC_ = x8B0_activeTime;
      }
      return x19F0_;
    }
    return PlayerPos(mgr);
  }

  if (x1848_ != kInvalidUniqueId) {
    if (const CScriptCoverPoint* cp = GetCoverPoint(mgr, x1848_)) {
      CVector3f delta = GetTranslation() - cp->GetTranslation();
      delta.SetZ(0.f);
      if (delta.Magnitude() < 4.f) {
        x184A_ = x1848_;
        x1848_ = kInvalidUniqueId;
      }
    }
  }
  if (x1848_ == kInvalidUniqueId) {
    x1848_ = ScanCoverPoints(mgr);
  }
  if (x1848_ != kInvalidUniqueId) {
    if (const CScriptCoverPoint* cp = GetCoverPoint(mgr, x1848_)) {
      return cp->GetTranslation();
    }
  }
  return PlayerPos(mgr);
}

// @ fn_56_A8E0
bool CSandworm::NearlyOnPath(const CVector3f& pos, float radius) const {
  for (float z = 0.f; z <= 5.f; z += 1.f) { // 0xA998 cror eq,lt,eq: <= 5.0
    if (x7C4_search.NearlyOnPath(pos + CVector3f(0.f, 0.f, z), radius) ==
        CPathFindSearch::kR_Success) {
      return true;
    }
  }
  return false;
}

// x1354 moved to the 'GRNT'/'NEXT' actor farthest from the player; updates `index`.
// @ fn_56_C468
CTransform4f CSandworm::MoveToScriptActor(CStateManager& mgr, int& index) const {
  const CVector3f& playerPos = PlayerPos(mgr);
  float bestDistSq = 0.f;
  const CActor* best = nullptr;
  int bestIndex = 0;
  int counter = 0;
  const rstl::vector< SConnection >& conns = GetConnectionList();
  for (rstl::vector< SConnection >::const_iterator it = conns.begin(); it != conns.end(); ++it) {
    if (it->state != kSS_GRNT || it->msg != kSM_Next) {
      continue;
    }
    const CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(mgr.GetIdForScript(it->objId)));
    // Retail quirk: the counter only advances for actors that are not skipped, so once it
    // reaches `index` every later actor is skipped too.
    if (actor == nullptr || counter == index) {
      continue;
    }
    const float distSq = (playerPos - actor->GetTranslation()).MagSquared();
    if (distSq > bestDistSq) {
      bestDistSq = distSq;
      best = actor;
      bestIndex = counter;
    }
    ++counter;
  }
  CTransform4f xf = x1354_;
  if (best != nullptr) {
    xf.SetTranslation(best->GetTranslation());
    index = bestIndex;
  }
  return xf;
}

// class T (fn_56_10A68): ramps the steering speed over 1.2 s after x134C.
// Unreferenced in retail - no 'bl fn_56_10A68' in the module's asm and no table entry or
// relocation for it, so nothing calls it. No marker (Sandworm_functions.tsv, class T).
void CSandworm::SetSteeringSpeedRange() {
  const float t = x8B0_activeTime - x134C_;
  BodyController()->CommandMgr().SetSteeringSpeedRange(0.f, t > 1.2f ? 1.f : t / 1.2f);
}

// --- Members ---

// @ fn_56_2718
void CSandworm::DescendHelper() { x1490_attacks.x8_history.clear(); }

// @ fn_56_33A4
void CSandworm::Interrupt(CStateManager& mgr, float dt) {
  BodyController()->CommandMgr().DeliverCmd(CBodyStateCmd(kBSC_NextState));
  BodyController()->CommandMgr().DeliverCmd(CBodyStateCmd(kBSC_AbortScripted));
}

// @ fn_56_3484
void CSandworm::EyeKilledReaction(CStateManager& mgr, EStateMsg msg, float dt) {
  SetBodyCmdRequest(3, 0xd);
  x1520_flags |= 0x80;
  // Retail CBCTauntCmd carries a second word (0 here); the port command only has the type.
  IssueTauntCmd(msg, pas::kAS_Taunt, CBCTauntCmd(pas::kTT_Zero));
  if (msg == kStateMsg_Deactivate && AnyEyeDead(mgr)) {
    x14D4_ = 1;
    if (!IsBombGuardian(*this)) {
      OrientBodyToPath(mgr);
    } else if ((x19A8_bombPhase.xc_flags & 0x20) != 0) {
      OrientBodyToPath(mgr);
    }
  }
}

// @ fn_56_35F8
bool CSandworm::OneEyeKilled(CStateManager& mgr, const CTriggerData& data) const {
  if (x14D4_ == 1 || IsBombGuardian(*this)) {
    return false;
  }
  int dead = 0;
  if (GetTailEye(mgr)->IsDead()) {
    ++dead;
  }
  if (GetHeadEye(mgr)->IsDead()) {
    ++dead;
  }
  return dead == 1;
}

// @ fn_56_3D44
bool CSandworm::PlayerReachable(CStateManager& mgr, const CTriggerData& data) const {
  return PathReachable(mgr, PlayerPos(mgr));
}

// Retail fn_56_3D64 is the (mgr, data) trigger body; it searches from the worm to the player at
// most once a second (0x3D8C: `fcmpo` of 1.0 + 0x1854 against 0x8B0, `bge` skips the body) and
// caches the result in the byte at 0x1850 (`stb` of 1/0 at 0x3DB4/0x3DC0). The return is that
// byte, loaded with `lbz r3, 0x1850(r31)` at 0x3DD0 and returned as it stands - the retail never
// tests it, so the member is a bool and the return is a plain copy.
// @ fn_56_3D64
bool CSandworm::PathReachable(CStateManager& mgr, const CVector3f& pos) const {
  CSandworm& self = const_cast< CSandworm& >(*this);
  if (x1854_ + 1.f < x8B0_activeTime) {
    self.x1850_ =
        self.x1858_search2.Search(GetTranslation(), pos) == CPathFindSearch::kR_Success ? 1 : 0;
    self.x1854_ = x8B0_activeTime;
  }
  return x1850_; // 0x3DD0: `lbz r3, 0x1850(r31)`, no compare (was `x1850_ != 0`)
}

// @ fn_56_3E1C
bool CSandworm::CanDescend(CStateManager& mgr, const CTriggerData& data) const {
  if (IsBombGuardian(*this)) {
    return false;
  }
  if (x13AC_ > 6.f) {
    return true;
  }
  return x1384_ > 3.f;
}

// @ fn_56_3E6C
bool CSandworm::ForceUnderground(CStateManager& mgr, const CTriggerData& data) const {
  return (x8C1_flags & 0x80) != 0;
}

// @ fn_56_3E78
bool CSandworm::PlayerHiding(CStateManager& mgr, const CTriggerData& data) const {
  if (IsBombGuardian(*this)) {
    return false;
  }
  return x13AC_ > 8.f;
}

// @ fn_56_3EAC
void CSandworm::UpdateLoopedSfx(CStateManager& mgr) {
  UpdateEmitter(x1828_sfx.x1c_);
  UpdateEmitter(x1828_sfx.x14_);
  UpdateEmitter(x1828_sfx.x18_);
}

// @ fn_56_4088
void CSandworm::StartMoveSfx(CStateManager& mgr) {
  StopMoveSfx();
  x1828_sfx.x18_ = CSfxManager::AddEmitter(
      x1828_sfx.x2_walkVocal, x1388_, 0x7f, GetCurrentAreaId().Value(), false, true,
      CSfxManager::kMedPriority + 8);
}

// Header names the parameter `chance`; retail passes dt.
// @ fn_56_40EC
void CSandworm::StartVocalSfx(CStateManager& mgr, float dt) {
  x1828_sfx.xc_ -= dt;
  if (x1828_sfx.xc_ <= 0.f) {
    x1828_sfx.x10_ = 2.f * mgr.Random()->Float() + 4.f;
    x1828_sfx.xc_ += x1828_sfx.x10_;
    CSfxHandle& handle = x1828_sfx.x14_;
    if (handle) {
      CSfxManager::RemoveEmitter(handle);
    }
    handle = CSfxManager::AddEmitter(x1828_sfx.x0_walk, x1388_, 0x7f, GetCurrentAreaId().Value(),
                                     false, false, CSfxManager::kMedPriority + 8);
  }
}

// @ fn_56_41BC
void CSandworm::StopVocalSfx(CStateManager& mgr) {
  x1828_sfx.xc_ = 0.f;
  CSfxHandle& handle = x1828_sfx.x14_;
  if (handle) {
    CSfxManager::RemoveEmitter(handle); // retail does not clear the handle
  }
}

// @ fn_56_4200
void CSandworm::Straighten(CStateManager& mgr, EStateMsg msg, float dt) {
  if (msg == kStateMsg_Activate) {
    x1520_flags &= ~0x80;
    x1670_.xfc_ = 1.f;
    BodyController()->CommandMgr().DeliverCmd(CBodyStateCmd(kBSC_NextState));
  } else if (msg == kStateMsg_Deactivate) {
    x1520_flags &= ~0x80;
    x1520_flags &= ~0x40;
    x1670_.xfc_ = 0.f;
    const CTransform4f look = CTransform4f::LookAt(x1614_residuePosHighHealth, x1608_residuePos);
    SetTransform(CQuaternion::FromMatrix(look).BuildTransform4f(GetTranslation()));
    SetTranslation(x1388_ - SpineHelper10038());
  }
  if (x1670_.xfc_ > 0.001f) {
    x1670_.xfc_ -= 2.f * dt;
    if (x1670_.xfc_ < 0.001f) {
      x1670_.xfc_ = 0.001f;
    }
  }
}

// @ fn_56_4384
bool CSandworm::DoneStraightening(CStateManager& mgr, const CTriggerData& data) const {
  return x1670_.xfc_ <= 0.002f;
}

// @ fn_56_541C
bool CSandworm::DoneSulking(CStateManager& mgr, const CTriggerData& data) const {
  // 0x5434 bge: NaN passes the time test
  return !(x8B0_activeTime < x165C_) && PlayerIsOnPathMesh(mgr, CTriggerData(0.f));
}

// @ fn_56_5470
void CSandworm::Sulk(CStateManager& mgr, EStateMsg msg, float dt) {
  BodyController()->SetLocomotionType(pas::ELocomotionType(7));
  if (msg == kStateMsg_Activate) {
    x165C_ = x8B0_activeTime + x1650_lurkMin + (x1654_lurkMax - x1650_lurkMin) * mgr.Random()->Float();
    x1658_ = SulkHelper();
  } else if (msg == kStateMsg_Deactivate) {
    SetTransform(MoveToScriptActor(mgr, x166C_));
  }
}

// @ fn_56_557C
bool CSandworm::ShouldSulk(CStateManager& mgr, const CTriggerData& data) const {
  if (IsBombGuardian(*this)) {
    return false;
  }
  const float healthPct = SulkHelper();
  if (healthPct < 10.f) {
    return false;
  }
  return healthPct + x164C_ < x1658_;
}

// Health percentage.
// @ fn_56_55F8
float CSandworm::SulkHelper() const {
  const float initial = GetHealthInfo()->GetInitialHP();
  return 100.f * (GetHealthInfo()->GetHP() / initial);
}

// @ fn_56_58C0
bool CSandworm::DoneLurking(CStateManager& mgr, const CTriggerData& data) const {
  return GetActive() && PlayerIsOnPathMesh(mgr, CTriggerData(0.f));
}

// @ fn_56_5978
bool CSandworm::PlayerIsOnPathMesh(CStateManager& mgr, const CTriggerData& data) const {
  return NearlyOnPath(PlayerPos(mgr), 0.f);
}

// @ fn_56_5D80
void CSandworm::Dead(CStateManager& mgr, EStateMsg msg, float dt) {
  CPatterned::Dead(mgr, msg, dt);
  if (msg == kStateMsg_Activate) {
    SetBodyCmdRequest(3, 8);
    x1520_flags |= 0x80;
    x14C0_flags |= 0x80;
    GetHeadEye(mgr)->SetDead();
    GetTailEye(mgr)->SetDead();
    SpawnResidue(mgr, 0);
  }
  // Retail reads a word at body controller +0x37C; taken to be the fall state (2 = landed).
  if (BodyController()->GetFallState() == pas::EFallState(2) && (x14C0_flags & 0x40) == 0) {
    x14C0_flags |= 0x40;
    DeathDelete(mgr);
  }
}

// @ fn_56_5E5C
void CSandworm::Deactivate(CStateManager& mgr, EStateMsg msg, float dt) {
  if (msg == kStateMsg_Activate) {
    SetActive(false);
  }
}

// @ fn_56_6A50
bool CSandworm::FacingPlayer(CStateManager& mgr, const CTriggerData& data) const {
  return AngleToTarget(PlayerPos(mgr), x1528_spit.x28_xf.GetColumn(kDY)) < 0.20943952f;
}

// @ fn_56_73F4
void CSandworm::UpdatePursuePathSpeed(CStateManager& mgr, float dt) {
  x1A04_ += dt;
  if (!(x1A04_ < 0.2f)) { // 0x7424 blt skips: NaN runs
    x1A04_ = 0.f;
    if (x7C4_search.OnPath(GetTranslation() + CVector3f(0.f, 0.f, 1.f)) !=
        CPathFindSearch::kR_Success) {
      BodyController()->SetTurnSpeed(1.5f * x1A00_);
    } else {
      BodyController()->SetTurnSpeed(x1A00_);
    }
  }
}

// @ fn_56_74B8
void CSandworm::Pursue(CStateManager& mgr, EStateMsg msg, float dt) {
  BeginAttack(0, msg); // retail fn_56_B85C(type, msg)
  CBodyStateCmdMgr& cmdMgr = BodyController()->CommandMgr();
  if (IsPossessedBombGuardian()) {
    cmdMgr.SetSteeringSpeedRange(0.f, 1.f);
  }
  const CVector3f target = FindCoverPoint(mgr);

  switch (msg) {
  case kStateMsg_Activate:
    BodyController()->SetLocomotionType(pas::ELocomotionType(1));
    cmdMgr.SetSteeringBlendMode(kSBM_Clamped);
    if (IsBombGuardian(*this)) {
      if (!IsPossessedBombGuardian()) {
        cmdMgr.SetSteeringSpeedRange(0.f, 0.f);
      } else {
        mSpeed = GetPhaseParams().moveSpeedMultiplier;
      }
    }
    if ((x15AC_toss.x58_flags & 0x80) != 0) {
      ReleaseGrabbedPlayer(mgr);
    }
    SetPathDestination(mgr, target, dt);
    x1520_flags |= 0x80;
    x1524_fadeTarget = 1.f;
    ClearBodyCmdRequest();
    x14B0_ = 0.f;
    StopVocalSfx(mgr);
    StartMoveSfx(mgr);
    x1384_ = 0.f;
    x13AC_ = 0.f;
    x13B0_ = GetTranslation();
    x184C_ = 0;
    if (!x1490_attacks.EndsWith(2, 1)) {
      ResetBombDropTimer();
    }
    break;

  case kStateMsg_Update: {
    StartVocalSfx(mgr, dt);
    UpdateBombDrop(mgr, dt);
    UpdatePursuePathSpeed(mgr, dt);
    FollowPath(mgr, dt);
    if ((target - x8B4_).MagSquared() > 16.f) {
      SetPathDestination(mgr, target, dt);
    }
    if (PathShagged(mgr, CTriggerData(0.f)) ||
        x7C4_search.GetCurrentWaypoint() >= x7C4_search.GetWaypoints().size() - 1) {
      x1848_ = kInvalidUniqueId;
      if (PathHelper(mgr, kInvalidUniqueId)) {
        OrientBodyToPath(mgr);
        SetPathDestination(mgr, target, dt);
      } else {
        x184C_ = x184C_ == 1 ? 0 : 1;
        SetPathDestination(mgr, target, dt);
      }
    }
    mPathFindNavigation.PathFind(mgr, msg, dt, *this);
    const CVector3f move = PursueHelper(cmdMgr.GetMoveVector());
    if (move.CanBeNormalized()) {
      cmdMgr.DeliverCmd(CBCLocomotionCmd(move.AsNormalized(), CVector3f::Zero(), 1.f));
    }
    if ((target - GetTranslation()).Magnitude() < x14B4_frustrationRadius) {
      x14B0_ += dt;
      if (!AnyEyeDead(mgr) && !IsBombGuardian(*this) && x14B0_ > x14B8_frustrationTimer) {
        OrientBodyToPath(mgr);
      }
    }
    x1384_ += dt;
    if (PlayerIsOnPathMesh(mgr, CTriggerData(0.f))) {
      x13AC_ = 0.f;
    } else {
      x13AC_ += dt;
    }
    break;
  }

  case kStateMsg_Deactivate:
    BodyController()->SetTurnSpeed(x1A00_);
    StopVocalSfx(mgr);
    StopMoveSfx();
    x1384_ = 0.f;
    x13AC_ = 0.f;
    ResetBombDropTimer();
    break;

  default:
    break;
  }
}

// @ fn_56_7C60
TUniqueId CSandworm::ScanCoverPoints(CStateManager& mgr) {
  TUniqueId best = kInvalidUniqueId;
  float bestScore = -1.f;
  int count = 0;
  const rstl::vector< SConnection >& conns = GetConnectionList();
  for (rstl::vector< SConnection >::const_iterator it = conns.begin(); it != conns.end(); ++it) {
    if (it->state != kSS_Connect) {
      continue;
    }
    ++count;
    const CScriptCoverPoint* cp =
        TCastToPtr< CScriptCoverPoint >(mgr.ObjectById(mgr.GetIdForScript(it->objId)));
    if (!TestCoverPoint(cp)) {
      continue;
    }
    // Retail tests a state-manager flag at 0x16E8 b7; taken to be "random available".
    if (!mgr.IsRandomAvailable()) {
      return cp->GetUniqueId();
    }
    const float score = mgr.Random()->Float();
    if (score > bestScore) {
      bestScore = score;
      best = cp->GetUniqueId();
    }
  }
  if (count != 0) {
    return best;
  }

  // No 'CONN' links: scan the AiWaypoint list (retail reads it at mgr+0x838).
  CObjectList& list = mgr.ObjectListById(kOL_AiWaypoint);
  for (int idx = list.GetFirstObjectIndex(); idx != -1; idx = list.GetNextObjectIndex(idx)) {
    const CScriptCoverPoint* cp = TCastToPtr< CScriptCoverPoint >(list[idx]);
    if (!TestCoverPoint(cp)) {
      continue;
    }
    if (!mgr.IsRandomAvailable()) {
      return cp->GetUniqueId();
    }
    const float score = mgr.Random()->Float();
    if (score > bestScore) {
      bestScore = score;
      best = cp->GetUniqueId();
    }
  }
  return best;
}

// @ fn_56_7E08
bool CSandworm::TestCoverPoint(const CActor* point) const {
  return point != nullptr && point->GetActive() && point->GetCurrentAreaId() == GetCurrentAreaId() &&
         point->GetUniqueId() != x184A_ && point->GetUniqueId() != x1848_;
}

// Header names the last parameter `speed`; retail passes dt through to PathFind.
// @ fn_56_7E60
void CSandworm::SetPathDestination(CStateManager& mgr, const CVector3f& dest, float dt) {
  x8B4_ = dest;
  mPathFindNavigation.SetDestination(x8B4_);
  mPathFindNavigation.PathFind(mgr, kStateMsg_Activate, dt, *this);
}

// @ fn_56_97FC
bool CSandworm::SnatchEnded(CStateManager& mgr, const CTriggerData& data) const {
  return IsPossessedBombGuardian();
}

// @ fn_56_981C
bool CSandworm::SnatchStarted(CStateManager& mgr, const CTriggerData& data) const {
  return IsBombGuardian(*this) && !IsPossessedBombGuardian();
}

// @ fn_56_986C
void CSandworm::Snatch(CStateManager& mgr, EStateMsg msg, float dt) {
  if (msg == kStateMsg_Activate) {
    mSpeed = 0.66f;
    x14D8_charge.x30_flags &= ~0x80;
    SetBodyCmdRequest(3, 0xd);
    x1520_flags |= 0x80;
    mAnimationState.SetState(CAnimationState::kAS_Ready);
  } else if (msg == kStateMsg_Update) {
    if (mAnimationState.CanIssueCommand(*BodyController(), pas::kAS_Taunt)) {
      // Retail CBCTauntCmd carries a second word (1 here); the port command only has the type.
      BodyController()->CommandMgr().DeliverCmd(CBCTauntCmd(pas::kTT_One));
    } else if (mAnimationState.GetState() == CAnimationState::kAS_Over) {
      mSpeed = 1.f;
    }
  } else if (msg == kStateMsg_Deactivate) {
    mAnimationState.SetState(CAnimationState::kAS_NotReady);
  }
}

// @ fn_56_A708
void CSandworm::FollowPath(CStateManager& mgr, float dt) {
  if (!PathHelper(mgr, kInvalidUniqueId) || (x1520_flags & 0x80) == 0) {
    return;
  }
  const CVector3f ahead = GetTranslation() + GetTransform().GetColumn(kDY).AsNormalized() * 4.f;
  if (NearlyOnPath(ahead, 4.f)) {
    return;
  }
  const CVector3f dir = (GetSegmentPosition(12) - GetSegmentPosition(11)).AsNormalized();
  const CVector3f tailAhead = GetSegmentPosition(12) + dir * 4.f;
  if (NearlyOnPath(tailAhead, 4.f)) {
    OrientBodyToPath(mgr);
  }
}

// @ fn_56_AA74
void CSandworm::Patrol(CStateManager& mgr, EStateMsg msg, float dt) {
  if (msg == kStateMsg_Activate) {
    BodyController()->SetLocomotionType(pas::ELocomotionType(1));
    BodyController()->CommandMgr().SetSteeringBlendMode(kSBM_Clamped);
    StopVocalSfx(mgr);
    StartMoveSfx(mgr);
    SetUnderground(mgr);
    x8C0_flags |= 0x40;
    x8C0_flags &= ~0x01;
    ResetBombDropTimer();
    x1520_flags |= 0x80;
  } else if (msg == kStateMsg_Deactivate) {
    x8C0_flags &= ~0x40;
    StopVocalSfx(mgr);
    StopMoveSfx();
    ResetBombDropTimer();
    x1520_flags &= ~0x80;
  } else if (msg == kStateMsg_Update) {
    UpdateBombDrop(mgr, dt);
    StartVocalSfx(mgr, dt);
    x1520_flags |= 0x80;
  }
  mWaypointNavigation.ConfigureWobbleSteering(x150C_cycle > 0.5f * WobblePeriod(*this),
                                              WobbleStrength(*this));
  CPatterned::Patrol(mgr, msg, dt);
}

// Retail takes only mgr; the header's id parameter is unused.
// @ fn_56_AC80
bool CSandworm::PathHelper(CStateManager& mgr, TUniqueId) {
  if (AnyEyeDead(mgr) || IsBombGuardian(*this)) {
    return false;
  }
  if (x1490_attacks.x4_current == 3) {
    return false;
  }
  if (x1210_.size() < 13) {
    return false;
  }
  return x134C_ + 5.f < x8B0_activeTime;
}

// @ fn_56_B320
void CSandworm::Descend(CStateManager& mgr, EStateMsg msg, float dt) {
  IssueGenerateCmd(msg, pas::kAS_Generate, CBCGenerateCmd(pas::EGenerateType(1), -1));
  if (msg == kStateMsg_Activate) {
    x1520_flags &= ~0x80;
    SendScriptMsgs(kSS_DeGenerate, mgr, kInvalidUniqueId, kSM_None);
    DescendHelper();
  } else if (msg == kStateMsg_Deactivate) {
    if (x998_collisionActors.get() != nullptr) {
      for (uint i = 0; i < 15; ++i) {
        const TUniqueId id =
            x998_collisionActors->GetCollisionDescFromIndex(i).GetCollisionActorId();
        if (CCollisionActor* actor = TCastToPtr< CCollisionActor >(mgr.ObjectById(id))) {
          actor->SetActive(false);
        }
      }
    }
  }
}

// @ fn_56_B50C
void CSandworm::Rise(CStateManager& mgr, EStateMsg msg, float dt) {
  if (msg == kStateMsg_Activate) {
    x8C1_flags &= ~0x80;
    x1354_ = GetTransform();
    BodyController()->SetLocomotionType(pas::ELocomotionType(1));
    SendScriptMsgs(kSS_GRNT, mgr, kInvalidUniqueId, kSM_None);
    x1668_flags |= 0x80;
    x1664_ = BodyController()->GetTurnSpeed();
    BodyController()->SetTurnSpeed(2000.f);
  } else if (msg == kStateMsg_Deactivate) {
    BodyController()->SetTurnSpeed(x1664_);
    x1524_fadeTarget = 1.f;
  }
  x1660_ = x8B0_activeTime;
  if ((x1668_flags & 0x80) != 0) {
    BodyController()->CommandMgr().SetTargetVector(PlayerPos(mgr) - GetTranslation());
  }
  IssueGenerateCmd(msg, pas::kAS_Generate, CBCGenerateCmd(pas::EGenerateType(0), -1));
}

// `lbz r0, 0x20(r3)`; `extrwi r3, r0, 1, 24` = CEntity::mActive (0x20, byte bit 0x80), the
// neg/or/srwi pair being the `!= 0` bool normalize. No marker: no FSM2 asset asks for this
// trigger (Sandworm.md l.7) and the coverage run entered it 0 times.
bool CSandworm::Activated(CStateManager& mgr, const CTriggerData& data) const {
  return GetActive();
}

// @ fn_56_C3E8
bool CSandworm::BeginUnderground(CStateManager& mgr, const CTriggerData& data) const {
  return (x8C0_flags & 0x80) != 0;
}

// @ fn_56_C3F4
void CSandworm::LurkUnderground(CStateManager& mgr, EStateMsg msg, float dt) {
  BodyController()->SetLocomotionType(pas::ELocomotionType(7));
  if (msg == kStateMsg_Activate) {
    SetTransform(MoveToScriptActor(mgr, x166C_));
  }
}

// @ fn_56_112E8
void CSandworm::Null(CStateManager& mgr, EStateMsg msg, float dt) {
  if (msg == kStateMsg_Activate) {
    SetUnderground(mgr);
    x1520_flags |= 0x80;
  }
}
