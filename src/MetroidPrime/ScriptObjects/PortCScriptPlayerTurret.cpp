// Host body for the ScriptPlayerTurret REL (module 63), milestone 1: the turret loads, registers,
// takes kSM_Create and thinks. The retail module is the shooting gallery's player turret: a
// script object with no model of its own that owns the turret base actor (the one with the
// model) and the turret top actor, and that the player enters to aim.
//
// Which methods the REL overrides is not guessed: its vtable lbl_63_data_40 lists them by
// address and the slots line up with CActor's declaration order (slot 5 Think, slot 6
// AcceptScriptMsg, slot 9 PreRender, slot 18 GetTouchBounds; the entries in between are the
// DOL's own CActor/CEntity symbols). AcceptScriptMsg (fn_63_1BE8, 0x784 bytes) has its message
// switch ported: the player enters the turret (kSM_Activate), leaves it (kSM_Deactivate) and the
// turret starts (kSM_Start), with the rest of retail's cases. Think (fn_63_23C0, 0x968 bytes)
// and PreRender (fn_63_2D68, 0x2CC) are ported too, and the five member functions the module
// hands the DOL through SPlayerTurret_FuncPtrs are ported, so the turret camera, the exit path
// and the turret HUD keep working. The analysis is docs/module-analysis/ScriptPlayerTurret.md.
#include "MetroidPrime/ScriptObjects/CScriptActor.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPlayerTurret.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Animation/CharacterCommon.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "Kyoto/Math/CVector2f.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CAnimPlaybackParms.hpp"
#include "MetroidPrime/CControlMapper.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Weapons/CEnergyProjectile.hpp"
#include "MetroidPrime/Weapons/CImpactVisorEffect.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/SFX/SamusImpact_MP.h"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/SLdrPlayerTurret.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "Weapons/CWeaponDescription.hpp"

#include <math.h>
#include <stdio.h>

namespace {

// lbl_63_rodata_60 + 0x26: the muzzle locator the module's static initializer fn_63_376C copies
// into lbl_63_bss_C (0x00003788).
const char* const kMuzzleLocator = "muzzleend_LCTR";

// lbl_63_bss_1C: the module's own damage colour, built by the module's initializer fn_63_376C at
// 0x000037A4 as CColor(0.5, 0, 0, 1).
const CColor kTurretFlashColor(0.5f, 0.f, 0.f, 1.f);

// The guest turns -1 into the invalid sfx id before storing it (fn_63_144 at 0x000004A0; the
// five tests are at 0x4A4, 0x4BC, 0x4D8, 0x4F0 and 0x50C).  The -1 arm does not run in the
// shipped data: all three shipped turrets (the shooting gallery, the Agon Wastes and the
// Sanctuary Fortress) have real sound ids - a once-per-arm probe of the run in each area printed
// "144_sfx_id_valid" and never "144_sfx_id_is_minus_one".
ushort TranslateSfx(int id) {
  return CSfxManager::TranslateSFXID(id == -1 ? CSfxManager::kInternalInvalidSfxId
                                             : static_cast< ushort >(id));
}

// The difference of two analog inputs is clamped to -1..1: the magnitude is compared with 1.0
// (lbl_63_rodata_10) and the value keeps its sign (lbl_63_rodata_1C = -1). The turret's input
// path uses it at 0x00001310 and 0x0000137C.
float ClampAxis(float value) {
  if (fabs(value) > 1.f) {
    return value >= 0.f ? 1.f : -1.f;
  }
  return value;
}

} // namespace

// fn_63_30E8 (0x3B4). The base call is the retail one: inGrave 0 (`li r7, 0x0` at 0x00003210),
// an empty CModelData (the turret has no model of its own; fn_63_374C at 0x00003268), the
// material list lbl_63_data_0 = 0x15 through the DOL's __shl2i helper (0x000031C8) and
// kInvalidUniqueId as the next draw node (0x000031A4).
//
// The turret's own fields are then written one by one: the one-unit box around the actor's
// position (0x00003240), the flags and the eight angles and speeds (0x0000328C..0x00003308), the
// editor transform's forward vector and translation (0x0000332C, 0x000033FC), the light colour
// (0x00003348), the two weapon effects (0x0000337C), the empty token (fn_63_349C at
// 0x00003384), the weapon damage (0x00003398..0x000033EC), the sound ids (0x00003418) and the
// flag byte at 0x22C with its two top bits cleared (0x0000342C).
// fn_63_0 (0x2C): the vtable's slot 15 - the module's own thunk for the base's HealthInfo.  The
// guest loads the vtable (0x0C), takes its slot 12 (the base's HealthInfo, 0x10) and calls it
// through the count register (0x18), so the port forwards to the same virtual call.
// @ fn_63_0
const CHealthInfo* CScriptPlayerTurret::GetHealthInfo() const {
  return const_cast< CScriptPlayerTurret* >(this)->HealthInfo();
}

// fn_63_30E8 (0x3B4): the constructor - the turret's own fields.
// @ fn_63_30E8
CScriptPlayerTurret::CScriptPlayerTurret(TUniqueId uid, const rstl::string& name,
                                         const CEntityInfo& info, const CTransform4f& xf,
                                         const CActorParameters& actParms, uint flags,
                                         float maxHorizRotationLeft, float maxHorizRotationRight,
                                         float maxVertElevationUp, float maxVertElevationDown,
                                         float damageAngle, float horizSpeed, float vertSpeed,
                                         float fireRate, const CDamageInfo& weaponDamage,
                                         CAssetId weaponEffect, CAssetId weaponEffectMultiPlayer,
                                         ushort sfxTurretRotation, ushort sfxSinglePlayerImpact,
                                         ushort sfxMultiPlayerImpact,
                                         ushort sfxSinglePlayerProjectile,
                                         ushort sfxMultiPlayerProjectile)
: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(kMT_CameraPassthrough), actParms,
         kInvalidUniqueId)
, mBounds(CAABox::MakeMaxInvertedBox()) // guest 0x158, filled in below
, mFlags(flags)                        // guest 0x170
, mMaxHorizRotationLeft(maxHorizRotationLeft)   // guest 0x174
, mMaxHorizRotationRight(maxHorizRotationRight) // guest 0x178
, mMaxVertElevationUp(maxVertElevationUp)       // guest 0x17C
, mMaxVertElevationDown(maxVertElevationDown)   // guest 0x180
, mMaxAimAngle(damageAngle)                     // guest 0x184
, mPlayerId(kInvalidUniqueId)                   // guest 0x188
, mHorizSpeed(horizSpeed)                       // guest 0x18C
, mVertSpeed(vertSpeed)                         // guest 0x190
, mFireRate(fireRate)                           // guest 0x194
, mBaseActorId(kInvalidUniqueId)                // guest 0x198
, mTopActorId(kInvalidUniqueId)                 // guest 0x19A
, mAnimId1(0)                                   // guest 0x19C
, mAnimId2(0)                                   // guest 0x1A0
, mUnknown1A4(0)                                // guest 0x1A4
, mUnknown1A8(0)                                // guest 0x1A8
, mUnknown1AC(0)                                // guest 0x1AC
, mPitchTarget(0.f)                           // guest 0x1B0
, mPitch(0.f)                            // guest 0x1B4
, mUnknown1B8(0.f)                              // guest 0x1B8
, mUnknown1BC(0.f)                              // guest 0x1BC
, mUnknown1C0(0.f)                              // guest 0x1C0
, mBaseForward(xf.GetForward())                 // guest 0x1C4
, mYawTarget(0.f)                              // guest 0x1D0
// guest 0x1D4: the model flags the base actor had: the colour (1, 1, 1, 1) from the CColor
// constructor at 0x00003348, the blend mode 0 and the material set 0 and the flags 3 from the
// stack slots the constructor fills at 0x0000333C..0x00003344.
, mModelFlags(CModelFlags(CModelFlags::kT_Opaque, CColor(1.f, 1.f, 1.f, 1.f)))
, mWeaponEffect(weaponEffect)                   // guest 0x1E0
, mWeaponEffectMulti(weaponEffectMultiPlayer)   // guest 0x1E4
// guest 0x1E8: the empty token of fn_63_349C (0x00003384); the host's null pointer is the
// same thing, an object reference with no object in it.
, mToken(nullptr)
, mWeaponDamage(weaponDamage)                   // guest 0x1F0
, mUnknown20C(0.f)                              // guest 0x20C
, mTargetPosition(xf.GetTranslation())          // guest 0x210
, mSfxHandle()                                  // guest 0x21C
, mHullActorId(kInvalidUniqueId)                // guest 0x220
, mSfxTurretRotation(sfxTurretRotation)        // guest 0x222
, mSfxSinglePlayerImpact(sfxSinglePlayerImpact) // guest 0x224
, mSfxMultiPlayerImpact(sfxMultiPlayerImpact)   // guest 0x226
, mSfxSinglePlayerProjectile(sfxSinglePlayerProjectile) // guest 0x228
, mSfxMultiPlayerProjectile(sfxMultiPlayerProjectile)   // guest 0x22A
, mPlayerInTurret(false)                        // guest 0x22C bit 0x80
, mUnknown22C_6(false)                          // guest 0x22C bit 0x40
{
  // fn_63_30E8 at 0x00003398: the weapon damage is copied from the loader's argument; 0x00003444
  // then overwrites its damage sound id with the module's own constant, 0x2812 (the Samus heavy
  // hit sound).
  mWeaponDamage.SetDamageSfxId(SFXsa2_r_hitheavy_00_oneshot);
  // fn_63_30E8 at 0x00003240: the bounds are the actor's position grown by half a unit
  // (lbl_63_rodata_5C = 0.5).
  const CVector3f position = GetTranslation();
  mBounds = CAABox(position - CVector3f(0.5f, 0.5f, 0.5f),
                   position + CVector3f(0.5f, 0.5f, 0.5f));
}

// fn_63_3070 (0x78): the vtable is reset, the token at 0x1E8 is destroyed and CActor's
// destructor runs.
// fn_63_3070 (0x78): the destructor - the vtable is reset, the token at 0x1E8 is destroyed
// and CActor's destructor runs.  The host's own destructor chain does all three (the members are
// destroyed by it and the pool frees the object), so the body is empty.  The guest's three
// branches are the compiler's own: the "delete this" test (0x308C), the placement-new test of
// the token's address (0x30A0) and the free's "is it a heap block" test (0x30C0).
// @ fn_63_3070
CScriptPlayerTurret::~CScriptPlayerTurret() {}

// vtable slot 5 = fn_63_23C0 (0x968): the turret's aiming, its sounds and the hull transform.
// Retail does not call CActor::Think.
// @ fn_63_23C0
void CScriptPlayerTurret::Think(float dt, CStateManager& mgr) {
  // 0x2400: only an active turret thinks (the guest's byte 0x20 bit 0x80, CEntity's active flag).
  if (!GetActive()) {
    return;
  }
  // 0x2414: the yaw the input asked for, read before this frame zeroes it (0x2788).
  const float yawInput = mYawTarget;
  // 0x2418: the damage flash timer counts down (lbl_63_rodata_8 = 0.0).
  if (mUnknown20C > 0.f) {
    mUnknown20C -= dt;
  }
  // 0x2438: the player in the turret (0x188) and the base actor (0x198).
  CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
  CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
  // 0x2460
  if (base != nullptr) {
    // 0x2474: this frame's yaw step, the input times the horizontal speed (0x18C) and dt.
    const CQuaternion yawStep = CQuaternion::ZRotation(
        CRelAngle::FromRadians(dt * (mYawTarget * mHorizSpeed)));
    // 0x24C0: the base actor's forward vector, flattened onto the ground plane.
    const CVector3f baseForward = base->GetTransform().GetForward();
    const CVector2f baseFlat =
        CVector2f(baseForward.GetX(), baseForward.GetY()).AsNormalized();
    const CVector3f current(baseFlat.GetX(), baseFlat.GetY(), 0.f);
    CVector3f aim = current;
    // 0x2518: a yaw input (lbl_63_rodata_24 = 1e-05) turns the base actor's direction.
    // 0x251C blt skips on |yaw| < 1e-05 only, so a NaN yaw turns (`!(a < b)`, not `a >= b`).
    if (!(fabs(mYawTarget - 0.f) < 1e-05f)) {
      // 0x252C
      aim = yawStep.Transform(current);
    }
    // 0x2564: the turret's own forward vector, flattened onto the ground plane.
    const CVector3f turretForward = GetTransform().GetForward();
    const CVector2f turretFlat =
        CVector2f(turretForward.GetX(), turretForward.GetY()).AsNormalized();
    const CVector3f rest(turretFlat.GetX(), turretFlat.GetY(), 0.f);
    // 0x2594: a player that is not in control (its turret state, the guest's player + 0x12F8, is
    // not 3 = kTS_Active) lets the base actor swing back towards the turret's forward, four times
    // (lbl_63_rodata_14) as fast.
    if (player != nullptr && player->GetTurretState() != CPlayer::kTS_Active) {
      // 0x25C0
      const CRelAngle maxAngle = CRelAngle::FromRadians(4.f * (dt * mHorizSpeed));
      // 0x25EC
      const CQuaternion swing =
          CQuaternion::LookAt(CUnitVector3f(current), CUnitVector3f(rest), maxAngle);
      // 0x261C
      aim = swing.Transform(current);
    }
    // 0x2660: the cosine between the new direction and the turret's forward.
    const float cosine = CVector3f::Dot(aim, rest);
    // 0x266C: lbl_63_rodata_28 = 0.0001
    if (fabs(cosine) >= 0.0001f) {
      // 0x2684: fn_63_2D28 (0x40) is the cross product (the guest's inline of CVector3f::Cross);
      // its sign of z tells the side.
      const CVector3f side = CVector3f::Cross(aim, rest);
      // 0x2694
      if (side.GetZ() >= 0.f) {
        // 0x26A4: past the right limit (0x178) the base actor keeps its own direction.
        if (static_cast< float >(acos(cosine)) > mMaxHorizRotationRight) {
          aim = base->GetTransform().GetForward();
        }
      } else {
        // 0x26D8: past the left limit (0x174) the base actor keeps its own direction.
        if (static_cast< float >(acos(cosine)) > mMaxHorizRotationLeft) {
          aim = base->GetTransform().GetForward();
        }
      }
      // 0x2704: the base actor's position (the guest's actor + 0x54).
      const CVector3f position = base->GetTranslation();
      // 0x2720
      if (aim.IsMagnitudeSafe()) {
        // 0x2770: the base actor looks along the new direction from where it stands.
        base->SetTransform(CTransform4f::LookAt(position, position + aim, CVector3f::Up()));
      }
    }
    // 0x2788: the yaw input is used up.
    mYawTarget = 0.f;
  }

  // 0x2798: this frame's pitch step, the vertical speed (0x190) times dt.
  float pitchStep = dt * mVertSpeed;
  bool inControl = true;
  // 0x279C: a player that is not in control (turret state != 3, kTS_Active) lets the pitch go
  // back to zero, four times (lbl_63_rodata_14) as fast and without the ease-out.
  if (player != nullptr && player->GetTurretState() != CPlayer::kTS_Active) {
    inControl = false;
    pitchStep *= 4.f;
    // 0x27C4
    mPitchTarget = 0.f;
  }
  // 0x27D8: the distance to the pitch the input asks for (lbl_63_rodata_2C = 0.0017453292).
  const float pitchDelta = mPitchTarget - mPitch;
  // 0x27E8 blt skips on |delta| < eps only, so a NaN delta takes the move arm.
  if (!(fabs(pitchDelta) < 0.0017453292f)) {
    // 0x27FC: the distance over a quarter turn (lbl_63_rodata_30 = 1.5707964), clamped to 0..1.
    const float t = CMath::Clamp(0.f, static_cast< float >(fabs(pitchDelta / 1.5707964f)), 1.f);
    // 0x2834: the ease-out, PhongBlob with the exponent 8 (lbl_63_rodata_34).
    float scale = 1.f - CMath::PhongBlob(t, 8.f);
    // 0x283C
    if (!inControl) {
      scale = 1.f;
    }
    // 0x2854
    const float step = pitchStep * scale;
    // 0x285C
    if (pitchDelta > 0.f) {
      // 0x2870: up towards the target, no further than it (lbl_63_rodata_38 = -3.1415927).
      mPitch = CMath::Clamp(-3.1415927f, mPitch + step, mPitchTarget);
    } else {
      // 0x28A8: down towards the target, no further than it (lbl_63_rodata_3C = 3.1415927).
      mPitch = CMath::Clamp(mPitchTarget, mPitch - step, 3.1415927f);
    }
  } else {
    // 0x28D8
    mPitch = mPitchTarget;
  }
  // 0x28DC: the pitch stays within the elevation limits (the guest's 0x180 down, 0x17C up):
  // 0x28EC: below the lower limit -> -down; 0x28F8: above the upper limit -> up; else the
  // pitch (0x2908).
  if (-mMaxVertElevationDown > mPitch) {
    mPitch = -mMaxVertElevationDown;
  } else if (mMaxVertElevationUp < mPitch) {
    mPitch = mMaxVertElevationUp;
  }

  // 0x290C: the base actor's two additive animations (0x19C up, 0x1A0 down) show the pitch. The
  // guest reads the model pointer (actor + 0x60) without a test; a port limit: the host allows an
  // actor without a model.
  if (base != nullptr && base->GetModelData() != nullptr &&
      base->GetModelData()->GetAnimationData() != nullptr) {
    CAnimData* animData = base->AnimationData();
    // 0x2938
    if (mPitch >= 0.f) {
      // 0x295C: the weight is the pitch over a quarter turn (lbl_63_rodata_30), clamped to 0..1.
      const float weight = CMath::Clamp(0.f, mPitch / 1.5707964f, 1.f);
      // 0x298C
      animData->AddAdditiveAnimation(mAnimId1, weight, false, false);
      // 0x29B0
      animData->AddAdditiveAnimation(mAnimId2, 0.f, false, false);
    } else {
      // 0x29C8
      const float weight = CMath::Clamp(0.f, -mPitch / 1.5707964f, 1.f);
      // 0x2A00
      animData->AddAdditiveAnimation(mAnimId2, weight, false, false);
      // 0x2A24
      animData->AddAdditiveAnimation(mAnimId1, 0.f, false, false);
    }
  }

  // 0x2A38: the top actor (0x19A).
  CActor* top = TCastToPtr< CActor >(mgr.ObjectById(mTopActorId));
  // 0x2A48: the top actor goes onto the base actor's "upwardballsnap_LCTR" locator
  // (lbl_63_rodata_60 + 7). Same port limit for the model pointer as above.
  if (base != nullptr && top != nullptr && base->GetModelData() != nullptr) {
    // 0x2A74
    const CTransform4f locator =
        base->GetModelData()->GetLocatorTransform("upwardballsnap_LCTR");
    // 0x2A90
    top->SetTransform(base->GetTransform() * locator);
  }

  // 0x2A9C: the rotation sound runs only while the player is in control (turret state 3).
  if (player != nullptr && player->GetTurretState() == CPlayer::kTS_Active) {
    // 0x2AD8: the "still" threshold, 1 - 60 * (lbl_63_rodata_44 = 1.001358e-05) * dt
    // (lbl_63_rodata_40 = 60).
    const float stillThreshold = 1.f - 60.f * (1.001358e-05f * dt);
    // 0x2AD4: last frame's alignment.
    const float lastAlignment = mUnknown1C0;
    // 0x2ADC
    if (top == nullptr) {
      return;
    }
    // 0x2B04: the alignment is the cosine between the top actor's forward vector and last
    // frame's one (0x1C4..0x1CC), clamped to -1..1 keeping its sign (lbl_63_rodata_1C = -1).
    const CVector3f topForward = top->GetTransform().GetForward();
    float alignment = CVector3f::Dot(topForward, mBaseForward);
    if (fabs(alignment) > 1.f) {
      // 0x2B20
      alignment = 1.f * (alignment >= 0.f ? 1.f : -1.f);
    }
    // 0x2B28
    mUnknown1C0 = alignment;
    // 0x2B50: no pitch input (lbl_63_rodata_48 = 0.05) and no yaw input count as still.
    if (fabs(mPitchTarget - 0.f) < 0.05f && fabs(yawInput) <= dt) {
      // 0x2B74
      mUnknown1C0 = 1.f;
    }
    // 0x2B78: this frame's forward vector is the next frame's reference.
    mBaseForward = topForward;
    // 0x2B7C: the turret started or stopped moving this frame.
    bool changed = false;
    if (lastAlignment >= stillThreshold && mUnknown1C0 < stillThreshold) {
      // 0x2BB8
      changed = true;
    } else if (lastAlignment < stillThreshold && mUnknown1C0 >= stillThreshold) {
      // 0x2BB8
      changed = true;
    }
    // 0x2BBC
    if (changed) {
      // 0x2BCC: the debounce restarts.
      mUnknown1BC = 0.f;
    } else if (mUnknown1BC < 0.05f) {
      // 0x2BE8: the debounce (lbl_63_rodata_48 = 0.05) runs up to its end.
      float timer = mUnknown1BC + dt;
      // 0x2BF0 bge L: timer >= 0.05 or NaN gives 0.05.
      if (!(timer < 0.05f)) {
        timer = 0.05f;
      }
      // 0x2BFC
      mUnknown1BC = timer;
      // 0x2C0C: on the frame it ends the sound starts or stops.
      if (0.05f == mUnknown1BC) {
        // 0x2C18
        if (mUnknown1C0 < stillThreshold) {
          // 0x2C28: moving: the rotation sound (0x222) starts, unless it already runs.
          if (!mSfxHandle) {
            // 0x2C60: the volume 0x7f, the player's sound pan 4 (kMSP_Player), the turret's area
            // (the guest's CEntity + 0x4), acoustics and looped.
            mSfxHandle = CSfxManager::SfxStart(
                mSfxTurretRotation, 0x7f, player->GetSoundPan(CPlayer::kMSP_Player),
                GetCurrentAreaId().Value(), true, true, CSfxManager::kMedPriority);
          }
        } else {
          // 0x2C7C: still: the sound stops.
          CSfxManager::SfxStop(mSfxHandle);
          // 0x2C84
          mSfxHandle = CSfxHandle();
        }
      }
    }
    // 0x2C90
    if (mSfxHandle) {
      // 0x2CD0: the pitch bend follows the pitch: 8192 (lbl_63_rodata_4C) plus 4000
      // (lbl_63_rodata_50) per quarter turn (lbl_63_rodata_30).
      CSfxManager::PitchBend(mSfxHandle,
                             static_cast< int >(4000.f * (mPitch / 1.5707964f) + 8192.f));
    }
  } else if (mSfxHandle) {
    // 0x2CF0: no player in control: the sound stops. Retail leaves the handle at 0x21C as it is.
    CSfxManager::SfxStop(mSfxHandle);
  }
}

// vtable slot 6 = fn_63_1BE8 (0x784): the turret's script messages.
//
// Retail switches on the message's four-character code (the comparison tree at 0x1BE8..0x1C98)
// and every case ends in CActor::AcceptScriptMsg (0x2348). The cases ported here are retail's, in
// retail's order:
//
//   kSM_Create     (0x58435254, asm 0x1C9C): the hull's model token.
//   kSM_Activate   (0x41435456, asm 0x1DA4): the player enters the turret.
//   kSM_Deactivate (0x44435456, asm 0x1F58): the player leaves it.
//   kSM_Start      (0x53545254, asm 0x1FB0): the turret starts; its top actor becomes solid.
//   kSM_Stop       (0x53544F50, asm 0x2008): the player is ejected and killed.
//   kSM_AreaLoaded (0x58414C44, asm 0x2244): the hull's actors (only the two connections of
//                                               fn_63_15B0 are ported).
//   kSM_Damage     (0x58444D47, asm 0x224C): the damage flash and the impact sound.
// @ fn_63_1BE8
void CScriptPlayerTurret::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // 0x1C00: the message's sender (its +0, a u16) and 0x1C0C its originator (its +2, a u16);
  // 0x1C04: the message's code (its +8, a u32) is compared with the six four-character codes.
  switch (msg.GetMessage()) {
  case kSM_Create: {
    // 0x1CA0: the multi-player weapon effect (0x1E4) in multiplayer, the single-player one
    // (0x1E0) otherwise.  The guest keeps the low byte of the game-mode test (0x1CA4,
    // `clrlwi. r0, r3, 24`), so it is a plain bool.
    const CAssetId effect = mgr.IsMultiplayer() ? mWeaponEffectMulti : mWeaponEffect;
    // 0x1CC0: the resource type of that asset.
    const FourCC type = gpResourceFactory->GetResourceTypeById(effect);
    // 0x1CF0: its token becomes the turret's own token (0x1E8) - the guest's copy assignment at
    // 0x1D08.
    mToken = rs_new CToken(gpSimplePool->GetObj(SObjectTag(type, effect)));
    break;
  }
  case kSM_Activate: {
    // 0x1DA4: the message's originator (the guest's message + 0x2) is the player that entered
    // the trigger; the turret remembers it as its player.
    mPlayerId = msg.GetOriginator();
    CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
    // 0x1DC8: the player's health info comes from the vtable (the guest's slot at +0x3C) and its
    // health is the float at + 4 of it (0x1DDC, the guest's CHealthInfo starts with the initial
    // health), and a dead player cannot enter the turret.  The null test is the guest's own
    // (0x1DC0: the TCastToPtr<CPlayer> result is tested before the health is read).
    if (player != nullptr && player->GetHealthInfo()->GetHP() <= 0.f) {
      // 0x1DE4/0x1DE8: the guest's test is (hp == 0.0) | (hp < 0.0) against lbl_63_rodata_8 = 0.
      // 0x1DEC: the guest returns here - the rest of the case and CActor::AcceptScriptMsg are
      // both skipped (the branch goes to the function's epilogue at 0x2358).
      return;
    }
    if (player != nullptr) {
      // 0x1DF0: the turret's own unique id (the guest's + 0x8) is what the player enters.
      player->StartTurret(GetUniqueId(), mgr);
      // 0x1E0C: the flag byte at 0x22C gains its bit 7 (0x80) - the guest's
      // `rlwimi r0, r3, 7, 24, 24` with r3 = 1.  The player is in the turret.
      mPlayerInTurret = true;
    }
    // 0x1E1C/0x1E30: both rotations start at zero (lbl_63_rodata_8 = 0.0; the guest stores it at
    // 0x1B4 and 0x1B0).
    mPitch = 0.f;
    mPitchTarget = 0.f;
    // 0x1E38: the base actor (0x198) and 0x1E48 the top actor (0x19A).
    CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
    CActor* top = TCastToPtr< CActor >(mgr.ObjectById(mTopActorId));
    // 0x1E70: the base actor's model is read without a test in the guest - a port limit, the host
    // allows an actor without a model.
    CModelData* model = base != nullptr ? base->ModelData() : nullptr;
    // 0x1E64: the base actor needs its animation data (0x1E78), and the top actor has to exist
    // (0x1E80).
    if (base != nullptr && model != nullptr && model->GetAnimationData() != nullptr &&
        top != nullptr) {
      // 0x1E88: the base actor keeps its own position (its 0x54, 0x58 and 0x5C) but takes the
      // turret's rotation (the guest's transform at 0x24).
      const CVector3f position = base->GetTranslation();
      base->SetTransform(GetTransform());
      base->SetTranslation(position);
      // 0x1EC4: the first additive animation (0x19C) at weight 0 (lbl_63_rodata_8).
      base->AnimationData()->AddAdditiveAnimation(mAnimId1, 0.f, false, false);
      // 0x1EDC: the second additive animation (0x1A0) at weight 0.
      base->AnimationData()->AddAdditiveAnimation(mAnimId2, 0.f, false, false);
      // 0x1F00: the locator is lbl_63_rodata_60 + 0x7, the static string "upwardballsnap_LCTR".
      const CTransform4f locator = model->GetLocatorTransform("upwardballsnap_LCTR");
      // 0x1F30: the locator in the base actor's frame, times the base actor's transform (the
      // guest's `__ml__` at 0x1F30 with the base actor's transform at 0x24).
      top->SetTransform(base->GetTransform() * locator);
    }
    // 0x1F48: the damage flash timer (0x20C) is 0 (lbl_63_rodata_8).
    mUnknown20C = 0.f;
    break;
  }
  case kSM_Deactivate: {
    // 0x1F58: the player that was in the turret (0x188) leaves it.
    CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
    if (player != nullptr) {
      // 0x1F7C: the state is 0 (kTS_None).
      player->SetTurretState(CPlayer::kTS_None, mgr);
    }
    // 0x1F84/0x1F98: the turret has no player any more.
    mPlayerId = kInvalidUniqueId;
    // 0x1F9C: the vtable's slot 7 with 0 - the turret is deactivated.
    SetActive(false);
    break;
  }
  case kSM_Start: {
    // 0x1FB0: only an active turret starts: the guest's byte 0x20 is CEntity's active flag (the
    // DOL's CEntity::SetActive writes 0/1 there) and the guest tests it with
    // `extrwi. r0, r0, 1, 24` (0x1FB4).
    if (!GetActive()) {
      break;
    }
    // 0x1FBC: the player in the turret (0x188).
    CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
    if (player != nullptr) {
      // 0x1FE0: the state is 3 (kTS_Active).
      player->SetTurretState(CPlayer::kTS_Active, mgr);
    }
    // 0x1FF0: fn_63_14B4 - the top actor becomes solid.
    AddSolidToTopActor(mgr);
    // 0x1FFC: the material is 0x29 (kMT_Orbit): the turret orbits the player.
    AddMaterial(kMT_Orbit, mgr);
    break;
  }
  case kSM_Stop: {
    // 0x2008: only an active turret stops: the guest's byte 0x20 is CEntity's active flag (the
    // DOL's CEntity::SetActive writes 0/1 there) and the guest tests it with
    // `extrwi. r0, r0, 1, 24` (0x200C).
    if (!GetActive()) {
      break;
    }
    // 0x201C: the turret loses the orbit material (0x29).
    RemoveMaterial(kMT_Orbit, mgr);
    // 0x2020: the base actor (0x198) gets the turret's saved model flags back (the guest's
    // 0x1D4..0x1DC, copied field by field at 0x2040..0x2064).
    CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
    if (base != nullptr) {
      // 0x2040
      base->SetModelFlags(mModelFlags);
    }
    // 0x2068: the player that was in the turret (0x188).
    CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
    if (player == nullptr) {
      // 0x2228: no player, so the turret is only deactivated (the vtable's slot 7 with 0).
      SetActive(false);
      break;
    }
    // 0x209C: the player is ejected from the turret (the guest's own id at 0x8).
    player->EjectFromTurret(GetUniqueId(), mgr);
    // 0x20A0: the damage is the float at + 0x14 of the player's CHealthInfo (the guest's
    // 0x1314 + 0x14) - the bits of the last damage's weapon mode, read as a float (the guest's
    // `lfs f1, 0x14(r5)`); 0x20C4: the player (0x188) is killed by the hull: the hull actor id
    // is 0x220.  It is not the player's health: that can be negative when the player dies, and
    // CStateManager::KillPlayer branches on the amount (CStateManager.cpp:3016), so the guest's
    // value (0, or a tiny positive number) is what the port has to pass.
    mgr.KillPlayer(player->GetHealthInfo()->GetLastDamageWeaponAsFloat(), mPlayerId, mHullActorId);
    // 0x20C8: the flag byte at 0x22C loses its bit 7 (0x80) - the guest's
    // `rlwimi r0, r3, 7, 24, 24` with r3 = 0.  The player is no longer in the turret.
    mPlayerInTurret = false;
    // 0x20D4/0x20EC: and the turret has no player any more.
    mPlayerId = kInvalidUniqueId;
    // 0x20F0: the base actor (0x198) and 0x2100 the top actor (0x19A) again.
    base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
    CActor* top = TCastToPtr< CActor >(mgr.ObjectById(mTopActorId));
    // 0x2128/0x212C: both rotations start at zero (lbl_63_rodata_8 = 0.0) - the guest does this
    // before it tests the two actors, so it happens even without a base actor.
    mPitch = 0.f;
    mPitchTarget = 0.f;
    // 0x2130: the base actor has to exist; 0x2138: so does the top actor.
    if (base != nullptr && top != nullptr) {
      // 0x213C: the base actor keeps its own position (its 0x54, 0x58 and 0x5C) but takes the
      // turret's rotation (the guest's transform at 0x24).
      const CVector3f position = base->GetTranslation();
      base->SetTransform(GetTransform());
      base->SetTranslation(position);
      // 0x216C: the base actor's animation data (its model data at 0x60, the anim data at 0x10).
      CAnimData* animData = base->AnimationData();
      // 0x2178: the first additive animation (0x19C) at weight 0 (lbl_63_rodata_8).
      animData->AddAdditiveAnimation(mAnimId1, 0.f, false, false);
      // 0x21A8: the second additive animation (0x1A0) at weight 0.
      animData->AddAdditiveAnimation(mAnimId2, 0.f, false, false);
      // 0x21D4: the animation data's flag byte at 0x2AC loses its bit 6 (0x40, the loop flag) -
      // the guest's `rlwimi r3, r7, 6, 25, 25` with r7 = 0.
      // 0x21E8: the same byte gains its bit 7 (0x80, the animating flag) - the guest's
      // `rlwimi r3, r6, 7, 24, 24` with r6 = 1.  The host's EnableLooping does both.
      animData->EnableLooping(false);
      // 0x21F0: the turret's own animation (0x1AC) plays: the id, -1 (0x21FC), the weight 1.0
      // (0x2200, lbl_63_rodata_10) and "animating" (0x2218).  The guest's SetAnimation gets 1
      // as its last argument (0x21DC).
      animData->SetAnimation(CAnimPlaybackParms(mUnknown1AC, -1, 1.f, true), true);
    }
    // 0x2228: the turret is deactivated (the vtable's slot 7 with 0).
    SetActive(false);
    break;
  }
  case kSM_AreaLoaded: {
    // 0x2244: fn_63_15B0 (0x3C4 bytes) - the hull's base and top actors and its five animation
    // ids. Only its two connections are ported (see LoadHullActors).
    LoadHullActors(mgr);
    break;
  }
  case kSM_Damage: {
    // No shipped code sends this message to a player turret: the turret has no health info
    // (CActor::HealthInfo() returns nullptr, 0x00003078 in the DOL), so CStateManager::ApplyDamage
    // never targets it, and the DOL's CScriptActor (the hull, the only damageable actor around)
    // does not forward damage to its attachments - its AcceptScriptMsg has no kSM_Damage case.
    // The case is retail's and is ported here; only the arm is unreachable in the game's flow
    // (a console `send <id> Damage` reaches it).
    // 0x2254: the damage flash timer (lbl_63_rodata_20 = 0.15) at 0x20C.
    mUnknown20C = 0.15f;
    // 0x225C: the game mode (the guest keeps the low byte of it at 0x2260).
    if (mgr.IsMultiplayer()) {
      // 0x2268: the player in the turret (0x188).
      CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
      if (player != nullptr) {
        // 0x2288: the player's sound pan (the guest's kMSP_Player = 4).
        // 0x22A8: the multi-player impact sound (0x226) at that pan.
        CSfxManager::SfxStart(mSfxMultiPlayerImpact, 0x7f,
                              player->GetSoundPan(CPlayer::kMSP_Player), CSfxManager::kAllAreas,
                              false, false, CSfxManager::kMedPriority);
      }
    } else {
      // 0x22C4: the single-player impact sound (0x224) as an emitter at the turret's target
      // position (0x210), in the turret's own area (the guest's 0x4 of the entity).
      CSfxManager::AddEmitter(mSfxSinglePlayerImpact, mTargetPosition, GetCurrentAreaId().Value(),
                              true, false, CSfxManager::kMedPriority);
    }
    // 0x22E8: the damaged actor - the message's sender (its +0) - gives the hull actor id.
    const CActor* damaged = TCastToConstPtr< CActor >(mgr.GetObjectById(msg.GetSenderId()));
    if (damaged != nullptr) {
      // 0x230C: the actor's health info comes from the vtable (the guest's slot at +0x3C).
      const CHealthInfo* health = damaged->GetHealthInfo();
      if (health != nullptr) {
        // 0x2338: the owner of the last damage (the guest's CHealthInfo + 0x18) at 0x220.
        mHullActorId = health->GetLastDamageOwner();
      } else {
        // 0x2344: the message's originator (its +2).
        mHullActorId = msg.GetOriginator();
      }
    }
    break;
  }
  default:
    break;
  }
  // 0x2348: every case ends in CActor's own handler.
  CActor::AcceptScriptMsg(mgr, msg);
}

// fn_63_14B4 (0x54): the turret's top actor (0x19A) becomes solid.
// @ fn_63_14B4
void CScriptPlayerTurret::AddSolidToTopActor(CStateManager& mgr) {
  // 0x14CC: the top actor's id.
  CActor* top = TCastToPtr< CActor >(mgr.ObjectById(mTopActorId));
  // 0x14E0: the top actor (0x19A) is the turret's own ATCL connection, and it resolves in the
  // shipped turrets, so the "no top actor" arm is unreachable in the shipped data; the probe
  // printed "14B4_top_actor_ok" and never "14B4_no_top_actor".
  if (top != nullptr) {
    // 0x14EC: the material is 0x3b (kMT_Solid).
    top->AddMaterial(kMT_Solid, mgr);
  }
}

// fn_63_19B0 (0x184): the animation parameters for one lookup - the state id and the eight
// parameter slots (the guest's CPASAnimParmData: the state at +0, the count at +4 and the eight
// eight-byte parameters at +8).
// @ fn_63_19B0
CPASAnimParmData CScriptPlayerTurret::BuildAnimParmData(pas::EAnimationState state,
                                                        const CPASAnimParm& p0,
                                                        const CPASAnimParm& p1,
                                                        const CPASAnimParm& p2,
                                                        const CPASAnimParm& p3,
                                                        const CPASAnimParm& p4,
                                                        const CPASAnimParm& p5,
                                                        const CPASAnimParm& p6,
                                                        const CPASAnimParm& p7) {
  // 0x19E0: the state id; 0x19F8..0x1B18: the eight parameter slots.
  return CPASAnimParmData(state, p0, p1, p2, p3, p4, p5, p6, p7);
}

// fn_63_15B0 (0x3C4): the base actor (0x198) and the top actor (0x19A) come from the turret's
// "AttachedAnimatedObject" and "AttachedCollisionObject" connections, and the base actor's
// animation database gives the five additive animation ids (0x19C..0x1AC) and the light colour
// (0x1D4..0x1DC).
//
// The five animation lookups go through fn_63_19B0 (BuildAnimParmData). The connections have to
// be real, because the DOL's turret HUD (CTurretHud::UpdateEnergy) reads the hull actor's id and
// dereferences the actor it names.
// @ fn_63_15B0
void CScriptPlayerTurret::LoadHullActors(CStateManager& mgr) {
  // 0x15E4: the base actor is the turret's "AttachedAnimatedObject" connection.
  mBaseActorId = FindConnectedObject(mgr, kSS_AttachedAnimatedObject, kSM_Attach);
  // 0x1608: the base actor itself.
  CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
  // 0x161C: the base actor's animation data; the guest reads the model pointer without a test
  // (a port limit: the host allows an actor without a model).
  // 0x1614: the base actor (0x198) is the turret's own ATOB connection and it resolves in all
  // three shipped turrets (the shooting gallery, the Agon Wastes and the Sanctuary Fortress), so
  // the "no base actor" and "no animation data" arms are unreachable in the shipped data; the
  // probe of the run in each area printed "15B0_hull_ok" and never "15B0_no_hull".
  CAnimData* animData = base != nullptr && base->GetModelData() != nullptr
                           ? base->AnimationData()
                           : nullptr;
  // Without a base actor or its animation data the guest skips the lookups and the flags copy
  // (0x16E8/0x16F8 branch to 0x1A04).
  if (animData != nullptr) {
    // 0x1634: the five animation ids (0x19C..0x1AC) come from the base actor's animation
    // database: the getup (twice), the step, the death and the locomotion animation. Only the two
    // getup lookups pass a parameter (the guest's FromEnum(0) at 0x1670 and FromEnum(1) at
    // 0x1704); the other three pass eight empty ones (the guest's eight NoParameter calls at
    // 0x175C..0x18B4).
    mAnimId1 = animData->FindBestAnimation(
        BuildAnimParmData(pas::kAS_Getup, CPASAnimParm::FromEnum(0)));
    mAnimId2 = animData->FindBestAnimation(
        BuildAnimParmData(pas::kAS_Getup, CPASAnimParm::FromEnum(1)));
    mUnknown1A4 = animData->FindBestAnimation(BuildAnimParmData(pas::kAS_Step));
    mUnknown1A8 = animData->FindBestAnimation(BuildAnimParmData(pas::kAS_Death));
    mUnknown1AC = animData->FindBestAnimation(BuildAnimParmData(pas::kAS_Locomotion));
    // 0x1918: the base actor's model flags (the guest's 0xFC..0x104) become the turret's saved
    // copy (0x1D4..0x1DC), which the stop message hands back.
    mModelFlags = base->GetModelFlags();
  }
  // 0x1930: the top actor is the turret's "AttachedCollisionObject" connection.
  mTopActorId = FindConnectedObject(mgr, kSS_AttachedCollisionObject, kSM_Attach);
}

// vtable slot 9 = fn_63_2D68 (0x2CC): the base actor's light colour.
//
// The guest's model flags are 12 bytes: a word, the blend mode byte, the material set byte, the
// flags half and the packed colour; the host's CModelFlags holds the same five fields.
// @ fn_63_2D68
void CScriptPlayerTurret::PreRender(CStateManager& mgr) {
  // 0x2D88: the base actor (0x198).
  CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
  if (base == nullptr) {
    return;
  }
  // 0x2DAC: the copy the guest works on is the turret's OWN saved flags (the guest's 0x1D4..0x1DC,
  // the copy fn_63_15B0 took from the base actor at the area load), not the base actor's flags as
  // they are now.
  CModelFlags flags = mModelFlags;
  // 0x2DB4: the player in the turret (0x188).  The guest has two flash arms: 0x2E9C (reached
  // once the id is valid and the index is the rendered player's, whatever the player's state is)
  // and 0x2EFC (reached when either test fails).
  bool renderedPlayer = false;
  if (mPlayerId != kInvalidUniqueId) {
    // 0x2DE4: the player; 0x2DF4: the player's index has to be the player being rendered - the
    // guest's mgr + 0x24DC is CStateManager::mCurrentRenderPlayerIndex (CFrustumPlanes is 0x64
    // bytes at 0x2478, so the index lands on 0x24DC) - and 0x2E24: its turret state (the guest's
    // player + 0x12F8) must not be 2 (kTS_Exiting).  The null test is the guest's own (0x2E1C:
    // the TCastToPtr<CPlayer> result is tested before the turret state is read).
    renderedPlayer = mgr.GetCurrentRenderPlayerIndex() ==
                     static_cast< int >(mgr.MaskUIdNumPlayers(mPlayerId));
    const CPlayer* player = TCastToConstPtr< CPlayer >(mgr.GetObjectById(mPlayerId));
    if (renderedPlayer && player != nullptr && player->GetTurretState() != CPlayer::kTS_Exiting) {
      // 0x2E30: white with the alpha 0.25 (lbl_63_rodata_54) and the blend mode 5 (kT_Blend).
      flags.SetTrans(CModelFlags::kT_Blend);
      flags.SetMatSetIdx(0);
      // 0x2E6C..0x2E94: the flags half (the guest's 0x102) is (3 & ~3) | 1 - the depth compare
      // (0x1) only, the depth update (0x2) is off (`clrrwi` 0x2E74 clears the low two bits,
      // `ori` 0x2E7C sets bit 0).
      flags.SetFlags(static_cast< CModelFlags::EFlags >(1));
      flags.SetColor(CColor(1.f, 1.f, 1.f, 0.25f));
    }
  }
  // 0x2EA8 / 0x2F0C: while the damage flash timer (0x20C) runs the colour changes.  This
  // arm needs a kSM_Damage message, and nothing in the shipped game sends one to a turret (the
  // turret has no health info, so the damage path never targets it, and the DOL's CScriptActor
  // does not forward damage to its attachments) - see the kSM_Damage case of AcceptScriptMsg.
  if (mUnknown20C > 0.f) {
    if (renderedPlayer) {
      // 0x2EB0..0x2EF4: the rendered player's turret flashes the fixed colour (1, 0, 0, 0.25)
      // (f1 = rodata_10, f2 = f3 = rodata_8, f4 = rodata_54) with the blend mode 5, the flags
      // half 3, and no lerp.
      flags.SetTrans(CModelFlags::kT_Blend);
      flags.SetMatSetIdx(0);
      flags.SetFlags(static_cast< CModelFlags::EFlags >(3));
      flags.SetColor(CColor(1.f, 0.f, 0.f, 0.25f));
    } else {
      // 0x2F20: the factor is the timer over 0.33 (lbl_63_rodata_58), clamped to 0..1.
      float factor = mUnknown20C / 0.33f;
      if (factor < 0.f) {
        factor = 0.f;
      } else if (factor > 1.f) {
        factor = 1.f;
      }
      // 0x2F48: Lerp(black, red, factor) with the blend mode 2 (kT_Two). The red is the
      // module's own colour lbl_63_bss_1C, which the module's initializer fn_63_376C builds as
      // CColor(0.5, 0, 0, 1) (0x000037A4) - not CColor::Red(), which is twice as bright.
      flags.SetTrans(CModelFlags::kT_Two);
      flags.SetMatSetIdx(0);
      flags.SetFlags(static_cast< CModelFlags::EFlags >(3));
      flags.SetColor(CColor::Lerp(CColor::Black(), kTurretFlashColor, factor));
    }
  }
  // 0x2F8C: the flags go back into the base actor.
  base->SetModelFlags(flags);
  // 0x2FB8: the base actor is a CScriptActor (the guest's TCastToPtr<CScriptActor>); 0x2FC4..
  // 0x30008: the new flags are compared with the turret's saved ones (0x1D8, 0x1D9, 0x1DA and
  // 0x1DC - the host's CModelFlags::operator== is the same comparison), and 0x300C-0x30014: the
  // base actor's flag byte at +0x396 gets its bit 1 (0x02, the guest's last bit field,
  // mRenderImmediately) set when they are the same and cleared otherwise
  // (`rlwimi r0, r5, 1, 30, 30` with r5 = 1 from 0x30008).
  CScriptActor* scriptBase = TCastToPtr< CScriptActor >(base);
  if (scriptBase != nullptr) {
    scriptBase->SetRenderImmediately(flags == mModelFlags);
  }
}

// vtable slot 18 = fn_63_3034 (0x3C): the touch bounds are the turret's own box.
// @ fn_63_3034
rstl::optional_object< CAABox > CScriptPlayerTurret::GetTouchBounds() const {
  return rstl::optional_object< CAABox >(mBounds);
}

// fn_63_1070 (0xC4). The camera looks from the turret's position towards the turret's position
// plus its forward vector flattened onto the ground plane (the z term is rodata_8 = 0.0).
// @ fn_63_1070
CTransform4f CScriptPlayerTurret::GetCameraTransform(CStateManager& mgr) {
  const CTransform4f turret = GetTurretTransform(mgr);
  const CVector3f position = turret.GetTranslation();
  const CVector2f forward(turret.GetForward().GetX(), turret.GetForward().GetY());
  return CTransform4f::LookAt(position,
                              CVector3f(position.GetX() + forward.GetX(),
                                        position.GetY() + forward.GetY(), position.GetZ() + 0.f),
                              CVector3f::Up());
}

// fn_63_F6C (0xB4). The turret sits on the base actor's "upwardballsnap_LCTR" locator
// (lbl_63_rodata_60 + 7); without a base actor the turret's own transform is used.
// @ fn_63_F6C
CTransform4f CScriptPlayerTurret::GetTurretTransform(CStateManager& mgr) {
  const CActor* base = TCastToConstPtr< CActor >(mgr.ObjectById(mBaseActorId));
  if (base == nullptr) {
    return GetTransform();
  }
  const CModelData* model = base->GetModelData();
  if (model == nullptr) {
    // Port limit: the guest reads the base actor's model without a test (0x00000FD0); on the
    // host the base actor can be a script actor without a model.
    return GetTransform();
  }
  const CTransform4f locator = model->GetLocatorTransform("upwardballsnap_LCTR");
  return base->GetTransform() * locator;
}

// fn_63_1B70 (0x78). The player is let out of the turret and the hull loses its solid material.
// @ fn_63_1B70
void CScriptPlayerTurret::ExitTurret(CStateManager& mgr) {
  mPlayerInTurret = false; // guest 0x22C bit 0x80, cleared at 0x00001BA0
  SendScriptMsgs(kSS_Exited, mgr, kInvalidUniqueId, kSM_None);
  // 0x1BCC: fn_63_1460 - the top actor (0x19A) loses kMT_Solid.
  RemoveSolidFromTopActor(mgr);
}

// fn_63_1460 (0x54): the turret's top actor (0x19A) loses kMT_Solid.
// @ fn_63_1460
void CScriptPlayerTurret::RemoveSolidFromTopActor(CStateManager& mgr) {
  // 0x1478: the top actor's id.
  CActor* top = TCastToPtr< CActor >(mgr.ObjectById(mTopActorId));
  // 0x148C: the top actor (0x19A) is the turret's own ATCL connection, and it resolves in the
  // shipped turrets, so the "no top actor" arm is unreachable in the shipped data; the probe
  // printed "1460_top_actor_ok" and never "1460_no_top_actor".
  if (top != nullptr) {
    // 0x1498: the material is 0x3b (kMT_Solid).
    top->RemoveMaterial(kMT_Solid, mgr);
  }
}

// fn_63_1134 (0x32C): the player's aim while in the turret.
// @ fn_63_1134
void CScriptPlayerTurret::ProcessInput(const CFinalInput& input, CStateManager& mgr) {
  // 0x1154: the player in the turret (0x188) and the base actor (0x198).
  CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
  CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
  if (player == nullptr || base == nullptr) {
    return;
  }
  // 0x1198: the player's turret state (the guest's player + 0x12F8) has to be 3 (kTS_Active).
  const CControlMapper& mapper = player->GetControlMapper();
  if (player->GetTurretState() == CPlayer::kTS_Active) {
    // 0x11AC: the reload timer (0x1B8) counts down.
    if (mUnknown1B8 > 0.f) {
      // 0x11BC: the guest's input + 0 is the frame time.
      mUnknown1B8 -= input.DeltaTime();
      // 0x11C4: the base actor's animation data; the guest reads the model pointer without a
      // test (a port limit: the host allows an actor without a model).
      CAnimData* animData =
          base->GetModelData() != nullptr ? base->AnimationData() : nullptr;
      if (animData != nullptr) {
        // 0x11D4: the time left of the "Whole Body" animation (lbl_63_rodata_60 + 0x1B).
        if (animData->GetAnimTimeRemaining("Whole Body") <= 0.f) {
          // 0x1220: the animation data's flag byte at 0x2AC gains bit 0x40 (the loop flag) and
          // bit 0x80 (the animating flag) - the host's EnableLooping does both.
          animData->EnableLooping(true);
          // 0x125C: the turret's animation (0x1A8) plays: the id, -1, the weight 1.0 and
          // "animating".
          animData->SetAnimation(CAnimPlaybackParms(mUnknown1A8, -1, 1.f, true), false);
        }
      }
    }
    // 0x1294: the beam is fired.
    if (player->FireBeamPressed(input)) {
      // 0x12A8: only when the reload timer is over.
      if (mUnknown1B8 <= 0.f) { // 0x12B4-0x12BC: fcmpo + cror eq,lt,eq is <=
        // 0x12C0: fn_63_84C fires the turret's shot.
        FireTurretShot(mgr);
        // 0x12CC: the reload timer is the fire rate (0x194).
        mUnknown1B8 = mFireRate;
      }
    }
    // 0x12D4: the turn axis: the guest's commands 4 and 3 (turn right and turn left).
    float yaw = mapper.GetAnalogInput(CControlMapper::kC_TurnLeft, input,
                                      CControlMapper::kFT_Filtered) -
                mapper.GetAnalogInput(CControlMapper::kC_TurnRight, input,
                                      CControlMapper::kFT_Filtered);
    // 0x1300: the difference is clamped to -1..1.
    yaw = ClampAxis(yaw);
    // 0x1334: the yaw the input asks for.
    mYawTarget = yaw;
  }
  // 0x1338: the pitch axis: the guest's commands 1 and 2 (forward and backward).
  float pitch = mapper.GetAnalogInput(CControlMapper::kC_Backward, input,
                                      CControlMapper::kFT_Filtered) -
                mapper.GetAnalogInput(CControlMapper::kC_Forward, input,
                                      CControlMapper::kFT_Filtered);
  // 0x1364: the difference is clamped to -1..1.
  pitch = ClampAxis(pitch);
  // 0x1398: with the Y axis inverted (the guest's game state byte 0xA4, bit 0x40) the two
  // commands swap.
  if (gpGameState->GameOptions().GetInvertYAxis()) {
    // 0x13B4
    pitch = mapper.GetAnalogInput(CControlMapper::kC_Forward, input,
                                  CControlMapper::kFT_Filtered) -
            mapper.GetAnalogInput(CControlMapper::kC_Backward, input,
                                  CControlMapper::kFT_Filtered);
    // 0x13F0
    pitch = ClampAxis(pitch);
  }
  // 0x1418: the pitch is scaled by the elevation limits: down for a negative input, up for a
  // positive one.
  if (pitch < 0.f) {
    // 0x1428
    mPitchTarget = pitch * mMaxVertElevationUp;
  } else {
    // 0x1438
    mPitchTarget = pitch * mMaxVertElevationDown;
  }
}

// fn_63_1508 (0x68): the muzzle transform - the base actor's (0x198) scaled locator in the
// base actor's own frame.
// @ fn_63_1508
CTransform4f CScriptPlayerTurret::GetMuzzleTransform(CStateManager& mgr) {
  // 0x1534: the turret's own transform.
  CTransform4f turret = GetTransform();
  // 0x1538: the base actor (0x198).
  CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
  // 0x1554: the base actor (0x198) is the turret's own ATOB connection and it resolves in all
  // three shipped turrets, so the "no base actor" arm is unreachable in the shipped data; the
  // probe printed "1508_base_actor_ok" and never "1508_no_base_actor".
  if (base == nullptr) {
    return turret;
  }
  // 0x1558: the locator is lbl_63_bss_C, the static string the module's initializer fn_63_376C
  // builds from lbl_63_rodata_60 + 0x26 (0x00003788): "muzzleend_LCTR".
  const CTransform4f locator = base->GetModelData()->GetScaledLocatorTransform(kMuzzleLocator);
  // 0x1578: the locator in the base actor's frame, times the base actor's transform.
  return base->GetTransform() * locator;
}

// fn_63_84C (0x184): the turret's shot.
// @ fn_63_84C
void CScriptPlayerTurret::FireTurretShot(CStateManager& mgr) {
  // 0x864: the muzzle transform - the guest calls fn_63_1508 (0x870) and copies the result into
  // its own frame (0x87C, the copy constructor).
  const CTransform4f xf = GetMuzzleTransform(mgr);
  // 0x880: the player in the turret - the guest's own id at 0x188 (0x88C).
  CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerId));
  // 0x89C: the player is always there - the shot is only fired from ProcessInput, which returns
  // early when the player is missing (fn_63_1134's own test at 0x1168).  The arm is unreachable
  // in the shipped data; a once-per-arm probe printed "84C_player_ok" and never
  // "84C_player_null".
  if (player == nullptr) {
    return;
  }
  // 0x8A0: the turret's own token (0x1E8) is the weapon description and has to be loaded - the
  // guest reads the token's flag word at +0x18 and compares it with 0 (0x8A8).  The token is
  // loaded by the kSM_Create case at the area load (fn_63_1BE8, 0x1C9C), before any shot, so the
  // "not loaded" arm is unreachable in the shipped data; the probe printed "84C_token_loaded"
  // and never "84C_token_not_loaded".
  if (mToken.null() || !mToken->IsLoaded()) {
    return;
  }
  // 0x8B0: the projectile's allocation - the guest's `operator new` of 0x568 bytes (0x8C0).
  // 0x958: the token is the turret's own (the guest's &0x1E8).
  // 0x970: the projectile is active (the guest's 1).
  // 0x98C: the projectile's type is the damage's weapon mode (the guest's 0x1F0).
  // 0x978: its exclude material is 0x14 (kMT_ProjectilePassthrough).
  // 0x908: the projectile's own unique id (the guest's AllocateUniqueId).
  // 0x8FC: the area is the player's own area (the guest's 0x4 of the player).
  // 0x8F4: the owner is the player in the turret (the guest's 0x188).
  // 0x8E8: the homing target is kInvalidUniqueId.
  // 0x940: the attributes are 0.
  // 0x968: the projectile is not underwater.
  // 0x974: the scale is the one vector (the guest's sOneVector).
  // 0x97C: the impact visor effect is the empty one (the guest's fn_63_B18 at 0x8D4).
  // 0x980: the "unused" flag is 0.
  // 0x984: the impact sound is played (the guest's 1).
  // 0x988: the impact is not oriented to the owner (the guest's 0).
  // 0x934: the charge factor (f1) is lbl_63_rodata_10 = 1.0.
  // 0x924: the close impact distance (f2) is lbl_63_rodata_14 = 4.0.
  // 0x948: the impact scale distance (f3) is a copy of f2 (the guest's `fmr f3, f2`) = 4.0.
  const TToken< CWeaponDescription > description(*mToken);
  CEnergyProjectile* projectile = rs_new CEnergyProjectile(
      true, description, static_cast< EWeaponType >(mWeaponDamage.GetWeaponMode1()), xf,
      kMT_ProjectilePassthrough, mWeaponDamage, mgr.AllocateUniqueId(),
      player->GetCurrentAreaId(), mPlayerId, kInvalidUniqueId, 0, false, CVector3f::One(),
      CImpactVisorEffect::None(), false, true, false, 1.f, 4.f, 4.f);
  // 0x8CC: the allocation returned null - the guest jumps to 0x998, where the null pointer ends
  // the function at 0x9B4 (before the sound).
  if (projectile == nullptr) {
    return;
  }
  // 0x9B8: the top actor (0x19A) is the one the projectile does not hit (the guest's call with
  // lbl_63_rodata_18 = 3.4028235e38f).
  projectile->AddCollisionCooldown(mTopActorId, 3.4028235e38f);
  // 0x9E0: the projectile joins the world.
  mgr.AddObject(projectile);
  // 0x9E4: the base actor (0x198) plays the shot animation (0x1A4).
  CActor* base = TCastToPtr< CActor >(mgr.ObjectById(mBaseActorId));
  // 0xA00: the base actor (0x198) is the turret's own ATOB connection, resolved at the area load
  // (fn_63_15B0); it resolves in all three shipped turrets, so the "no base actor" arm is
  // unreachable in the shipped data (the probe printed "84C_base_and_anim_ok" and never
  // "84C_base_or_anim_null").
  if (base != nullptr && base->GetModelData() != nullptr &&
      base->GetModelData()->GetAnimationData() != nullptr) {
    CAnimData* animData = base->AnimationData();
    // 0xA28: the flag byte at 0x2AC loses its bit 6 (0x40, the loop flag) - the guest's
    // `rlwimi r0, r8, 6, 25, 25` with r8 = 0.
    // 0xA44: the same byte gains its bit 7 (0x80, the animating flag) - the guest's
    // `rlwimi r6, r7, 7, 24, 24` with r7 = 1.  The host's EnableLooping(false) does both.
    animData->EnableLooping(false);
    // 0xA4C: the animation id (0x1A4), -1 (0xA54), the weight 1.0 (0xA58, lbl_63_rodata_10) and
    // "animating" (0xA74); the guest's SetAnimation gets 0 as its last argument (0xA3C).
    animData->SetAnimation(CAnimPlaybackParms(mUnknown1A4, -1, 1.f, true), false);
  }
  // 0xA84: the shot's sound.  The guest tests the game mode (0xA88) and keeps the low byte of
  // the result (0xA8C, `clrlwi. r0, r3, 24`), so the test is a plain bool.
  // The multiplayer arm is live in retail (Echoes' multiplayer is local split-screen for 2-4
  // players on one console and it ships on the disc - the M0x_* worlds, M06_ShootingGallery among
  // them, are multiplayer arenas), but the port cannot start a match: there is no console command
  // and no environment variable for it, the front end's VS flow is what creates one
  // (CScriptGuiScreen's deathmatch/coin switches, CGameState's CGMDeathMatch/CGMCoin) and the
  // harness skips the front end and sets the single-player mode itself (PortSkipFrontEnd.cpp:313),
  // driving pad 0 only.  MP2_TURRET_MP=1 forces the arm so a run can cover it.
  if (mgr.IsMultiplayer()) {
    // 0xA94: the multi-player projectile sound (0x22A) at the player's sound pan (0xA9C, the
    // guest's kMSP_Player = 4).
    CSfxManager::SfxStart(mSfxMultiPlayerProjectile, 0x7f,
                          player->GetSoundPan(CPlayer::kMSP_Player), CSfxManager::kAllAreas,
                          false, false, CSfxManager::kMedPriority);
  } else {
    // 0xAEC: the single-player projectile sound (0x228) at the fixed pan 0x40 (0xAEC).
    CSfxManager::SfxStart(mSfxSinglePlayerProjectile, 0x7f, 0x40, CSfxManager::kAllAreas, false,
                          false, CSfxManager::kMedPriority);
  }
}

// fn_63_840 (0xC): the actor the turret HUD reads the hull's health from.
// @ fn_63_840
TUniqueId CScriptPlayerTurret::GetHullActorId() {
  return mTopActorId;
}

// fn_63_144 (0x4CC). The property loop is the generated one (SLdrPlayerTurret.inc); the loader
// turns the seven angles and speeds from degrees into radians (lbl_63_rodata_0 = 0.017453292)
// and leaves the fire rate alone.
// @ fn_63_144
static CEntity* PortLoadScriptPlayerTurret(CStateManager& mgr, CInputStream& input,
                                            CEntityInfo& info) {
  SLdrPlayerTurret sldrThis;
#include "MetroidPrime/ScriptLoader/SLdrPlayerTurret.inc"

  const float kDegToRad = 0.017453292f;

  return rs_new CScriptPlayerTurret(
      mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
      LdrToEntityInfo(info, sldrThis.editorProperties),
      LdrToTransform4f(sldrThis.editorProperties), CActorParameters(), sldrThis.flagsPlayerTurret,
      kDegToRad * sldrThis.maxHorizRotationLeft, kDegToRad * sldrThis.maxHorizRotationRight,
      kDegToRad * sldrThis.maxVertElevationUp, kDegToRad * sldrThis.maxVertElevationDown,
      kDegToRad * sldrThis.damageAngle, kDegToRad * sldrThis.horizSpeed,
      kDegToRad * sldrThis.vertSpeed, sldrThis.fireRate,
      LdrToDamageInfo(sldrThis.weaponDamage), sldrThis.weaponEffect,
      sldrThis.weaponEffectMultiPlayer, TranslateSfx(sldrThis.sFXTurretRotation),
      TranslateSfx(sldrThis.sFXSinglePlayerImpact), TranslateSfx(sldrThis.sFXMultiPlayerImpact),
      TranslateSfx(sldrThis.sFXSinglePlayerProjectile),
      TranslateSfx(sldrThis.sFXMultiPlayerProjectile));
}

static SPlayerTurret_FuncPtrs REL_loader_PlayerTurret;

// Retail's RELMain calls fn_63_70, which stores the loader fn_63_144 at lbl_63_bss_20 +0x0 and
// copies the five pointers-to-member lbl_63_data_4 = {0, -1, fn_63_1070},
// lbl_63_data_10 = {0, -1, fn_63_F6C}, lbl_63_data_1C = {0, -1, fn_63_1B70},
// lbl_63_data_28 = {0, -1, fn_63_1134} and lbl_63_data_34 = {0, -1, fn_63_840} to +0x4 .. +0x3C,
// then calls the DOL's SetSPlayerTurret_FuncPtrs. The DOL's proxies in ScriptLoaderRel.cpp call
// through it, so the turret camera, the exit path, the input path and the turret HUD work.
// RELMain (0x20) is the linker's entry: it calls fn_63_70 and returns.  The port's body is
// fn_63_70's (the linker calls the port's RELMain, which is where the module's setup lives):
// the guest stores the loader at lbl_63_bss_20 + 0 (0xB4), copies the five pointers-to-member
// tables (lbl_63_data_4, _10, _1C, _28, _34) to lbl_63_bss_20 + 4 .. + 0x3C (0xF0..0x12C) and calls
// the DOL's SetLoader_PlayerTurret (0x12C); the port fills the same table with the loader and the
// five member functions and hands it to the DOL's SetSPlayerTurret_FuncPtrs.
// @ fn_63_70
void RELMain() {
  REL_loader_PlayerTurret.mLoadPlayerTurret = PortLoadScriptPlayerTurret;
  REL_loader_PlayerTurret.mGetCameraTransform =
      static_cast< CTransform4f (CEntity::*)(CStateManager&) >(
          &CScriptPlayerTurret::GetCameraTransform);
  REL_loader_PlayerTurret.mGetTurretTransform =
      static_cast< CTransform4f (CEntity::*)(CStateManager&) >(
          &CScriptPlayerTurret::GetTurretTransform);
  REL_loader_PlayerTurret.mExitTurret =
      static_cast< void (CEntity::*)(CStateManager&) >(&CScriptPlayerTurret::ExitTurret);
  REL_loader_PlayerTurret.mProcessInput =
      static_cast< void (CEntity::*)(const CFinalInput&, CStateManager&) >(
          &CScriptPlayerTurret::ProcessInput);
  REL_loader_PlayerTurret.mGetHullActorId =
      static_cast< TUniqueId (CEntity::*)() >(&CScriptPlayerTurret::GetHullActorId);
  SetSPlayerTurret_FuncPtrs(&REL_loader_PlayerTurret);
}

// RELExit (0x24): the linker's exit - the DOL's SetLoader_PlayerTurret with 0, which the port's
// SetSPlayerTurret_FuncPtrs(nullptr) is (the DOL's proxy drops the table).
void RELExit() { SetSPlayerTurret_FuncPtrs(nullptr); }
