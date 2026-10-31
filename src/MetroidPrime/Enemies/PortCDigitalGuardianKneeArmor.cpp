// Port-only: the DigitalGuardian REL's knee-armour actor (module 14, docs/module-analysis/
// DigitalGuardian.md section a). The retail class is 0x1A0 bytes; its vtable lbl_14_data_694
// (.data 0x694, 31 words) keeps CActor's entries except the destructor (fn_14_D460), Think
// (fn_14_CF94), AcceptScriptMsg (fn_14_D320), GetHealthInfo (fn_14_10C, the base's inline
// copy: it calls HealthInfo through the vtable), GetDamageVulnerability() (fn_14_D7BC) and
// GetTouchBounds (fn_14_D774). The comments cite the REL's section offsets.

#include "MetroidPrime/Enemies/CDigitalGuardianKneeArmor.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "MetroidPrime/CDamageVulnerability.hpp"
#include "MetroidPrime/CEchoEmitter.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"

namespace {

// The module's two flash colours (lbl_14_bss_1C and lbl_14_bss_20): the static initialiser
// fn_14_D904 builds them from .rodata 0x2EC (0.75), 0x264 (0) and 0x270 (1) at 0xD938/0xD974.
const CColor sHitColor(0.75f, 0.f, 0.f, 1.f);        // lbl_14_bss_1C
const CColor sVulnerableColor(0.f, 0.f, 0.75f, 1.f); // lbl_14_bss_20

// The retail's PassThruVulnerability is the host's CDamageVulnerability::PassThroughVulnerabilty;
// the retail's name is kept so the asm-vs-C++ check can match the call.
const CDamageVulnerability& PassThruVulnerability() {
  return CDamageVulnerability::PassThroughVulnerabilty();
}

} // namespace

// @ fn_14_D4D0
CDigitalGuardianKneeArmor::CDigitalGuardianKneeArmor(
    TUniqueId uid, const rstl::string& name, const CEntityInfo& info, uint inGrave,
    const CTransform4f& xf, const CModelData& mData, const CMaterialList& list, TUniqueId owner,
    bool vulnerable, const CActorParameters& params, const SEchoParameters& echo,
    const SLdrAudioPlaybackParms& sound)
: CActor(uid, name, info, inGrave, xf, mData, list, params, owner)
, x158_(0.f) // .rodata 0x264, the store at 0xD540
, x15C_(0.f) // the store at 0xD550
, x160_(owner)
// A two-unit box round the transform's translation (sOneVector, 0xD538..0xD59C).
, x164_(CAABox(xf.GetTranslation() - CVector3f::One(), xf.GetTranslation() + CVector3f::One()))
, x180_(sound)  // the copy at 0xD5F0..0xD61C
, x198_()       // the store at 0xD620
, x19C_24_armored_(!vulnerable) // cntlzw/srwi at 0xD5A8, the rlwimi at 0xD628
{
  // The echo emitter (0xD648): the model's bounds at the actor's transform.
  AllocateEchoEmitter(true, GetModelData()->GetBounds(GetTransform()), echo);
}

// The retail destructor destroys the sound parameters (0x180) and the CActor base; its deleting
// form's Free__7CMemoryFPCv (0xD4B0) is the compiler's.
// @ fn_14_D460
CDigitalGuardianKneeArmor::~CDigitalGuardianKneeArmor() {}

// Arms (dg30): the hit flash with and without time left (the Damage message), armoured and
// vulnerable (InternalMessage1..4 through fn_14_3A44), the sound handle set and not, the echo
// emitter null and not (tools/cov/specs/digitalguardian_dg30_knees.py, forced). Two arms are dead:
// - the touch box's engaged test (0xD204, the optional's flag 0x17C): the ctor engages it (the
//   stb of 1 at 0xD5AC) and nothing disengages it, so the assignment always takes 0xD24C;
// - HasModelData's false arms (no model data 0xD294, no animation and no model 0xD2AC):
//   fn_14_4080 builds a knee only with the static model loaded (the xF58 test at 0x4098) and
//   passes it (0xF0C, the addi at 0x4250); the ctor reads its bounds unconditionally (0xD634),
//   and nothing clears a knee's model.
// The bit test at 0xCFC0 is GetActive (CEntity 0x20); 0xD19C's x18F bits are the maxVolume byte.
// @ fn_14_CF94
void CDigitalGuardianKneeArmor::Think(float dt, CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }
  CActor::Think(dt, mgr);

  // The hit flash: x158_ counts down to zero and t is its share of the 1.5 s flash (.rodata
  // 0x2A4, the fdivs at 0xD008).
  float t = 0.f;
  if (x158_ > 0.f) {
    float left = x158_ - dt;
    if (!(0.f < left)) { // fcmpo 0, left / bge at 0xCFE8 to the fmr of 0: a NaN becomes 0
      left = 0.f;
    }
    x158_ = left;
    t = x158_ / 1.5f;
  }

  if (x19C_24_armored_) {
    if (t > 0.f) {
      SetModelFlags(CModelFlags(CModelFlags::kT_Two, 0,
                                static_cast< CModelFlags::EFlags >(3),
                                CColor::Lerp(CColor::Black(), sHitColor, t)));
    } else {
      SetModelFlags(CModelFlags(CModelFlags::kT_Two, 0, static_cast< CModelFlags::EFlags >(3),
                                CColor::Black()));
    }
  } else {
    // The vulnerable pulse: |sin(4 pi * phase)| (.rodata 0x2C4, the fmuls at 0xD0B8).
    x15C_ += dt;
    const float pulse = CMath::AbsF(CMath::FastSinR(12.566371f * x15C_));
    CColor color = CColor::Lerp(CColor::Black(), sVulnerableColor, pulse);
    if (t > 0.f) {
      color = CColor::Lerp(color, sHitColor, t);
    }
    SetModelFlags(CModelFlags(CModelFlags::kT_Two, 0, static_cast< CModelFlags::EFlags >(3),
                              color));
  }

  // The vulnerable sound follows the armour: the translation (guest 0x54), the transform's
  // forward column (0x28/0x38/0x48) and the sound's max volume (the low byte of the word at
  // 0x18C, the clrlwi at 0xD19C).
  if (x198_ != CSfxHandle()) {
    CSfxManager::UpdateEmitter(x198_, GetTranslation(), GetTransform().GetColumn(kDY),
                               static_cast< uchar >(x180_.maxVolume));
  }

  // The touch bounds: the two-unit box round the translation (0xD1A4..0xD278).
  x164_ = CAABox(GetTranslation() - CVector3f::One(), GetTranslation() + CVector3f::One());

  // The echo emitter's bounds follow the model (0xD27C..0xD2F8).
  if (EchoEmitter() != nullptr && HasModelData()) { // the retail's tests at 0xD27C..0xD2A0
    EchoEmitter()->SetBounds(GetModelData()->GetBounds(GetTransform()));
  }
}

// @ fn_14_D320
void CDigitalGuardianKneeArmor::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CActor::AcceptScriptMsg(mgr, msg);
  switch (msg.GetMessage()) {
  case kSM_Create: // "XCRT", the case at 0xD380
    if (x19C_24_armored_) {
      mgr.GetPlayer(0)->SetOrbitRequestForTarget(GetUniqueId(), CPlayer::kOR_ActivateOrbitSource,
                                                 mgr);
      RemoveMaterial(kMT_Target, kMT_SeekerTarget, mgr);
    } else {
      AddMaterial(kMT_Target, kMT_SeekerTarget, mgr);
    }
    SetVisorOrbitableFlags(CVisorParameters::kVOF_All, true);
    SetVisorOrbitableFlags(CVisorParameters::kVOF_Echo, false);
    break;
  case kSM_Delete: // "XDEL", the case at 0xD3FC
    if (x198_ != CSfxHandle()) {
      CSfxManager::RemoveEmitter(x198_);
      x198_ = CSfxHandle();
    }
    break;
  case kSM_Damage: // "XDMG", the case at 0xD424: the hit flash (.rodata 0x2A4, 1.5 s)
    x158_ = 1.5f;
    if (EchoEmitter() != nullptr) { // the retail's test at 0xD440
      EchoEmitter()->SetAutoReducingDamage(x158_);
    }
    break;
  default:
    break;
  }
}

// fn_14_D7BC: always pass-through (0xD7BC..0xD7D8).
// @ fn_14_D7BC
const CDamageVulnerability* CDigitalGuardianKneeArmor::GetDamageVulnerability() const {
  return &PassThruVulnerability(); // the call at 0xD7C8
}

// fn_14_D774: the copy of the optional touch box (its flag 0x17C, the beqlr at 0xD784 skips the
// box when clear). The empty arm is dead: the ctor engages x164_ (the stb of 1 at 0xD5AC) and
// Think only assigns it (0xD204..0xD278).
// @ fn_14_D774
rstl::optional_object< CAABox > CDigitalGuardianKneeArmor::GetTouchBounds() const {
  return x164_;
}

// fn_14_CE1C: with the flag (the clrlwi. at 0xCE28) the orbit material (0x29 = kMT_Orbit) is
// added (0xCE48); without it, it is removed (0xCE58) and the player is asked to break any orbit
// on this armour (request 8, kOR_ActivateOrbitSource, the li at 0xCE68 and the call at 0xCE78).
// @ fn_14_CE1C
void CDigitalGuardianKneeArmor::SetOrbitable(CStateManager& mgr, bool orbitable) {
  if (orbitable) {
    AddMaterial(kMT_Orbit, mgr);
  } else {
    RemoveMaterial(kMT_Orbit, mgr);
    mgr.GetPlayer(0)->SetOrbitRequestForTarget(GetUniqueId(), CPlayer::kOR_ActivateOrbitSource,
                                               mgr);
  }
}

// fn_14_CE94: no change -> nothing (the cmplw/beq at 0xCEBC). Otherwise the flag bit (the rlwimi
// at 0xCEC8) and the pulse phase 0x15C = 0.0f (0xCED4); vulnerable: the looped sound when none
// plays (0xCEEC, PlayCustomSound at 0xCF20) and the target materials 0x28/0x3F (0xCF3C);
// armoured: the sound removed when set (0xCF4C, 0xCF5C) and the materials removed. Arms (dg30):
// knee 0 with no handle, knee 2 with one (the spec digitalguardian_dg30_knees.py, forced).
// @ fn_14_CE94
void CDigitalGuardianKneeArmor::SetArmored(CStateManager& mgr, bool armored) {
  if (x19C_24_armored_ == armored) {
    return;
  }
  x19C_24_armored_ = armored;
  x15C_ = 0.f;
  if (!x19C_24_armored_) {
    if (x198_ == CSfxHandle()) {
      x198_ = PlayCustomSound(GetTranslation(), GetTransform().GetColumn(kDY), x180_, true);
    }
    AddMaterial(kMT_Target, kMT_SeekerTarget, mgr);
  } else {
    if (x198_ != CSfxHandle()) {
      CSfxManager::RemoveEmitter(x198_);
      x198_ = CSfxHandle();
      RemoveMaterial(kMT_Target, kMT_SeekerTarget, mgr); // only with a live emitter (asm 0xCF4C beq)
    }
  }
}
