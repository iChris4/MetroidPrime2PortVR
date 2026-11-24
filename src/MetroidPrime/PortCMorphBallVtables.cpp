// Host definitions of the three `.data` vtable objects `src/MetroidPrime/Player/CMorphBall.cpp`
// references from the deleting destructors it now writes out, `fn_800C88C0` (retail 0x800C88C0)
// and `fn_800C33DC` (retail 0x800C33DC).
//
// **Not a configure.py unit, and the two must never be compiled together.** All three are
// *unclaimed* `.data` gaps - `config/G2ME01/splits.txt` ends the nearest units at 0x803B36F0
// and 0x803B1760 - so in the DOL build `dtk` fills them with retail's own bytes and they
// resolve with no declaration anywhere. The host build has no `dtk` step and nothing else
// defines them, so without this file the two destructors add `lbl_803B1750` to
// `tools/link_gap.py`'s MISSING set and `tools/gate.sh` fails on `link-gap` ("gap grew:
// lbl_803B1750 is not in port_link_gap_list.md" - measured). This is the `Port*.cpp`
// arrangement the repo uses for exactly this case: listed in `files.cmake`'s port-only list,
// not in `configure.py`.
//
// The three objects, from `config/G2ME01/symbols.txt:17971` and `:18099-18000`, measured with
// `python3 tools/dol_read.py`:
//
//   0x803B36F0  size 0xC  `0, 0, 0x800C88C0`  the derived vtable of the class `fn_800C88C0`
//   0x803B36FC  size 0xC  `0, 0, 0x800C33DC`  the derived vtable of the class `fn_800C33DC`
//   0x803B1750  size 0x10 `0, 0, 0x8000DF48`  the shared base vtable both store next
//
// Each is the MWCC layout `{0, 0, slot0}` - the two leading words are retail's offset-to-top
// and type-info slots - with slot 0 the class's own destructor, which is why the two derived
// objects hold their own `fn_` address in the third word and the base one holds `__dt__19CPlayerBodyStateCmdFv`
// (`./tools/dis.sh 0x8000DF48 0x48`, the same store-the-vtable-then-`CMemory::Free`-on-the-flag
// shape).
//
// **They are left zero-filled rather than transcribed**, deliberately. Their contents are DOL
// code addresses: copying `0x800C88C0` into a host object would build a vtable whose slot 0
// points at unmapped host memory, which is worse than an empty one. Nothing in the port calls
// through them - the only references are the two `stw`s in the destructors themselves, and the
// objects those destructors run on are the `CMorphBall` stack temporaries of `CollidedWith` and
// `ComputeScrewAttackMovement`, which are still scaffolds. The sizes are retail's, so the
// `.bss` the host build produces is the shape retail's `.data` has.
//
// The zero initialiser is load-bearing and was measured: **g++ 15 emits nothing at all for an
// unreferenced `extern "C" char name[N];` with no initialiser** - no symbol, a zero-sized
// `.bss` - so the file compiled to an object with no `lbl_*` symbol and `link-gap` still
// failed. `= {0}` makes all three appear as `B` (`lbl_a[12]`, `lbl_b[16]` in a two-line test).
// The initialiser is inside an `extern "C" { }` block rather than written on the declaration,
// which keeps g++ from warning "initialized and declared 'extern'".
extern "C" {
char lbl_803B36F0[0xC] = {0};
char lbl_803B36FC[0xC] = {0};
char lbl_803B1750[0x10] = {0};
}