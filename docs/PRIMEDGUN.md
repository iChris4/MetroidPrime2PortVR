# PrimedGun Echoes: engineering notes

PrimedGun Echoes is the VR layer of PrimedGun v2 (iChris4/MetroidPrimePortVR, the Metroid Prime
VR fork) carried onto this port of Metroid Prime 2: Echoes. This file is the fork's own log,
newest first, kept apart from `PORT_NOTES.md` (upstream's) so upstream merges never conflict on it.

- Branches: `master` tracks `upstream/port` (github.com/Odrannnn/MetroidPrime2Port); VR work is
  on `OpenXR`.
- The Metroid Prime VR fork (`../MetroidPrimePortVR`, branch `OpenXR`) is the reference for every
  VR file; keep the shared files byte-identical where possible and list the deltas here.

## Aurora: the VR renderer merged over Echoes' aurora (2026-10-29)

Echoes vendors MP1's aurora: re-vendored from MP1 upstream `174975c2c` on 2026-10-23, then changed
(`docs/GOTCHAS.md`, "Full list of vendored-Aurora additions"). The MP1 VR fork's aurora is MP1
upstream `011c131db` (0.35.0 era) plus PrimedGun's stereo renderer. The merge is a per-file
three-way merge with base `174975c2c`, Echoes `HEAD` and the VR fork's `OpenXR`:

    python -I tools/primedgun/aurora_merge.py <scratch>        # merged tree + report.txt
    python -I tools/primedgun/cfl.py show <file> [hunk]         # diff3 hunks (echoes | base | vr)
    python -I tools/primedgun/cfl.py take <file> <n> echoes|vr|both-ev|both-ve
    python -I tools/primedgun/dupcheck.py <scratch> <scratch>/report.txt

Result on 2026-10-29: 294 files the same on both sides, 48 VR-only changes, 15 Echoes-only, 47
VR-only new files, 9 Echoes-only new files, 18 clean merges, 22 conflicted files (~68 hunks).
`dupcheck.py` matters: both sides carried the same MP1 commits (Echoes by cherry-pick), and a
"clean" text merge can apply such code twice (it did in `volfog.cpp`, also inside a WGSL string,
and in `command_processor.cpp`).

Resolutions:

- `lib/gx/command_processor.cpp`: rebuilt from the VR fork's file with
  `tools/primedgun/cp_rebuild.py` (anchored edits that fail loudly), adding Echoes' own work: the
  draw diagnostics ring, the no-render-pass early return in `push_gx_draw`, `MP2_GX_STORAGE_LOG`,
  the dstAlpha alpha-only second draw (its eye pipeline added for the stereo passes),
  `AURORA_RENDER_EVERY` in `draw_prim` and `DRAW_INDEXED`, and the subcommands 0x3B-0x3E (scan
  peek, EFB read-back, depth snapshot), also added to `aurora_subcommand_ids_unique()`. Echoes'
  cherry-picks the fork already has (array check, display-list chunk sources) were not re-applied.
  Echoes' trimming of partial triangle lists was not taken: the fork's path (MP1 upstream's draw
  code with the strip-merge work) draws the same whole triangles but batches differently, so the
  two Echoes tests in `tests/gx_dl_test.cpp` now assert the triangles drawn, not the batching.
- `lib/gfx/volfog.cpp`, `pipeline_cache.cpp`: the VR fork's file (Echoes' were older forms of the
  same MP1 code), with Echoes' `MP2_PIPELINE_THREADS` rename.
- `lib/gfx/probe.cpp`, `vfx.cpp`, `xray.cpp`, `tex_copy_conv.cpp`, `tests/tex_copy_conv_msaa_test.cpp`:
  MP1 upstream's newer Compatibility-device paths (the VR side).
- `lib/aurora.cpp`: Echoes' `render_this_frame` (`AURORA_RENDER_EVERY`) kept; a stereo frame is
  always rendered (`frameStereo.active || render_this_frame()`), since the headset's frame loop
  waits for every frame it hands out. The end-frame lambda captures both `renderFrame` and the
  stereo state.
- `lib/gfx/recording.cpp`: Echoes' uniform de-dup is off on an immersive frame (a multiview draw's
  eye clip block must follow its own mono uniform, so a shared range would only be pushed again);
  the push window is 2 x `MaxUniformSize` (VR). The final pass keeps its mono image when the CPU
  reads it back (`scanPeek`, `efbCpuCopy`, `forceDepthSnapshot`): `skipMono` would leave the scan
  peek, `PortEfbCopy` and the flare occlusion reading nothing.
- `lib/webgpu/gpu.cpp`: Echoes' feature loop with the VR interop and multiview features added;
  `options` is not const (the D3D12 adapter LUID is chained into it).
- `tests/gx_test_stubs.cpp`: the VR fork's staging hooks (`testing::mergeDraws`, `stagedVerts`,
  `stagedIndices`); Echoes' merged-index test uses them.
- Namespaces: Echoes' counters are `aurora::perf` (`perf.h`), the fork's stereo statistics are
  `aurora::gfx::perf`; Echoes' calls inside `aurora::gfx` (`recording.cpp`) are spelled
  `aurora::perf::` so they do not bind to the fork's.
- Environment: the VR names Echoes would prefix with `MP2_` are renamed (`MP2_FRAME_STATS`,
  `MP2_FOVEATION_LAYERS`, `MP2_NATIVE_VERTICES`, `MP2_MULTIVIEW_ADRENO6`); `MP_GX_CHECK_ARRAYS`
  stays as Echoes has it. Redo the renames on every re-merge (`grep -rn 'getenv("MP_' extern/aurora`).

Verification on this PC (Windows 11, RTX 3090, D3D12, VR not wired yet): the build is clean;
`ctest -L port` 32/32; aurora `gx_fifo_tests` 287/287, `gfx_recording_tests` 135/135,
`foveation_tests` 8/8, `gx_texture_cache_tests` 23/23, `render_worker_tests` 6/6. A new-game run
(`MP2_NEW_GAME=1 MP2_SKIP_FRONTEND=1 MP2_SKIP_CINEMATICS=1 MP2_SKIP_INTRO=1 MP2_FRAMES=1200`)
reaches gameplay at 4.8 s like the untouched port, and its screenshots at game updates 300, 700
and 1100 are pixel-identical to the untouched port's (`MP2_SHOT_AT`, `tools/primedgun/shot_ab.py`).
The first start drops the GX pipeline-cache rows of the old ShaderConfig layout once (epoch 3).

## Harness: `MP2_SHOT_AT` (2026-10-29)

`MP2_SHOT_AT=<update>[,<update>...]` takes the F12 capture at those game updates
(`platform/port_harness.cpp`). The console and its `screenshot` command are Linux-only, so this is
how an unattended Windows run is checked by picture. Aurora writes
`screenshots/metroid-prime-<stamp>.bmp` under the working directory and logs the path.

## Baseline (2026-10-29)

Upstream `a33e1c0d`, Windows 11, VS 2022 clang-cl 19.1.5, Ninja, CI flags
(`.github/workflows/windows.yml`) plus static OpenSSL 3.5.9: configure and full build in ~5 min,
`ctest -L port` 32/32. D3D12 on the RTX 3090. New game to gameplay at 4.8 s.
