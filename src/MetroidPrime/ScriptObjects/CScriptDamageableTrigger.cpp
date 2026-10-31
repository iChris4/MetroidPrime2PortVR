#include "MetroidPrime/ScriptObjects/CScriptDamageableTrigger.hpp"

#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/SLdrDamageableTrigger.hpp"
#ifdef TARGET_PC
#include <cstdio>
#include <cstdlib>
#include <map>

#include "port_overlay.h"

// MP2_DEBUG_BALL=1: the wall of a two-stage cannon puzzle is one of these, so a damage message
// and the HP it leaves behind are the only way to see whether the missile, the cannon ball or
// neither got through.
static bool PortBallDebug() {
  static const bool enabled = std::getenv("MP2_DEBUG_BALL") != nullptr;
  return enabled;
}
#endif

static CMaterialList skDamageableTriggerMaterials(kMT_Trigger, kMT_Immovable,
                                                  kMT_NonSolidDamageable);

static CMaterialList
make_damageable_trigger_materials(CScriptDamageableTrigger::ECanOrbit canOrbit,
                                  CScriptDamageableTrigger::ESeekerLockOn seekerLockOn) {
  CMaterialList materials = skDamageableTriggerMaterials;
  if (canOrbit == CScriptDamageableTrigger::kCO_Orbit) {
    materials.Add(kMT_Orbit);
  }
  if (seekerLockOn == CScriptDamageableTrigger::kSLO_Enabled) {
    materials.Add(kMT_SeekerTarget);
  }
  return materials;
}

CEntity* LoadDamageableTrigger(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SLdrDamageableTrigger sldrThis;
#include "MetroidPrime/ScriptLoader/SLdrDamageableTrigger.inc"
#ifdef TARGET_PC
  if (PortBallDebug()) {
    std::fprintf(stderr,
                 "[ball] DTRG eid %04x loaded: %d props, power %d x%.2f, missile %d x%.2f, "
                 "cannon %d x%.2f\n",
                 info.GetEditorId().Value(), propertyCount,
                 int(sldrThis.vulnerability.power.effect),
                 static_cast< double >(sldrThis.vulnerability.power.damageMultiplier),
                 int(sldrThis.vulnerability.missile.effect),
                 static_cast< double >(sldrThis.vulnerability.missile.damageMultiplier),
                 int(sldrThis.vulnerability.cannonBall.effect),
                 static_cast< double >(sldrThis.vulnerability.cannonBall.damageMultiplier));
  }
#endif

  return rs_new CScriptDamageableTrigger(
      mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
      LdrToEntityInfo(info, sldrThis.editorProperties),
      sldrThis.editorProperties.transform.position, sldrThis.editorProperties.transform.scale,
      LdrToHealthInfo(sldrThis.health), LdrToDamageVulnerability(sldrThis.vulnerability),
      static_cast< CScriptDamageableTrigger::ECanOrbit >(sldrThis.orbitable),
      static_cast< CScriptDamageableTrigger::ESeekerLockOn >(sldrThis.enableSeekerLockOn),
      static_cast< CScriptDamageableTrigger::EInvulnerable >(sldrThis.invulnerable),
      LdrToVisorParameters(sldrThis.visor));
}

CScriptDamageableTrigger::CScriptDamageableTrigger(
    TUniqueId uid, const rstl::string& name, const CEntityInfo& info, const CVector3f& position,
    const CVector3f& extent, const CHealthInfo& health, const CDamageVulnerability& vulnerability,
    ECanOrbit canOrbit, ESeekerLockOn seekerLockOn, EInvulnerable invulnerable,
    const CVisorParameters& visor)
: CActor(uid, name, info, 0, CTransform4f::Translate(position), CModelData::CModelDataNull(),
         make_damageable_trigger_materials(canOrbit, seekerLockOn),
         CActorParameters::None().MakeDamageableTriggerActorParms(visor), kInvalidUniqueId)
, mBounds(-(0.5f * extent), 0.5f * extent)
, mHealth(health)
, mVulnerability(vulnerability)
, mDeathOriginator(kInvalidUniqueId)
, mNotOccluded(false)
, mInvulnerable(invulnerable == kIV_Invulnerable)
, mCanOrbit(canOrbit == kCO_Orbit)
, mPendingDeath(false) {}

rstl::optional_object< CAABox > CScriptDamageableTrigger::GetTouchBounds() const {
  if (GetActive() && mNotOccluded) {
    const CVector3f& position = GetTranslation();
    return CAABox(mBounds.GetMinPoint() + position, mBounds.GetMaxPoint() + position);
  }
  return rstl::optional_object_null();
}

void CScriptDamageableTrigger::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_Activate:
    if (!GetActive()) {
      mHealth.SetHP(mHealth.GetInitialHP());
      if (mCanOrbit) {
        AddMaterial(kMT_Orbit, mgr);
      }
    }
    break;
  case kSM_Damage:
#ifdef TARGET_PC
    if (PortBallDebug()) {
      std::fprintf(stderr, "[ball] trigger %04x damaged: hp %.1f/%.1f invulnerable %d\n",
                   GetUniqueId().Value(), mHealth.GetHP(), mHealth.GetInitialHP(),
                   int(mInvulnerable));
    }
#endif
    if (mHealth.GetHP() <= 0.f) {
      mDeathOriginator = msg.GetSenderId();
      if (mgr.IsMultiplayer()) {
        mDeathOriginator = mHealth.GetDamageId1();
        if (mDeathOriginator == kInvalidUniqueId) {
          mDeathOriginator = mHealth.GetDamageId2();
        }
      }
      mPendingDeath = true;
    }
    break;
  case kSM_Increment:
    mInvulnerable = true;
    break;
  case kSM_Decrement:
    mInvulnerable = false;
    break;
  default:
    break;
  }
  CActor::AcceptScriptMsg(mgr, msg);
}

void CScriptDamageableTrigger::Think(float, CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }

  const CGameArea& area = mgr.GetWorld()->GetAreaAlways(GetCurrentAreaId());
  const bool wasNotOccluded = mNotOccluded;
  mNotOccluded = area.GetOcclusionState() == CGameArea::kOS_Visible;
  if (mNotOccluded != wasNotOccluded) {
    SetTransformDirty();
  }

  if (mPendingDeath && !mInvulnerable && mHealth.GetHP() <= 0.f) {
    SendScriptMsgs(kSS_Dead, mgr, mDeathOriginator, kSM_None);
    RemoveMaterial(kMT_Orbit, mgr);
    SetActive(false);
    mPendingDeath = false;
  }
}

CHealthInfo* CScriptDamageableTrigger::HealthInfo() { return &mHealth; }

#ifdef TARGET_PC
// Beam doors (F1 > Debug > Cheats): the vulnerability of a colour-locked lock with the four beams
// (shot and charged) made Normal. Built on first use and kept per object so the pointer
// GetDamageVulnerability() hands out stays valid; ~CScriptDamageableTrigger drops it.
static std::map< const CScriptDamageableTrigger*, CDamageVulnerability > sAnyBeamVulnerability;

CScriptDamageableTrigger::~CScriptDamageableTrigger() { sAnyBeamVulnerability.erase(this); }

const CDamageVulnerability* CScriptDamageableTrigger::PortActiveVulnerability() const {
  if (!PortOverlay::UnlockBeamDoors() || !mVulnerability.PortIsColorLock()) {
    return &mVulnerability;
  }
  auto it = sAnyBeamVulnerability.find(this);
  if (it == sAnyBeamVulnerability.end()) {
    it = sAnyBeamVulnerability.emplace(this, mVulnerability.PortAnyBeam()).first;
  }
  return &it->second;
}
#endif

const CDamageVulnerability* CScriptDamageableTrigger::GetDamageVulnerability() const {
#ifdef TARGET_PC
  return PortActiveVulnerability();
#else
  return &mVulnerability;
#endif
}

EWeaponCollisionResponseTypes
CScriptDamageableTrigger::GetCollisionResponseType(const CVector3f&, const CVector3f&,
                                                   const CWeaponMode& weapon, int) const {
#ifdef TARGET_PC
  const CDamageVulnerability& vulnerability = *PortActiveVulnerability();
  return vulnerability.WeaponHits(weapon, 0) ? kWCR_OtherProjectile : kWCR_Unknown15;
#else
  return mVulnerability.WeaponHits(weapon, 0) ? kWCR_OtherProjectile : kWCR_Unknown15;
#endif
}
