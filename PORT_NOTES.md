# Metroid Prime 2: Echoes native port — how it works

Notes for the native-PC port of **Metroid Prime 2: Echoes** (`G2ME01`, USA v1.00): the
[PrimeDecomp/echoes](https://github.com/PrimeDecomp/echoes) matching decompilation is compiled
against [Aurora](https://github.com/encounter/aurora), an MIT GameCube SDK/GX compatibility layer,
and runs on the GPU directly — no emulator, no static recompilation. No game content is included;
the port reads a user-supplied disc image.

Current position: `docs/STATUS.md` (port); decomp method: `docs/DECOMP_README.md`, `docs/DECOMP_WORKFLOW.md`. Build and run commands:
`README.md`, `docs/RUNNING.md`. The September 2026 bring-up log that used to fill this file (link-gap
measurements, the shim queue, the 7.1% coverage table, next steps) is kept verbatim in
`docs/history/PORT_NOTES-to-2026-10-29.md`; its numbers are stale.

## REL modules are the structural difference from Prime 1

Echoes splits gameplay across `main.dol` plus 86 REL modules (one per enemy, boss and script
family). The port compiles every module into the executable and implements the REL runtime itself
(`platform/rel.cpp`, `platform/include/port_rel.h`, covered by `port_rel_tests`), since Aurora
declares `OSLink`/`OSLinkFixed` but implements neither.

**The modules we compile in.** On the cube every REL is its own symbol namespace, so modules define
the same `RELMain`/`RELExit`, module-local `Load*` functions that also exist as DOL trampolines, and
weak template copies. The host build keeps that: `tools/gen_files_cmake.py` emits one
`MP_MODULE_<Name>_SOURCES` list per `Rel("<Name>", ...)` block in `configure.py` (plus `MP_MODULES`)
into `files.cmake`; CMake compiles each as an OBJECT library `mp_mod_<Name>` with the game's flags,
and `tools/scope_module.py scope` (target `mp_modules`) runs `ld.bfd -r` per module into
`build-port/modules/<Name>.o`, renames `RELMain`/`RELExit` to `mp_rel_<Name>_prolog`/`_epilog`, and
`objcopy --localize-symbols` every other defined symbol except those an object outside the module
references that nothing outside defines. The scoped objects link into the executable.
`scope_module.py registry` writes `build-port/generated/compiled_modules.inc` (name, prolog, epilog
per module, `configure.py` order), which `platform/compiled_modules.cpp` includes; `platform/main.cpp`
runs it either side of `InvokeCMain`. `src/REL/REL_Setup.cpp` (console `_prolog`/`_epilog` glue) is
excluded from the host build. `-Wl,--allow-multiple-definition` was the alternative and is worse: it
would green the link while one module's entry point ran and the rest were silently skipped.
`docs/MODULES.md` lists which modules compile.

## `TARGET_PC`, and the rule for port edits

The port defines `TARGET_PC`; the matching build does not. Port edits to decomp sources follow:

- Port-only behaviour goes behind `#ifdef TARGET_PC`.
- A declaration that would change a class's vtable (adding a virtual) is guarded too, because the
  matching build measures those layouts.
- Nothing else under `src/` or `include/` may diverge from upstream.

Examples: the port-only part of `CDvdFile` lives in `DolphinCDvdFile.cpp` behind `TARGET_PC` (an
adapter callback and pointer-width arguments), with the header back to the console's `u32`.
`CGameState` and `CPersistentOptions` use upstream's layout on the host too (no opaque byte layout).
The `gpTweak*` globals are `rstl::single_ptr<CTweakX>` and are defined in
`src/MetroidPrime/PortGlobals.cpp`; `ScriptLoader/Tweaks.cpp` is generated upstream and defines every
`SLdrTweak*` constructor, so `SLdrStructMembers.cpp` must not (200+ duplicate symbols). Upstream
selects versions with `-DVERSION=<n>` against `include/GameVersions.h`; `CMakeLists.txt` passes
`VERSION=0 BUILD_VERSION=0` to `mp_game`.

**`operator delete` routes by address.** On the host `rs_new` is plain `new`, but `CMemory::Alloc` is
the game heap carved from the Aurora arena, and `Matching` units pair the two (`CResLoader::AddGroupCache`
puts a `CMemory::Alloc` buffer in an `rstl::auto_ptr`). glibc aborts on that `delete`.
`src/Kyoto/Alloc/PortMwccNew.cpp` therefore defines every global `operator delete`: a pointer inside
`[gPortGameHeapLo, gPortGameHeapHi)` (set in `platform/main.cpp` from `OSGetArenaLo/Hi` before
`CMemorySys` is built) goes to `CMemory::Free`, anything else to `std::free`.

`COsContext::COsContext` also calls `OSInit()`, which nothing else in the port calls and which is
load-bearing: it maps MEM1 and sets the arena bounds. Without it `OSGetArenaLo/Hi` are null and
`CGameAllocator::Initialize` throws `std::bad_alloc` (retail gets this from `CBasics::Init()`).

## Two builds

- **Matching build** (measures the decompilation; uses the Prime 1 toolchain under `../MetroidPrimePort/build/`):
  `python3 configure.py --version G2ME01 --compilers ../MetroidPrimePort/build/compilers --dtk ../MetroidPrimePort/build/tools/dtk --wrapper ../MetroidPrimePort/build/tools/wibo --build-dir build && ninja`.
  The result must stay byte-identical to retail. Method: `docs/DECOMP_WORKFLOW.md`.
- **Port build** (compiles the same sources for the host):
  `cmake -S . -B build-port-link -G Ninja -DMP_SDK_HEADERS_ONLY=OFF` then
  `cmake --build build-port-link --target metroid_prime2_port`. `MP_SDK_HEADERS_ONLY=ON` (the CMake
  default) compiles against Aurora's headers without linking Aurora.
- `tools/probe_sources.sh` syntax-checks every source without a configure (`-v` prints errors).
