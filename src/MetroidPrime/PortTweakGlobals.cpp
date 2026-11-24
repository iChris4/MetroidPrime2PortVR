// The host stand-in for Tweaks.rel's `REL_CreateTweakGlobals`, and the only thing
// that makes `CGameArchitectureSupport`'s constructor survivable on a PC.
//
// **Not a configure.py unit**, for the reason every file in this group gives: a
// definition inside a claimed unit shifts that unit's small-data offsets and
// re-optimises unrelated functions, which `tools/gate.sh` reports as a
// regression (measured: adding one `static` to `MetroidPrime/main.cpp` moved
// `__ct__CGameArchitectureSupport` 84.51% -> 81.54%). Nothing here can reach
// `main.dol` or any of the 86 REL modules.
//
// ---------------------------------------------------------------------------
// Why this file exists: the first null dereference, measured
// ---------------------------------------------------------------------------
//
// `CGameArchitectureSupport::CGameArchitectureSupport` is DOL .text 0x80007EC4 and
// is 0x3F8 = 1016 bytes (0x80007EC4..0x800082BC). Twelve instructions in, it
// loads `gpTweakPlayerA` and calls through it with **no null test at all**:
//
//   80007f38:  83 ad 91 c4   lwz   r29,-28220(r13)   ; 0x8041FD80-0x6E3C = 0x80418F44
//   80007f3c:  7f a3 eb 78   mr    r3,r29
//   80007f40:  48 21 05 8d   bl    802184cc <GetRightAnalogMax__12CTweakPlayerFv>
//   80007f44:  ff e0 08 90   fmr   f31,f1
//   80007f48:  7f a3 eb 78   mr    r3,r29
//   80007f4c:  48 21 05 8d   bl    802184d8 <GetLeftAnalogMax__12CTweakPlayerFv>
//
// and the two floats it reads go into the `CInputGenerator` constructor's
// arguments on the very next call (0x80007F58, `addi r3,r31,48`). So the
// constructor cannot even begin without a usable `gpTweakPlayerA`.
//
// 0x80418F44 is `gpTweakPlayerA`. Measured, it has exactly one writer outside a
// destructor registration:
//
//   * `fn_800324A4` (.text 0x800324A4..0x80032670, first word of `.ctors` entry 4)
//     stores **0** into it at 0x80032588 and registers a destructor with
//     `__register_global_object` (0x80344E20). That is retail *announcing* the
//     slot, not filling it.
//   * Tweaks.rel's `REL_CreateTweakGlobals` (module .text 0x508, 0x5AC = 1452
//     bytes) stores into it at module .text **0x78C**. That is the only store
//     anywhere, and the store-by-store map is `docs/research/tweak_globals.md`.
//
// ---------------------------------------------------------------------------
// What retail's `CreateGlobals` writes, and why this file is not that function
// ---------------------------------------------------------------------------
//
// Retail allocates a **4-byte** object and stores one pointer in it:
//
//   Tweaks .text 0x78C:  gpTweakPlayerA = new[4];  cell[0] = &gpTweakContents->TweakPlayer
//
// `TweakPlayer` is at retail offset **+0x10E8** in `CTweakContents` (0x31F4
// bytes). The cell is the receiver: each accessor is three instructions that
// dereference word 0 and read a float at a **retail** offset in
// `SLdrTweakPlayer` -
//
//   802184cc <GetRightAnalogMax__12CTweakPlayerFv>:  lwz r3,0(r3); lfs f1,428(r3); blr
//                                                        428 = 0x1AC = misc.rightAnalogMax
//   802184d8 <GetLeftAnalogMax__12CTweakPlayerFv>:   lwz r3,0(r3); lfs f1,424(r3); blr
//                                                        424 = 0x1A8 = misc.leftAnalogMax
//
// and the other three (`GetVariaSuitDamageReduction` +0x370, `GetDarkSuit…`
// +0x374, `GetLightSuit…` +0x378) are the same shape. All five bodies exist in
// this tree, in `src/MetroidPrime/Tweaks/CTweakPlayer.cpp` (upstream's unit since
// the 2026-09-30 sync).
//
// **What is *not* reproducible here is the pointer.** `&gpTweakContents->TweakPlayer`
// needs a `CTweakContents` that `REL_LoadTweaks` (.text 0xCD4, 0x218 bytes) filled
// from `MetroidPrime/Common/Str("Standard.NTWK")` read out of a pak - and the paks
// need `CGameGlobalObjects::AddPaksAndFactories` (boot-path step 13, 1936 bytes of
// retail with no body in this tree). `REL_CreateTweakGlobals` itself dereferences
// `gpTweakContents` with no null test, so it cannot run before `Loader` either.
//
// So the choice is between a port that null-derefs and a port that says so. This
// file takes the second option and then goes one step further than that: it gives
// `gpTweakPlayerA` and `gpTweakPlayerB` real cells over a **zeroed**
// `SLdrTweakPlayer`, which makes all five accessors answer 0.0f. That is a legal
// value for both (`CInputGenerator` takes two floats and uses them as analog
// dead-zones/scales), it is what a tweaks file that set nothing would give, and
// it is honest about what it is: a stand-in with no tweak data behind it, named
// as one, that disappears the moment `REL_LoadTweaks` can run.
//
// It is deliberately **not** wired into `REL_CreateTweakGlobals`. That function
// has a body in `src/MetroidPrime/Tweaks/Tweaks.cpp` at 68.29%, and it is
// unreachable on the host for the reason above; the honest fix is a caller for
// it, not a replacement. `platform/main.cpp` calls
// `port::tweaks::CreateStandInTweakPlayers()` immediately after
// `port::modules::InitAll()`, which is where retail's module load has happened
// and its func-pointer table exists (`STweaks_FuncPtrs::CreateGlobals`,
// `include/MetroidPrime/ScriptLoaderRel.hpp`), unused.
//
// ---------------------------------------------------------------------------
// The allocation, and why it is raw
// ---------------------------------------------------------------------------
//
// `SLdrTweakPlayer` is generated (`scripts/generate_script_loaders.py`) with a
// declared-but-undefined constructor, a declared-but-undefined destructor and an
// `rstl::string` member, so `new SLdrTweakPlayer` does not link on the host. The
// block is therefore raw and zeroed, which is also the only value that can be
// claimed for it: retail's constructor (`fn_801449C8`'s neighbour in the Tweaks
// module) initialises the members, and none of those bodies exists here.
//
// The size is `sizeof(SLdrTweakPlayer)` **as this host compiles it**, not retail's
// 0x37C. That is correct and deliberate: the accessors reach the floats through
// the *header's* member names, so header and accessor agree by construction, and
// no retail offset is written here. (`docs/research/tweak_globals.md` records the
// 64-bit-probe trap that produced 0x388 and the wrong offsets; the rule is never
// measure a layout with the host compiler - but this allocation needs no measured
// layout at all, only the compiler's own.)

#include "MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "rstl/rmemory_allocator.hpp"

#include <cstring>

namespace port::tweaks {

namespace {

// Zeroed storage for one `SLdrTweakPlayer`. `SLdrTweakPlayer` has a declared
// constructor with no body, so this cannot be a `new`; the object is only ever
// reached through `CTweakPlayer::mData` and read by the five accessors, all of
// which are plain float loads out of it.
//
// `rstl::rmemory_allocator::allocate` rather than `calloc`, deliberately: it is
// what retail's own `operator new` reaches, so the block comes out of the game's
// heap like every other object, and it keeps the port's libc/libm dependency count
// where it was (measured: `std::calloc` here moved `tools/link_gap.py`'s
// libc/libm bucket 29 -> 30 for no gain). The zeroing is `memset` because there is
// no constructor to do it.
//
// **There is no null check on the result and that is deliberate.** `rs_new`
// *throws* `std::bad_alloc`; `rstl::rmemory_allocator::allocate` returns null only
// for `size == 0`, which `sizeof(SLdrTweakPlayer)` is not. A
// `if (block == nullptr) return nullptr;` here would be a branch that cannot be
// taken, and the caller in `platform/main.cpp` had a matching dead error path that
// this lane deleted for the same reason. Out of memory is retail's behaviour too:
// its `operator new` in `Kyoto/Alloc/CMemory.hpp:33` throws with a `CCallStack`.
SLdrTweakPlayer* ZeroedTweakPlayer() {
  void* const block = rstl::rmemory_allocator::allocate(static_cast< int >(sizeof(SLdrTweakPlayer)));
  std::memset(block, 0, sizeof(SLdrTweakPlayer));
  return static_cast< SLdrTweakPlayer* >(block);
}

// Retail's row: `slot = new[4]; cell[0] = tweak`. `CTweakPlayer` is exactly those
// four bytes (see its header), so `new` is retail's own `__nw__FUlPCcPCc` and the
// word is the only member.
CTweakPlayer* MakePlayerCell(const SLdrTweakPlayer& tweak) { return new CTweakPlayer(tweak); }

} // namespace

// See the header. Idempotent: a second call with both slots already set does
// nothing.
void CreateStandInTweakPlayers() {
  if (!gpTweakPlayerA.null() && !gpTweakPlayerB.null()) {
    return;
  }

  SLdrTweakPlayer* const player = ZeroedTweakPlayer();

  // Retail gives each slot its own 4-byte cell and both point at the same
  // `CTweakContents` block; two cells over one zeroed `SLdrTweakPlayer` is the
  // same sharing, and it is what makes the second player's five accessors work
  // without a second copy of 0x37C bytes.
  gpTweakPlayerA = MakePlayerCell(*player);
  gpTweakPlayerB = MakePlayerCell(*player);
}

} // namespace port::tweaks
