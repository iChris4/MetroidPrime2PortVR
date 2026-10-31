# Metroid Prime 2: Echoes native port — bring-up notes

Working notes for a native-PC port of **Metroid Prime 2: Echoes** (`G2ME01`, USA
v1.00), built the same way as the Metroid Prime port next door
(`../MetroidPrimePort`): the [PrimeDecomp/echoes](https://github.com/PrimeDecomp/echoes)
matching decompilation is compiled against [Aurora](https://github.com/encounter/aurora),
an MIT GameCube SDK/GX compatibility layer, and runs on the GPU directly — no
emulator and no static recompilation.

No game content is included. The port reads a user-supplied disc image.

## Status (2026-09-24)

**The scaffold compiles.** All 109 decompiled game and engine translation units
build against Aurora's SDK headers with **0 errors** and produce 109 objects.
They cannot be linked or run yet, and the reason is not the port layer.

The blocker is decompilation coverage. From [decomp.dev](https://decomp.dev/PrimeDecomp/echoes):

| | Echoes `G2ME01` | Metroid Prime `GM8E01_00` (for reference) |
| --- | --- | --- |
| Functions matched | 2,024 / 28,465 (**7.1%**) | 16,317 / 16,685 (**97.8%**) |
| Code size | 6.54 MB (DOL 3.81 + 2.73 in RELs) | 4.01 MB (single DOL) |
| DOL | 1,834 / 16,726 fns (**11.0%**) | 16,317 / 16,685 fns |
| REL modules | 190 / 11,739 fns (**1.6%**), 86 modules | none |
| Fully linked | 4.5% | 49.2% |

Metroid Prime's port consumed a decompilation that was 97.8% complete. Echoes is
at 7.1%: the same exercise repeated today would need roughly 26,000 more
functions written before the port can do anything with them.

## Blocker ranking

1. **Decompilation coverage** — years of upstream work; nothing here shortcuts it.
2. **REL modules and `OSLink`** — new subsystem, see below. Needed by *every*
   route to a native Echoes, including a static recompilation.
3. **Port layer adaptation** — the part this scaffold starts. Bounded, and mostly
   transferable from the Metroid Prime port.

### REL modules are the structural difference from Prime 1

Echoes splits gameplay across `main.dol` plus 86 REL modules (one per enemy,
boss and script family: `DarkSamus`, `EmperorIngStage1..3`, `SpacePirate`,
`Blogg`, `SandBoss`, `Tweaks`, `ScriptGui`, …), which the game loads from the
disc as areas load and links at runtime against the DOL's exports.

Prime 1 is not quite a single DOL either — its disc carries `NESemu.rel`,
`NESemuD.rel`, `NESemuP.rel` and `Tweaks.Pak` — but its port sidesteps modules
entirely by compiling those sources into the executable and excluding the NES
emulator (see `files.cmake` in this tree). Echoes cannot do that: 86 modules
carry most of its gameplay.

The port therefore implements the runtime itself (see **REL runtime** below).
Aurora declares `OSLink`/`OSLinkFixed` in
`extern/aurora/include/dolphin/os/OSModule.h` but implements neither, and it
cannot host the linker as written — see the first finding below.

**As of 2026-09-26 the runtime is compiled into the port** (`mp_platform`). It had been in neither
`mp_platform` nor `files.cmake`, only in the two test executables, so the module loader was in no
binary the port produces. **But nothing calls it, and the caller cannot be written yet:** the retail
module descriptor table — which modules exist, where they sit on the disc, in what order — is not in
this tree. Measured absent from the DOL (2 of 86 names appear, as incidental strings), from
`config/` and from `orig/`. `docs/research/rel_module_manager.md` has the measurement, what a disc
image would give us, and the alternative of generating the table from `RelProd/`. The runtime half is
done and tested against all 86 modules; only the table lookup is missing.

## REL runtime (2026-09-24)

`platform/include/port_rel.h` and `platform/rel.cpp` implement the module
runtime: a 32-bit guest arena, the REL probe/loader, the linker (a port of the
console's `OSLink` — section and bss placement, import lists, every relocation
type the console handles, the `R_DOLPHIN_*` control records, and unlink with
undo) and the module registry (`SearchModule` answers the pointer-to-module
queries the OS error paths use). It deliberately does not live in Aurora: see
the first finding.

### Three things that shape it

1. **Aurora's `OSModuleHeader` cannot overlay a REL image on a 64-bit host.** On
   the console the struct is built with 32-bit pointers, so the loaded image can
   be overlaid with it and patched in place. On the host `OSModuleLink` holds two
   8-byte pointers, which shifts every field after it, while the relocation
   encoding is fixed at 32 bits. The port therefore works on the file's own
   layout (`RelModuleHeader`, `RelSectionInfo`, `RelImportInfo`, `RelReloc`) and
   keeps its registry outside the image. An earlier attempt that used the SDK
   structs silently read `version` from the `nameSize` bytes; the real-module
   test now catches exactly that.
2. **The image is big-endian and stays that way.** Only the header, section
   table, import table and relocation records are converted to host order, for
   the linker's benefit. Section payloads keep their guest bytes and relocation
   results are written in guest byte order, so the linked image is the same image
   the console would produce — which is what a recompilation or interpreter
   runtime needs. Consumers of pointer-bearing data convert per field, as the
   port does for other disc resources.
3. **The arena has to sit in the console's module region.** `R_PPC_REL24` encodes
   ±32 MiB, and a module's addends are absolute addresses in the DOL (0x80003100
   upward). Anywhere below 4 GiB is not enough: linking the real module at
   0x40000000 produced branches that decode back to 0x40389a84 instead of
   0x80389a84. `GuestArena(size, preferredBase)` takes a base; the tests use
   0x81000000.

### Verification

- `port_rel_tests` — a hand-built module exercising every relocation type the
  linker implements, the code flag on section offsets, bss placement and zeroing,
  prolog/epilog/unresolved resolution, a second module importing from the first,
  unlink-and-undo, the registry, and a malformed image.
- `port_rel_real_tests --dir <rel-directory>` — **all 86 modules of a `G2ME01`
  disc**, extracted with `tools/extract_disc_file.py` and linked in dependency
  order (27 of them import other modules). It checks every module's header,
  section and bss placement and entry points, then decodes every relocation back
  out of the patched image: **96,533 checks, 49,396 of them imports from the DOL,
  each landing on a symbol in `config/G2ME01/symbols.txt`, 0 unresolved, 0
  failures**, and then unlinks all 86.
- The same test on `NESemuP.rel` from a Metroid Prime disc keeps the version-2
  layout covered: 1474 checks, 411 DOL imports, all on known symbols. That module
  is the reason the version-2 path is still exercised — it puts its relocation
  records *before* the import table, where Echoes' version-3 modules put them
  after `fixSize`.
- Configure with `-DMP_REL_FIXTURE=<module.rel or directory>` to run it; without
  it ctest reports the test skipped (`SKIP_RETURN_CODE 77`). The symbol list
  defaults to the decompilation's own `config/G2ME01/symbols.txt`.
- The 109 game translation units still compile, and all three tests pass.

To reproduce the fixture set:

```sh
python3 tools/extract_disc_file.py "/path/to/Metroid Prime 2 - Echoes.iso" \
    --extract-dir RelProd /tmp/e2rels
cmake -S . -B build/probe -DMP_REL_FIXTURE=/tmp/e2rels
```

### The version-3 image layout

Echoes' modules are all version 3, and their `fixSize` covers the header, section
table, section data and import table — the **relocation records follow it in the
file** (`relOffset == impOffset + impSize`). Prime 1's version-2 module is the
other way round: records before the import table, no `fixSize`.

The first version of the loader treated `fixSize` as the image size and copied
only that much into the arena, which would have rejected every Echoes module as
unterminated. It now maps the whole file — everything the linker walks has to be
addressable — and reports `fixedSize` separately, with `Probe` rejecting an image
whose import table is not inside its fixed part.


### Still missing

- `OSLinkFixed`'s fixed-address path (the version-3 `impSize` truncation).
  Version 3 is the precondition for it, and every Echoes module is version 3, so
  the game may well use it. Leaving it out is still behaviour-preserving here:
  re-relocating a module against one that is already linked computes the same
  absolute addresses every time, so the port simply does the extra work. The
  truncation only avoids that work, and the unlink path does not depend on it.
- The game-side module manager — which module an area loads, and the module
  entry points the game calls — is part of the undecompiled 93%.
- Nothing loads modules yet: the runtime is exercised by tests only.

## Platform layer and the SDK link gap (2026-09-24)

### What the compiled game still needs from the SDK

Measured, not guessed: `nm -u` over the built objects, classified against what
Aurora actually defines (not just declares).

- **99** SDK entry points are referenced by the 109 compiled translation units.
- Aurora provides **79** of them.
- **20** are missing. The port's carried-over platform layer already implements
  17 (13 stubs, the 4 AI DMA entries in `ai_dma.cpp`); the 3 genuinely new ones —
  `OSClearContext`, `OSSetCurrentContext`, `OSGetStackPointer`, referenced by
  `RAssertDolphin` and `REL_Setup` — are now stubbed there.
- Script loaders are upstream's since the ninth sync (2026-10-02): `CEntity* LoadX(CStateManager&,
  CInputStream&, CEntityInfo&)`, non-const. A port stub or forwarder spelled with `const CEntityInfo&`
  is a different symbol and resolves nothing.
- The carried-over `OSLink`/`OSUnlink` **no-op stubs were removed**: they returned
  `TRUE` without linking anything, which would have made module loading silently
  appear to work. A call to the SDK entry point is now a link error on purpose. The
  module manager's link/unlink (`CRELFileToken::Link/Unlink` in `src/MetroidPrime/CRelFile.cpp`,
  upstream's unit since the eighth sync, 2026-10-01; its `TARGET_PC` blocks replaced the port-only
  `PortModuleManager.cpp`) calls `port::modules::Prolog/Epilog` instead, which run a compiled-in module's
  init/shutdown by disc name; `port::rel::LinkModule` is not used there because
  the image's code is PowerPC.

`platform/shims.cpp`, `platform/sdk_stubs.cpp`, `platform/glibc_compat.c` and
`platform/entry.cpp` are built as the `mp_platform` target, so the SDK-facing
layer stays compiled while the game cannot link; `platform/main.cpp` is compiled
by `mp_port_entry`. `tools/probe_sources.sh` runs the same sweep without a
configure (280 files: the 114 game units plus those five).
`ai_dma.cpp` needs SDL3 and `disc.cpp` needs the game's resource model, so both
stay out of the build for now. `debug_ui.cpp`, `port_textures.cpp`,
`port_prompts.cpp`, `port_randomizer.cpp` and `smoke.cpp` are still
Metroid-Prime-shaped; `platform/README.md` lists what each waits for.

### The entry point, and why nothing runs yet

The decompilation exports the seam the port needs:
`InvokeCMain(argc, argv, COsContext*, void*, CMemorySys*, void*)` in
`src/MetroidPrime/main.cpp:79`, which constructs `CMain` and calls `RsMain` —
cleaner than Metroid Prime, whose port had to rename the decompilation's `main`
to `metroid_main` because that is what its bootstrap exported.

`platform/main.cpp` is that entry point: the proven sequence
(`aurora_initialize` → disc resolution → `aurora_dvd_open` → check the disc is
`G2ME01` revision 0 → `aurora_update` → hand control to `InvokeCMain` →
`aurora_dvd_close` → `aurora_shutdown`). The parts that do not need the game are
split into `platform/entry.cpp` and covered by `port_entry_tests`: which disc the
port accepts, and how it finds the image (argument, then `MP2_DISC`, then the
first image beside the executable).

`InvokeCMain` takes the OS context and memory system from *its* caller, and that
caller is not in this tree's sources, so the entry point builds them the way that caller will
need to:

> **Corrected 2026-09-25.** This used to say the caller "is not decompiled". It is: it is
> `main` at **0x801EFB00**, 0x168 bytes, named in `config/G2ME01/symbols.txt:7983`, and it is
> the DOL's own entry. Nothing here has written it yet, but it is *identified*, and reading it
> changed two things:
>
> - **The window is opened there, not in `CMain`.** `main` builds an 8-byte object with
>   `fn_802BE85C` and passes it as `InvokeCMain`'s sixth argument; that reaches
>   `fn_802C329C` and then `fn_802C2FD4`, and `fn_802C2FD4` is retail's window/VI bring-up
>   (`VIGetTvFormat` → `GXAdjustForOverscan` → two framebuffers → `VIConfigure` → `VIFlush` →
>   `GXInit` → `GXSetCopyFilter`). **There is no `CMain::OpenWindow` in this game** —
>   `include/MetroidPrime/CMain.hpp`'s declaration is Metroid Prime carry-over, and no
>   symbol by that name occurs anywhere in the DOL.
>
>   **Updated 2026-09-29: `fn_802BE85C` is `CGraphicsSys::CGraphicsSys` and the port runs
>   it.** `platform/main.cpp` constructs a real `CGraphicsSys(osContext, memorySys, false)`
>   between `CMemorySys` and `InvokeCMain` — retail's own order — and its body is
>   `CGraphics::Startup` → `CGraphics::ConfigureVideo`, port-only in
>   `src/Kyoto/Graphics/CGraphicsHostStartup.cpp` together with the rest of
>   `Startup` → `InitGraphicsVariables` → `ConfigureFrameBuffer` → `InitGraphicsDefaults` →
>   `SetDefaultVtxAttrFmt`. The `false` is retail's progressive flag, which retail reads out
>   of the console's saved region (the *next* bullet) and the host does not have.
>
>   The host-only `CMain::OpenWindow` that used to stand in for this — a call into
>   `COsContext::OpenWindow` from `CMain::RsMain` — **is deleted**. It could not be right:
>   it wrote `COsContext::mRenderMode` at +0x30, and nothing in this DOL ever reads that
>   word; the renderer reads CGX's `mRenderModeObj__9CGraphics` at 0x80417264, which only
>   `ConfigureVideo` fills. `COsContext::OpenWindow` is now uncalled and the `CMain`
>   declaration is unused. What the port gained is a non-zero `fbWidth`, so `EndScene`'s
>   fade quad and `GXCopyDisp` run instead of being skipped, and a written
>   `CGraphics::SetViewport` (0x802C207C).
> - **The port passes `nullptr` for one of `main`'s five arguments**, at the
>   `InvokeCMain(...)` call in `platform/main.cpp`: retail's second is a 12-byte
>   saved-region helper (`fn_801EFC68`: `OSGetSavedRegion`, `OSSetSaveRegion(0,0)`, a
>   128-byte copy into a global) — it is also where retail's progressive flag comes from.
>   Its fifth, the graphics object, is real since 2026-09-29. Aurora substitutes for the
>   rest.
>
> `docs/research/boot_path.md` is the measured, step-by-step map from here to a rendered frame.
> Read it before planning this half of the project; it also corrects
> `docs/research/port_link_gap.md`, whose claim that the frame loop is "not a symbol problem"
> was wrong.

**All three of the objects the entry point builds are now defined, so the entry
point's game-side link gap is zero** (measured: `nm -u` on
`mp_port_entry`'s object minus everything `mp_game`/`mp_platform` define; the
only things left are Aurora's own entry points, libc and the C++ runtime, which
`MP_SDK_HEADERS_ONLY` deliberately does not link).

| what | where it is defined |
| --- | --- |
| `COsContext` — all six methods plus the `mProgressiveMode` static | `src/Kyoto/Basics/COsContext.cpp` (new) |
| `CMemorySys` — ctor, dtor, `GetGameAllocator` | `src/Kyoto/Alloc/CMemory.cpp` (already there) |
| `CGraphicsSys` — ctor, dtor, and `CGraphics::Startup`'s whole chain | `src/Kyoto/Graphics/CGraphicsHostStartup.cpp` (new, 2026-09-29) |

`CGraphicsHostStartup.cpp` is the third of the three port-only files that replace retail
bodies rather than decompiling them, and it is the one where the split is cleanest: upstream's
`src/Kyoto/Graphics/DolphinCGraphics.cpp` *is* retail's Echoes `CGraphics`, and it cannot be
listed — four compile errors that are not local to it, and it pulls in `CCubeModel`,
`CCubeMaterial` and `CFrameDelayedKiller`. So its bodies are copied, the eleven `CGraphics`
static data members the chain needs are defined beside them with retail's initial values, and
the members a carve already owns are reached by dtk's `extern "C"` name instead — the same rule
`CGraphicsHostGlobals.cpp` follows, and for the same reason. One retail call stays a reach stub
because it has no decompiled body in this tree: `CTevCombiners::Init` (`fn_802BE51C`).
`fn_8032F6EC`, the skinned-model workspace set-up, is hand-written from the asm in the port-only
`src/Kyoto/Graphics/CGraphicsHostWorkspace.cpp` (superseded 2026-10-01: it was a stub).

Two corrections to what this section used to claim. `CMemorySys` was never
missing: all three of its methods, and the `gGameAllocator` that
`GetGameAllocator()` returns, have been in `CMemory.cpp` since the port's first
build — the header having no `.cpp` of its own is not the same as the class
having no definition. And `COsContext`'s definitions are port code, not
decompilation: the file is in `files.cmake` for `mp_game` but has no
`Object()` line in `configure.py` and no retail counterpart to match, so it does
not move the matching build (verified: `report.json` is byte-identical with and
without it).

`COsContext::OpenWindow` **is no longer called** (2026-09-29): the VI bring-up it performed is
`CGraphics::ConfigureVideo`, and it wrote a render mode at `COsContext`+0x30 that nothing in this
DOL reads. The body is kept — it is a written method of the class — and the reason it can no
longer be reached is in the file's own header. What it did when it was called: Aurora has
already created the window by then, so it was an adapter over Aurora's VI rather than a real
window setup, the same shape the Prime 1 port ended up with. `VIConfigure` is the one call Aurora acts on (it
publishes the EFB/XFB size the game's GX work produces); `title`, `x`, `y` and
`fullscreen` are retail's own window management for a window that does not exist
yet, and the title is set once in `AuroraConfig::appName`.

`COsContext::COsContext` also calls `OSInit()`, which nothing else in the port
does and which is **load-bearing**: it is the only thing that maps MEM1 and sets
the arena bounds. Without it `OSGetArenaLo()`/`OSGetArenaHi()` are both null,
`GetBaseFreeRam()` returns 0, and `CGameAllocator::Initialize` — reached from
`CMemorySys`'s constructor, which the entry point runs immediately — underflows
its heap size and throws `std::bad_alloc`. Retail gets this from
`CBasics::Init()`, which has no definition in this tree; it is a decompilation
unit, not port code, so the two halves of it are called directly instead
(`CStopwatch::InitGlobalTimer` is the game's own half, and `OSInitFastCast` and
`DVDInit` have no PC meaning — Aurora's `aurora_dvd_open` has already run).

`COsContext::Update()` is a deliberate no-op returning `true`, and that is
derived from its one caller: `CInputGenerator::Update` does
`if (!x0_context->Update()) return false;`, so the return value is a "keep
generating input" flag. The frame pump already lives in the game's main loop
(`aurora_update()`), and pumping it a second time from the input path would
consume events the loop expects to see. `GetOsKeyState` returns "nothing
pressed" for the same reason the Prime 1 port's does: there is no Dolphin
keyboard, and input goes through Aurora's controller layer.

**`CMain::RsMain` is an empty body** (`src/MetroidPrime/main.cpp:219`), so there
is nothing to run even once it links. The same file stubs the pieces that make
the rest of the bootstrap meaningful:

| function | line | state |
| --- | --- | --- |
| `CMain::RsMain` | 219 | empty — the game loop |
| `CMain::InitializeSubsystems` | 88 | `ARInit` plus a TODO |
| `CGameGlobalObjects::AddPaksAndFactories` | 203 | empty — registers asset factories |
| `CMain::FillInAssetIDs` | 215 | partial, TODOs |
| `CMain::AsyncIdle` | 221 | partial |
| `CMain::MemoryCardInitializePump` | 201 | empty |
| `CMain::DrawDebugMetrics` | 205 | empty |
| `CMain::CheckReset` | 213 | empty |
| `CMain::ShutdownSubsystems` | 93 | empty |
| `CGameArchitectureSupport::Update` | 199 | empty |
| `CMain::StreamNewGameState` | 275 | TODO |

`CGameGlobalObjects::PostInitialize` (98) and `LoadStringTable` (106) *are*
implemented, and `CGameArchitectureSupport`'s constructor is substantial, so the
bootstrap is in progress rather than absent. Until `RsMain` and the paks/factories
land there is nothing for the port's entry point to drive; that is upstream's
work, and no port-side change can substitute for it.

## The shim queue

The first sweep produced 187 errors across all 110 sources. Applying the queue
below left 0 errors, and a clean build produces 109 objects with 25 warnings
(4 upstream categories, listed under open issues below). This is the port's
whole SDK-compatibility cost so far.

| Errors | Root cause | Fix |
| --- | --- | --- |
| 106 | `include/Kyoto/Alloc/CMemory.hpp` redeclared the standard placement `new` and defined weak global `operator delete`, conflicting with `<new>` | Guard both behind `__MWERKS__ || CLANGD`; the host runtime owns global new/delete |
| 46 | `src/Kyoto/CARAMManager.cpp` — `CARAMManager.hpp` is a stub missing the members the `.cpp` defines, and the TU is not in upstream `configure.py` | Excluded from the file list; re-enable when upstream finishes it |
| 12 | `src/rstl/rstl_strings.cpp` specialized members after implicit instantiation (MWCC allowed use-before-specialization) | Declared every specialization in `rstl/string.hpp` and again at the top of the `.cpp`, as the Metroid Prime port does |
| 11 | `src/Kyoto/Basics/RAssertDolphin.cpp` dumped `OSContext::gpr/lr/cr/srr0`; Aurora's `TARGET_PC` `OSContext` is opaque | Dump guarded with `#ifndef TARGET_PC`, with a one-line substitute |
| 2 | LZO compiled for 32-bit `size_t` | `SIZEOF_SIZE_T` from `CMAKE_SIZEOF_VOID_P` in `files.cmake`, plus a `TARGET_PC` fallback in `lzo_conf.h` for compiles that bypass CMake |
| 1 | `IRenderer::AddParticleGen` — derived code overrides a bounds-taking overload the base never declared | `AddParticleGen2` was a placeholder name for that overload; it now carries the real signature (same vtable slot, so the layout is unchanged) |
| 1 | `CActor::AddToRenderer` was missing the frustum-taking overload derived classes override | Added alongside the existing 1-arg form |
| 1 | `CGX::SetArray` — Aurora's `TARGET_PC` `GXSetArray` needs the array size and endianness | Forwarded with size 0 (see the open issue below) |
| 3 | `CDvdFile::ARAMARAMXferCallback` and two address casts assumed 32-bit pointers | Signature and casts moved to `uintptr_t`, matching Aurora's `ARQCallback` |
| 1 | `RstlExtras.cpp` declared `char* strchr(const char*, int)`, clashing with modern glibc's const-correct one | Declaration dropped; the host libc's is used |

## Open issues carried in the port

These are known, deliberate and documented in the code. None is fixed by the
port; each needs either upstream decompilation work or Aurora work.

- **`CGX::SetArray` has no size parameter.** Aurora uploads exactly `size` bytes
  of a vertex array to the GPU and renders nothing for an array of size 0. Echoes'
  console signature `(attr, data, stride)` cannot supply one, and no caller is
  decompiled yet. The forwarder passes 0. When the model code lands, Echoes must
  grow the size-taking overload the way Metroid Prime's
  `include/Kyoto/Graphics/CGX_Impl.hpp` does.
- **Five functions fall off the end of a non-void function** — they are
  unfinished decompilations that MWCC accepted as undefined behaviour:
  `MetroidPrime/main.cpp` (×2), `ScriptObjects/CScriptForgottenObject.cpp`,
  `ScriptObjects/CScriptPickup.cpp`, `ScriptObjects/CScriptSpawnPoint.cpp`.
  The port build keeps `-Wno-error=return-type` so they warn rather than fail;
  the Metroid Prime port uses `-Werror=return-type`, and this should return to
  that once upstream completes them. The port deliberately does not invent
  return values in not-yet-decompiled code.
- **`single_ptr`/`rc_ptr` delete incomplete types** (`-Wdelete-incomplete`
  warnings). Console-side lifetime idiom; harmless as warnings, and unchanged
  from the decompilation.
- **`REL_Setup.cpp::_unresolved` walks the PowerPC stack.** The REL module's
  handler for unlinked imports reads the 32-bit back-chain via
  `OSGetStackPointer`, which neither the host stack layout nor 64-bit pointers
  match (`-Wint-to-pointer-cast`). It only matters once a REL is actually loaded,
  so it belongs with the REL runtime work in the first next step.
- **`CGameOptions::ResetControllerAssets` reads past a fixed-size array**
  (`-Waggressive-loop-optimizations`, iteration 1 of an inlined loop is undefined
  behaviour). Upstream's; it compiles but needs fixing before it runs.

## What transfers from the Metroid Prime port

Aurora is game-agnostic, and Echoes is the same engine generation (Kyoto, `rstl`,
`CMain`/`CGameGlobalObjects`/`InitializeSubsystems` bring-up). Expect to reuse:

- `platform/compat.h` — already in use here; it restores the SDK macros Aurora
  omits and maps `triggerL/R` to Aurora's `TARGET_PC` `PADStatus`.
- `platform/include/dolphin/{arq.h, gba.h, PPCArch.h, thp/, gx/GXShims.h}` — the
  SDK headers Aurora omits.
- `platform/{disc.cpp, sdk_stubs.cpp, shims.cpp, glibc_compat.c}` — disc mounting,
  GX/SI/PAD token shims, the glibc floor.
- The `extern/aurora` and `extern/musyx` snapshots (both MIT, vendored, not
  submodules). **Caveat:** the vendored MusyX is Prime 1's snapshot; Echoes ships
  a later MusyX (streamed music, different managers) and will need its own audio
  bring-up, as the Prime port's MusyX work shows.
- The Prime port's playbook in `../MetroidPrimePort/PORT_NOTES.md` and
  `docs/NATIVE_PORT.md`, including the widescreen/HUD, mouse-aim and save work
  that has to be redone against Echoes' layouts.

`platform/` here is a carried-over copy of the Prime port's platform sources. It
is **not referenced by the build**: several files are MP1-specific (textures,
prompts, randomizer, debug UI) and all of them assume MP1's entry point and
globals. They are the adaptation baseline, not working code.

## The matching build, locally (2026-09-24)

The decompilation now builds here, which matters twice: it is how progress is
measured, and it is the gate every port edit to a decomp source has to pass.

**Toolchain** (all already present, from the Prime 1 work):

- MWCC compilers: `../MetroidPrimePort/build/compilers` (`GC/2.7` for the game
  and RELs, `GC/1.2.5n` for the SDK, `GC/1.3.2` for script objects).
- `dtk`, `wibo` (runs the Windows compilers) and `objdiff-cli`/`sjiswrap.exe`
  from `../MetroidPrimePort/build/tools`, staged into `build/tools`.

**Originals**: `orig/G2ME01/sys/main.dol` and `orig/G2ME01/files/RelProd/*.rel`,
extracted from an owned disc with `tools/extract_disc_file.py`. Every hash matches
`config/G2ME01/config.yml` (the DOL's is `6ef9b491…34d0010`).

```sh
python3 configure.py --version G2ME01 \
    --compilers ../MetroidPrimePort/build/compilers \
    --dtk ../MetroidPrimePort/build/tools/dtk \
    --wrapper ../MetroidPrimePort/build/tools/wibo --build-dir build
ninja
```

**Result**: `dtk` analyses 87 modules, finds the 28,465 functions and splits 1091
objects; MWCC compiles the decompiled translation units; the link produces
`build/G2ME01/main.dol` **byte-identical to retail** and all 86 RELs identical,
because everything not yet decompiled comes from the originals. `build.sha1`
reports `87 files OK`, and the progress report reproduces decomp.dev exactly:

```
All:  5.97% matched, 4.54% linked (2024 / 28465 functions)  [checkpoint at the time; see
docs/HANDOFF.md for the current position - build/report.json is the source of truth]
DOL:  9.57% matched        Modules: 0.93% matched
Game: 34.67% matched       SDK: 97.98% matched
```

### TARGET_PC, and the rule for port edits

The decompilation is port-aware: 11 files already carry `#ifdef TARGET_PC`
branches — the SDK headers under `include/dolphin/` (GX geometry/enums/structs,
`pad.h`, `vi.h`, `types.h`) and `src/Kyoto/Graphics/CGX.cpp`. The port defines
`TARGET_PC`; the matching build does not. So port edits to decomp sources follow
that convention exactly:

- Port-only behaviour goes behind `#ifdef TARGET_PC`.
- A declaration that would change a class's vtable (adding a virtual) is guarded
  too, because the matching build measures those layouts.
- Nothing else under `src/` or `include/` may diverge from upstream.

The rule earned its place immediately. The matching build's first run failed on
`include/Kyoto/CDvdFile.hpp`, where the port had widened an ARQ callback to
`uintptr_t` — a type MWCC cannot see. The port-only part now lives in
`DolphinCDvdFile.cpp` behind `#ifdef TARGET_PC` (an adapter callback and
pointer-width arguments at the call site), with the header back to the console's
`u32`. The same treatment went to `CGX::SetArray`'s fifth argument, and to the
`IRenderer`/`CActor` overloads the port's derived classes need.

`libc/` and `scripts/` were missing from the fork; they are part of the
decompilation and are now present.

**`CGameState` and `CPersistentOptions` no longer have a `TARGET_PC` layout (2026-10-01).** The
host used an opaque byte layout for both, with carves (`CGameStateCtor.cpp` and eleven more)
written against it. Upstream's `Player/CGameState.cpp` could not compile against that, and the
memory card needs its `InitializeMemoryStates`/`StateForWorld`. The headers now have one layout,
upstream's; `Player/CGameState.cpp`, `CWorld.cpp`, `CGameArea.cpp` and `CMemoryCard.cpp` are
listed whole and the sixteen carves they replace are in `check_files_cmake.py`'s `EXCLUDED` as
superseded. `PortStreamNewGameState.cpp` copies the real `rstl` members (it is a friend).

**`operator delete` routes by address (2026-10-01).** On the host `rs_new` is plain `new`, but
`CMemory::Alloc` is the game heap carved from the Aurora arena, and `Matching` units pair the two:
`CResLoader::AddGroupCache` puts a `CMemory::Alloc` buffer in an `rstl::auto_ptr`. glibc aborts on
that `delete` (`munmap_chunk(): invalid pointer`). `src/Kyoto/Alloc/PortMwccNew.cpp` therefore
defines every global `operator delete`: a pointer inside `[gPortGameHeapLo, gPortGameHeapHi)`
(set in `platform/main.cpp` from `OSGetArenaLo/Hi` before `CMemorySys` is built) goes to
`CMemory::Free`, anything else to `std::free`.

### Tweaks after the third upstream sync (2026-09-29)

Upstream (a14f961) rewrote the tweak layer, and the port follows it:

- The `gpTweak*` globals are `rstl::single_ptr<CTweakX>` (call sites use `.get()` and
  `.null()`), except `gpTweakPlayerGun`, which stays a raw pointer. All 16 slots have DOL
  addresses 0x80418F28..0x80418F64; the port defines them in `src/MetroidPrime/PortGlobals.cpp`.
- `CTweakPlayer` is built from a `const SLdrTweakPlayer&` and keeps it in a private `mData`.
- `src/MetroidPrime/ScriptLoader/Tweaks.cpp` is generated upstream and defines every
  `SLdrTweak*`/`SLdrT*` constructor, destructor and loader. `SLdrStructMembers.cpp` no longer
  defines those; defining them there too is 200+ duplicate symbols.
- `Tweaks/Tweaks.cpp`'s real `REL_CreateTweakGlobals` now compiles on the host (the old
  `TARGET_PC` empty stub is gone); only `RELMain`/`RELExit` are renamed for the host.
- Upstream selects versions with `-DVERSION=<n>` against `include/GameVersions.h`, not
  `-DVERSION_G2ME01`. `CMakeLists.txt` passes `VERSION=0 BUILD_VERSION=0` to `mp_game`, and the
  `tools/probe_*` scripts pass `-DVERSION=0`.

## How a unit actually completes (2026-09-25)

Worth writing down, because it corrects an assumption made earlier in this file.

`ninja` links `build/G2ME01/main.dol` from **`build/G2ME01/obj/*.o`** - objects that
`dtk dol split` cut out of the retail binary - and then converts the linked ELF with
`elf2dol`. Our own objects (`build/G2ME01/src/*.o`) are compared against those by
objdiff for the progress report, but they are **not** in the link.

Marking a unit `Matching` in `configure.py` swaps its `obj/` input for its `src/`
one. That is the real completion test: a unit is only genuinely done when the build
still reproduces retail with our object linked in. Two consequences:

- The "DOL sha1 is unchanged and all 86 RELs match" check this file cites after
  every round only proves that for units *already* marked Matching. For the rest it
  validates the untouched parts of the DOL, not the work.
- Flipping a unit whose object is not link-equivalent changes the DOL, and because
  the REL relocations point at DOL addresses, it changes almost every REL too.

Trying that flip on the units that look complete (100% code, 100% data) is how this
was found: five of them are still short at the link level. `tools/compare_unit.sh`
now reports the difference per unit; it ignores `.comment` (the compiler banner,
different by construction) and `.note.split` (a dtk artifact), and requires every
other section to match in size and content, plus equivalent symbols.

`Kyoto/CFrameDelayedKiller` is the smallest example:

| | retail-derived | ours |
| --- | --- | --- |
| `.text` | 0x5ac | 0x5ac, but contents diverge from 0x190 |
| `.sbss` | 8 | 9 |
| `.sdata` | absent | 2 bytes |
| symbols | `lbl_803E04E0` | `kUnknownValueEqualKey__4rstl`, `kUnknownValueNewItem__4rstl`, `kUnknownValueNewRoot__4rstl` |

So three extra bytes of rstl sentinel statics are emitted in this unit, and part of
`.text` differs even though objdiff calls every function in it matched - the
per-function report does not measure the gaps between declared symbols, which the
object comparison does.

### A correction: the "prologue scheduling" shape is constness

An earlier note here called "retail keeps the first member load outside the prologue stores"
unfixable, and blamed the compiler. It is not: our compiler hoists loads through a `const this`
or a `const T&` parameter, and the same body in a non-const method matches retail. Where the
signature was a guess, dropping `const` converted four more functions. Where Prime 1 or Trilogy
show the declaration really is const, the function stays unmatched - so it is a signature
question, not a codegen dead end.

The related shape that *is* a dead end remains the constant-in-a-callee-saved-register one, and
Prime 1's decompilation never matched those either.

### A scaffold alone does not build a module

`tools/scaffold_rel_module.py` writes the split ranges a module's own unit will claim, but
trying it on `SkyRipple` broke the module's hash: `config/G2ME01/build.sha1` went from 87
files OK to one failure, and 85 of 86 RELs still compared clean. The reason is the same
mechanism as the DOL's: a unit's ranges have to be reproduced byte-for-byte by that unit's
own object, so assigning ranges to a unit whose source is still empty removes those bytes
from the link. Scaffold and implement together, and watch the module's hash.

`configure.py` has a requirement worth knowing, and an earlier version of this note got it
backwards: a `Matching` object with no source file does **not** stop it. It prints
`Missing source file`, links the retail object and carries on - so the unit reads as `Matching`
while our code is in no link. `tools/gate.sh` and `tools/flip_test.sh` both refuse that case. That is how two units flipped earlier turned out to be a
bad flip: the flip test ran ninja without forcing regeneration, and a stale `build.ninja`
hid the failure. `tools/flip_test.sh` now checks for the source file and runs
`configure.py` explicitly before ninja.

### The REL modules

The 86 modules under `RelProd/` are the larger half of what is left (about 11.3k functions) and
they have no source units at all: dtk's `auto_*` units hold them, and every module's
`splits.txt` is empty except for its section list. The pattern to start one is now in
`tools/scaffold_rel_module.py`: it reads the module's version-3 sections and its `symbols.txt`
and prints the three artifacts - a splits entry that gives the module's own translation unit the
leading range of each section and `REL/REL_Setup.cpp` the tail (the shape
`config/G2ME01/rels/ForgottenObject/` already uses), a `Rel(...)` call for `configure.py`, and a
source skeleton listing the module's functions in address order. `--write` applies them.
`IngSwarm` is the degenerate case worth knowing: its entire `.text` is REL_Setup, so it has no
class code to write at all.

### Naming is what pairs a function with retail's

objdiff pairs by symbol name. A function of ours that matches retail byte for byte still
reads as unmatched if retail's symbol is an unnamed `fn_<address>` - so reconstructing a
function usually means renaming its retail symbol too, in `config/G2ME01/symbols.txt`.
The converse surprised a session: a *call or vtable target* inside the function can stay
auto-named and the function still scores 100%, because only the function's own symbol has
to line up. That makes the TypesMatch unit's 350 renames the model to follow: same unit,
same names, verified by the percentage moving.

Names come from, in order of reliability: another version's config already in this repo
(`config/R3ME01`, `R3MP01`, `R32J01` name 75 TypesMatch and 82 TCastToPtr functions),
this repo's own `symbols.txt` (an existing cast or vtable symbol gives a class away),
Prime 1's matched source, and the REL module a vtable reference lands in. Guessing is the
last resort and has to be marked in the source as inferred.

### Two things that block link-completion more often than code does

**Weak instantiations our compiler emits where retail's linker dropped them.** A unit
whose functions all match can still fail the flip by carrying extra code: MWCC emits
`__dt__`/copy instantiations for member templates where retail's build did not, and
the linker keeps whatever our object defines. `DolphinCLZOInputStream` passes anyway
(`CCircularBuffer`, already Matching, has the same stripped weak function), while
`CScanTreeInventory` does not. Two ways out: shape the templates so the instantiation
is not emitted here (that is how `rstl::construct.hpp` and `CInputStream::Get<T>`
were fixed, and those fixes brought nine units to link equivalence), or, as a last
resort, write the instantiation out under retail's `fn_*` name - which matches the
bytes but is reconstruction, not recovery, and needs saying so in a comment.

**Claiming data changes how our code addresses it.** `CScanTreeInventory`'s inventory
table (`lbl_803ACAE0`, 0xD4 in `.rodata`) sits in an `auto_*` unit because its split
omits it. Assigning that range to the unit and defining the table there - with the
retail symbol name and external linkage, values taken from retail - dropped the
unit's one function from 100% to 99.3%: retail's code references the table as an
*external* symbol, and a definition in the same translation unit makes MWCC address
it differently. Keeping retail-external data external is therefore the arrangement
that matches; moving a range into a unit is only right when retail references it as
its own.

The rule for future rounds: **flip the unit to `Matching` and rebuild.** The
project's own check (`config/G2ME01/build.sha1`) then fails loudly if the linked DOL
or any REL stops reproducing retail, which is exactly the question being asked.
`tools/compare_unit.sh` is a diagnostic for seeing *what* differs first, and it is
stricter than the link: trailing gap padding and weak code the retail linker drops
make most already-Matching units fail it while they still link identically.
`tools/flip_test.sh <unit>...` runs the flip test itself (flip, rebuild, check the DOL
and all 86 RELs, keep what holds and revert what does not), which is the one command
to reach for when a unit looks complete.

Applying that test to the nine candidates that looked complete: **six passed** -
`Kyoto/CFrameDelayedKiller`, `Kyoto/Math/CAABox`, `Kyoto/Streams/CFilePreload`,
`Kyoto/Streams/DolphinCLZOInputStream`, `MetroidPrime/CAxisAngle` and
`MetroidPrime/CEulerAngles` - and are now marked Matching, with the DOL and all 86
RELs still byte-identical with their objects linked in. That is real completion, not
a percentage, and it moved the linked metric from 4.54% to 4.68%.

The three that failed, with what blocks them:

| unit | blocker |
| --- | --- |
| `Kyoto/Math/CQuaternion` | `.sdata2` constant order: retail's 0.0 and 1.0 come from a function the linker dropped, and recreating it would add `.text` |
| `Kyoto/Graphics/DolphinCColor` | trailing `.sdata2` gap (0x14 vs 0x18); `lbl_8041F900` in `.sbss2` sits outside the split |
| `MetroidPrime/ScriptObjects/CScanTreeInventory` | the split covers only the loader's `.text`; its `.rodata`, `.sdata` and the rest have nowhere to go |

The last two need `config/G2ME01/splits.txt` changes; the first needs a decision about
how to account for linker-dropped code.

## Running the decompilation

**If you are starting fresh, read `docs/HANDOFF.md` first** - current position, verdict tools,
the open blocker, and what to do next.


`docs/RUNNING_THE_DECOMP.md` is the operating companion to this file: the measurement rig,
the one rule that decides whether a unit is actually done, how the parallel lanes are run and
why collecting them costs more than the work they produce, what to delegate to which lane, and
a list of every REL module attempted so far with what blocked it. Read it before starting a
round; keep it current when the strategy changes.

## Building

```sh
cmake -S . -B build/probe -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/probe -j "$(nproc)"
```

Two builds exist. The matching build above measures the decompilation. The port
build below compiles the same sources for the host: `MP_SDK_HEADERS_ONLY=ON`
(the default) builds them against Aurora's headers without linking Aurora, which
is the configuration the shim queue is tracked against. `-DMP_SDK_HEADERS_ONLY=OFF`
is the port shape: it adds `extern/aurora`, links the Aurora targets and builds
the `metroid_prime2_port` executable. **That configuration used to stop with a
`FATAL_ERROR`; it no longer does, and a real link has now been attempted.** What
it takes, all measured:

```sh
../MetroidPrimePort/build/review-tools/bin/cmake -S . -B /tmp/linkprobe -G Ninja \
  -DCMAKE_MAKE_PROGRAM=../MetroidPrimePort/build/review-tools/bin/ninja \
  -DMP_SDK_HEADERS_ONLY=OFF
../MetroidPrimePort/build/review-tools/bin/cmake --build /tmp/linkprobe --target metroid_prime2_port
```

Aurora configures from here in about 20 seconds - it fetches its own SDL3 and
Dawn, so there is no separate dependency to install first, which had looked like
a blocker. All 118 game units compile. The link then fails on **525 undefined
symbols and no duplicate definitions**, which is the first real measurement of
what is left.

**The modules we compile in.** *(Superseded 2026-10-08: the earlier text renamed each
module's `RELMain`/`RELExit` in the sources and kept a hand-written registry.)* On the cube every
REL is its own symbol namespace, so modules define the same `RELMain`/`RELExit`, module-local
`Load*` functions that also exist as DOL trampolines, and weak template copies. The host build
keeps that: `tools/gen_files_cmake.py` emits one `MP_MODULE_<Name>_SOURCES` list per
`Rel("<Name>", ...)` block in `configure.py` (plus `MP_MODULES`) into `files.cmake`; CMake compiles
each as an OBJECT library `mp_mod_<Name>` with the game's flags, and `tools/scope_module.py scope`
(target `mp_modules`) runs `ld.bfd -r` per module into `build-port/modules/<Name>.o`, renames
`RELMain`/`RELExit` to `mp_rel_<Name>_prolog`/`_epilog`, and `objcopy --localize-symbols` every other
defined symbol except those an object outside the module references that nothing outside defines.
The scoped objects, not the raw ones, link into the executable. `scope_module.py registry`
writes `build-port/generated/compiled_modules.inc` (name, prolog, epilog per module, configure.py
order), which `platform/compiled_modules.cpp` includes; `platform/main.cpp` runs it either side of
`InvokeCMain`. `src/REL/REL_Setup.cpp` (console `_prolog`/`_epilog` glue) is excluded from the
host build. `-Wl,--allow-multiple-definition` was the alternative and is worse: it would green
the link while one module's entry point ran and the rest were silently skipped.

The same sweep is available without a configure:

```sh
tools/probe_sources.sh          # syntax-check every source; -v prints errors
```

It compiles the 111 game units plus the `mp_platform` and `mp_port_entry`
sources (280 files), mirroring the build's flags: `compat.h` is C++-only, and the
bundled LZO `.c` files are compiled as C.

## Next steps, in dependency order

1. **Done: the REL runtime** (`platform/rel.cpp`) — loader, linker and registry,
   verified against 86 modules from an owned disc. What remains here is
   `OSLinkFixed`'s fixed-address path, and feeding it real disc reads.
0. **Decompilation, in parallel**: the matching build works locally and gates
   every port edit. The port half is built; the remaining 26,441 functions are
   the decompilation itself (see "The matching build, locally").
2. **Adapt the platform layer** (started): the engine-independent sources build,
   and the entry point is written against the decompilation's own seam.
   `COsContext`, `CMemorySys` and `CGraphicsSys` are all defined, so the entry
   point's game-side link gap is zero. The VI bring-up is reached, and it is
   reached as **retail reaches it** since 2026-09-29: `CGraphicsSys` is constructed in
   `platform/main.cpp` between `CMemorySys` and `InvokeCMain`, with
   `src/Kyoto/Graphics/CGraphicsHostStartup.cpp` carrying retail's `CGraphics::Startup`
   chain. The host-only `CMain::OpenWindow` stand-in is deleted; only the host-only
   `CMain::RsMain` remains in `src/MetroidPrime/PortBoot.cpp`.
   What is left is retail's `CMain::RsMain` body and the asset factories before it
   can draw. **`docs/research/boot_path.md` is the ordered, measured list of what
   stands between here and a rendered frame**, and it changes the shape of the work:
   the frame loop's own body is 2,584 bytes across twelve symbols that are *already*
   on `port_link_gap_list.md` (`CIOWinManager`, `CInputGenerator`, `CStopwatch::CSWData`),
   and what actually blocks it is two null-pointer dereferences in
   `CGameArchitectureSupport`'s constructor, not missing decompilation. Disc resources,
   CARD/saves, input and the debug overlay come after those — see "Platform layer and
   the SDK link gap" above.
3. **Track upstream**: re-run the compile and re-add `CARAMManager.cpp`, the
   size-taking `CGX::SetArray`, and the five unfinished functions as the
   decompilation fills them in. Tighten `-Werror=return-type` when it is complete.
4. **Only once the decompilation links**: entry point wiring, the game-side
   module manager and module entry points, `mp_game` link, packaging
   (AppImage/Flatpak/APK), widescreen, mouse aim.

A static recompilation of the DOL (the approach used for the earlier Metroid
Prime recompilation experiment) remains a faster route to a *playable* binary,
but it inherits the same REL problem and gives up the readable source that makes
widescreen, mouse aim and HD textures practicable. The Metroid Prime port's
history — rewritten from a recompilation experiment into a decompilation port for
exactly that reason — is the argument for this route.
