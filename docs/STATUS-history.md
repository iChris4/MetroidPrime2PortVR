# Session status history

Older sessions moved out of docs/STATUS.md (newest first). Read only when tracing history.


<!-- Sections below through 'batches 12 and 13' moved verbatim from docs/STATUS.md on 2026-10-29 (newest first). -->

## 2026-10-29: 0.6.2 released

- Bumped `versionCode` 8 / `versionName` "0.6.2" and added the 0.6.2 metainfo release entry
  (`cb344643e`); release commit `37a61d2c2` (code-identical; only the Flatpak manifest differs).
- Flatpak moved to runtime `org.freedesktop.Platform` 25.08, which closes the 0.6.1 end-of-life gap. The 25.08 SDK
  ships SDL3 3.2.30, which Aurora's auto provider picked and which lacks `SDL_SavePNG`, so the
  manifest now forces `-DAURORA_SDL3_PROVIDER=vendor` (`37a61d2c2`). The local build
  (12,246,864 B) installed once: `flatpak info` reports `Version: 0.6.2`, runtime 25.08,
  `files/` holds only the binary, metainfo, desktop file, icons and licences, and it reaches the
  disc prompt (run with this shell's `XAUTHORITY` set; without it, "x11 not available").
  Test install removed.
- CI on `37a61d2c2`, all `success`: linux-release 40852272521, windows 40852276725, macos
  40852280313, ios 40852284307, android 40852288057.
- Crash soak (binary-identical to `37a61d2c2`): `build/s-release-2026-10-29b/soak/summary.tsv`,
  **96/96 rows `ok`**, exit 0. Log `/tmp/opencode/soak-062.log`.
- Artefact checks from `build/release-assets-0.6.2/out` (12 assets): APK `versionCode='8'
  versionName='0.6.2'` signed by `5c380a13…8bd20`; IPA and macOS `Info.plist` 0.6.2; macOS
  executable `-rwxr-xr-x` in the zip; x86_64 binary needs at most `GLIBC_2.34` and its Build ID
  matches the `.debug`; no game data in any zip, APK, IPA, tarball or the x86_64 AppImage; no
  `user/` folder in the Windows zips; the new "Use twin-stick controller" string is in the
  Linux, AppImage and Windows binaries.
- APK installed on the test phone (`25102PCBEG`) as an in-place update: versionName 0.6.2 /
  versionCode 8.
- Tag `v0.6.2` = `37a61d2c2`; GitHub release
  https://github.com/Odrannnn/MetroidPrime2Port/releases/tag/v0.6.2, published as a **normal
  (latest) release**, not a pre-release, so the update check's `/releases/latest` finds it (it was
  a 404 with only pre-releases; now answers `v0.6.2`). 12 assets, all `uploaded`. Notes from
  `build/release-notes-0.6.2.md`, with no link to the deleted PrimeDecomp/echoes repo.

## 2026-10-29: 0.6.1 released

- Bumped `versionCode` 7 / `versionName` "0.6.1" and added the 0.6.1 metainfo release entry;
  artefacts are built from that commit (`ad26e59e6`) by the release workflows.
- Tag `v0.6.1` = `ad26e59e6`; GitHub pre-release
  https://github.com/Odrannnn/MetroidPrime2Port/releases/tag/v0.6.1 with the twelve assets
  (`gh release view v0.6.1 --json assets` lists them all `uploaded`). CI: linux-release
  40791251657, windows 40791254843, macos 40791258032, ios 40791260907, android 40791264448,
  all `success` on `ad26e59e6`.
- Flatpak built locally from the same commit (12,058,096 B) and installed once for
  verification: `flatpak info` reports `Version: 0.6.1`, `files/` holds only the binary, the
  debug data and `share/{licenses,metainfo,applications,icons}`, and it starts and reaches the
  disc prompt. Test install removed afterwards.
- Crash soak on the release commit: `build/s-release-2026-10-29/soak/summary.tsv`, **96/96 rows
  `ok`**, exit 0 (mods/none × validate/release × MSAA 1/4 × the 12-area tour, 117 s per
  (mode, toggles) game). Log `/tmp/opencode/soak-061.log`.
- Artefact checks from `build/release-assets-0.6.1/out`: APK `versionCode='7'
  versionName='0.6.1'` signed by `5c380a13…8bd20`; IPA and macOS `Info.plist`
  `CFBundleShortVersionString 0.6.1`; macOS executable kept `-rwxr-xr-x` in the zip; x86_64
  binary needs at most `GLIBC_2.34`; no `.dol`/`.rel`/`.pak`/disc image in the tarballs, either
  AppImage, the macOS zip, the Flatpak or the APK (its `assets/` is licences plus textures). No
  version string exists in the Linux/Windows binaries, so the #4 fix was confirmed in them by
  the new `Scan Peek Resolve` label (4 hits in each; 0 in the 0.6.0 Windows exe), and their
  provenance is the CI runs above on `ad26e59e6`.
- Remaining known gap: the Flatpak runtime `org.freedesktop.Platform` 24.08 is now reported
  end-of-life by flatpak; moving the manifest to a supported runtime is the next release task.
- Release APK built locally with the project key (signer SHA-256 still
  `5c380a13...8bd20`, so it updates over 0.6.0 without uninstalling) and installed on the
  test phone (`25102PCBEG`, Android 16) as an in-place update: versionName 0.6.1 confirmed,
  save and settings kept.
- On-device check of issue #4: the game resumed in the scan visor with `msaa=4`, read-back
  logs showed six distinct EFB scan ids (28811/59536 non-zero pixels), and a logbook scan
  completed. Screenshots `/tmp/opencode/phone-0.6.1-launch.png` (mid-scan) and
  `phone-0.6.1-scan2.png` (after).

## 2026-10-29: issue #3 emulator save framing

### Trilogy mirror integration test

Tested the actual `CSaveGameScreen::Update` mirror hook with
`emulator_folder=/tmp/opencode/issue3-mirror-{out,in}/emulator`,
`share_emulator_card=1` and `mirror_trilogy_save=1` in isolated settings files.
Native front-end runs used `MP2_SKIP_FRONTEND=0`, 5500 updates, 8 ticks/loop and
40-update A presses at updates 100, 300, 600, 1000, then every 500 through 4000.
Three-update presses can be skipped entirely at 8 ticks/loop; those initial runs
never reached the save screen and were not counted as mirror verification.

- Outbound: the actual native save-station G2ME card (0%, 2.4 seconds) was newer
  than a copy of `13-gbatemp-100.bin`. The mirror replaced Echoes file 1, made an
  exact `save.bin.bak`, and equalized the card/bin mtimes. Independent byte comparison
  found 813 changed bytes, all within the target Echoes parts, its summary and the
  global CRC. The other slots and the Prime 1/Prime 3 data were unchanged.
- Inbound: the same unmodified Trilogy fixture was newer than another copy of the
  G2ME card. The mirror updated the card, kept an exact pre-import GCI backup, left
  the NAND file unchanged and equalized mtimes. A subsequent native boot loaded
  the imported 100% / 06:13 Echoes progress into gameplay at update 273, exit 0.
- Dolphin 2512 booted the USA Trilogy disc with the actual outbound mirror output.
  Its game-selection screen showed Echoes in Temple Grounds at 0% / 00:00, alongside
  the unchanged Prime 1 and Prime 3 progress. Selecting Echoes reached first-person
  gameplay in Temple Grounds (99 energy, 5 missiles). Screenshot:
  `/tmp/opencode/issue3-trilogy-mirror-loaded.png`. The mirrored Echoes parts remained
  byte-identical through the menu. Dolphin was stopped after the successful load.

The initial emulator setup seeded only `save.bin`, omitting Wii's `banner.bin`.
Both the mirrored output **and the untouched source fixture** were then rejected as
corrupt. A clean Trilogy boot generated both files; its empty `save.bin` was exactly
`01-firstboot.bin`. Adding that game-generated banner to the isolated test NAND
resolved the rejection. It was a test-setup failure, not evidence against the mirror.
The mirror expects a complete Trilogy save directory already created by the game.

Evidence: `/tmp/opencode/issue3-mirror-{out,in}/`,
`/tmp/opencode/log_issue3-mirror-import-load.log`,
`/tmp/opencode/issue3-trilogy-games.png`. This tests USA G2ME card <-> USA Trilogy
NAND sharing, not PAL conversion.

**Trilogy-disc failure reproduced, then fixed:** created a real
native `01-R3ME-MetroidPrime2.gci` by mounting the Trilogy disc in the port, starting
a new game and saving at the station. Re-entering the native front end with sharing
and mirroring enabled logged `Trilogy mirror: import failed: there is no Echoes save
on the card to put the Trilogy file into: save once in the port first`. Both files
remained unchanged. The old `GameFilesNewest()` filtered headers by the first three bytes
`G2M`, so the existing R3ME file is ignored before conversion. The payload converter
does support R3ME/R3MP; the wrapper's file selection does not. Reproduction:
`/tmp/opencode/log_issue3-mirror-r3-enabled.log`, isolated data
`/tmp/opencode/issue3-mirror-r3/`. That test-only checkpoint was superseded by the fix below.

### R3ME mirror fix

`GameFilesNewest()` now selects the mounted release's exact four-byte game code and
maker `01`, through `port_save_mirror.h::IsCardForRelease`. Simply admitting all R3M
files would still choose the wrong layout when a newer GameCube card shares the same
region folder. Both sync direction detection and the actual import/export now use
the mounted release only. G2MP conversion remains explicitly unsupported, as before.

Verification:

- `port_save_mirror_tests` covers the full G2ME/G2MP/R3ME/R3MP selection matrix,
  wrong makers, unrelated games, malformed codes and truncated headers. It and the
  framing/conversion tests passed through CTest; the selector also passed ASan/UBSan.
- Actual front-end mirror tests on native Trilogy data passed in both directions
  under `/tmp/opencode/issue3-r3-fixed-{out,in}/`, with a **newer G2ME decoy** in the
  same card folder. The decoy stayed byte-identical; selected card/bin mtimes became
  equal. Export touched only the target Echoes slot, summary and CRC, and kept an
  exact NAND backup. Import kept an exact R3ME backup and did not change the NAND.
- The imported R3ME card loaded 100% / 06:13 progress into native gameplay at update
  273, exit 0 (`/tmp/opencode/log_issue3-r3-fixed-load.log`).
- Dolphin 2512 loaded the actual R3ME mirror export into retail Trilogy gameplay
  in Temple Grounds (99 energy, 5 missiles). The Echoes payload remained identical
  to the exported file. Screenshot: `/tmp/opencode/issue3-r3-mirror-loaded.png`;
  isolated emulator: `/tmp/opencode/issue3-r3-dolphin/`.
- G2ME mirror import and export also passed with a newer R3ME decoy left unchanged
  (`/tmp/opencode/issue3-g2-regression-{out,in}/`).
- Full native game link and source manifest check passed; the Trilogy fixture suite
  still reports 0 failures. Retail matching artifacts remain unchanged (87 hashes pass).
  PAL selection is unit-tested; no actual R3MP disc test was performed.

### GameCube framing fix

`fix/issue3-save` corrects the runtime card reader/writer, not just the import inspector.
`CCardFileInfo` now checks CRCs in console big-endian or legacy host-little-endian order,
reads the generation in the matching order, and always writes new CRC/generation words
big-endian. Payload bytes are not swapped. Generations other than 0/1 are rejected before
the slot selector uses their XOR as an index. All upstream changes are `TARGET_PC`-guarded.
Reading a valid legacy header does not rewrite it; a normal save migrates the data copies
as they are written. Retail repairs an old header's non-console CRC via its existing
header-repair path. Keep backups and never run both programs on the shared card at once.

Verification (isolated cards under `/tmp/opencode/issue3-*`; no live saves changed):

- `port_save_framing_tests` and the real-fixture `port_save_convert_tests` passed through CTest.
  Framing tests use an independent bitwise CRC/console decoder and cover both byte orders,
  both generations, unchanged payloads, legacy migration, repeated writes, CRC corruption,
  invalid generations, short buffers and a byte-palindromic CRC. ASan/UBSan passed too.
- Full native game linked. Headless native boots loaded both byte orders of the same real
  G2ME fixture into gameplay (frame 121), rather than falling back to an empty card.
- The console driver sent `station` at update 140, then wall-clock `press a` / `stick 0 0`
  at 1/1.2, 2/2.2 and 3/3.2 seconds. An actual in-game save changed both data copies;
  Python `zlib.crc32(...) ^ 0xffffffff` independently verified their big-endian CRCs.
  The existing legacy header was unchanged, as intended. A subsequent native boot
  accepted that saved card into the running world and exited normally at update 180.
- Trilogy container/payload tests passed on the local fixture set (0 failures).
- Matching build: `87 files OK`; generated source manifest check passed.
- Actual emulator test: Dolphin 2512 (`/usr/games/dolphin-emu-nogui`, JIT64/OpenGL)
  booted the owned G2ME01 disc using an isolated `-u /tmp/opencode/issue3-dolphin`
  folder and a copy of the file produced by the native save-station test above.
  Selecting Single Player loaded Temple Grounds into first-person gameplay; screenshot
  `/tmp/opencode/issue3-dolphin-loaded.png` (cropped from `screen-30.png`). Dolphin
  repaired the legacy header CRC to big-endian, but both data copies (GCI bytes
  `0x2040..end`) remained byte-identical to the port-written source. This was not a
  deleted/recreated card. The emulator ran on a dedicated Xvfb display and was stopped;
  the normal emulator user directory was not used.

Native integration, console-format validation and an actual Dolphin load passed.
Steam Deck and PrimeHack specifically remain untested. No GitHub comment, push or release was made.

## 2026-10-29: issue #4 scan readback with anti-aliasing

- `scan_peek::encode` previously dropped every multisampled EFB request. The encoder now
  supplies the original colour attachment view, and a scan-only render pass reads sample zero
  from both colour and depth into single-sample RGBA8/R32Float region targets. It does not
  average integer scan IDs or change the game's EFB. Recording preserves the source stores
  even when the following pass clears the EFB.
- Core-device 1x readback uses the same conversion: validation-enabled testing exposed an
  existing invalid partial depth texture-to-buffer copy. It also normalizes HDR colour to the
  four-byte readback layout. The Compatibility-device fallback is unchanged.
- `ScanPeekMsaaTest` renders per-sample IDs/depths and a spatial pattern on Vulkan, then checks
  actual CPU output (not just pipeline validation): 1x/4x, BGRA, HDR, clipped regions, slot reuse,
  AA toggling and attachment-store retention. The old MSAA early return was temporarily restored
  as a negative control: 4x fails while the 1x control passes. No negative-control code remains.
- Graphics suite: 112 tests pass using lavapipe with validation enabled; native game links;
  matching build: `87 files OK`. Evidence: `/tmp/opencode/issue4-all-gfx-tests.log`,
  `issue4-negative-tests.log`, `issue4-matching.log`.
- Native rendered integration check on RX 7900 XTX/RADV: Dark Trooper `e000b000c` in Temple
  Grounds area 11 completed its 1.50-second scan at 4x, after combat/scan visor re-entry, at
  1x, and after switching back to 4x. Each check reacquired with ordinary held L, not
  `PortForceOrbit`. The initial console `scan` pair was only camera positioning. The harness
  boot was stuck morphed; a temporary console setup forced `SetMorphBallState(Unmorphed, 0)`
  and first-person camera once before positioning. That setup was removed and the final native
  binary rebuilt. Logs: `/tmp/opencode/issue4-game4.log`, `issue4-game-results4.log`; screenshots:
  `/tmp/opencode/issue4-scan-aa-{1,2}.png`. No Windows, Android or reporter-specific driver test.

## 2026-10-29: scan visor widescreen registration and model proportions

- Reproduced the reported detached background with a completed Dark Trooper logbook scan
  (`e000b000c`, Temple Grounds area 11), at a 2000x781 window and `scanspread 100`.
  Generic non-logbook scans do not exercise the hierarchy panels.
- `basewidget_crazyleft/right` contain only the background; the six category rows are sibling
  trees. Register both the background and rows for the live scan-spread weight, and give every
  piece of each side the same label anchor. The BWIG roots themselves are at the origin and
  cannot serve as side anchors. `CGuiFrame::SetSpreadAnchorTree` now accepts an optional anchor;
  existing one-argument callers retain their original behavior.
- The model preview bypassed `CGuiCamera::Draw` and used its authored aspect unchanged. Apply
  the GUI's aspect widening, fit-width FOV and HUD scaling to its custom projection too: wide
  windows no longer flatten the scanned model.
- Verified on Linux/Xvfb with an actual completed logbook entry: spread 0/50/100 at 2000x781,
  plus 1280x720, 1680x720, 960x720, 720x960, and HUD scale 70%. Evidence is under
  `/tmp/opencode/scanfix/` (`logbook-before-100.png`, `logbook-before-0.png`, `final-0.png`
  through `final-8.png`, `checked-matrix.png`). Test-only forced unmorph setup and widget
  probes were removed; neither is part of the fix. Native game links; matching build passes
  all 87 checks. Windows/Android device rendering has not been rerun.

## 2026-10-28: issue #2 controller bindings

The twin-stick-only camera path omitted `PortMouse::StickEnabled()` when applying aim
pitch in `CFirstPersonCamera::UpdateTransform`. Include it alongside mouse and touch:
the player already consumes both stick axes and turns the body for yaw, but native
free-look is suppressed in twin-stick mode, leaving no path to apply pitch otherwise.
Enabling mouse aim alongside twin-stick was a code-derived workaround, not a device-tested one.

The menu and trigger reports are fixed too:

- Separate menu buttons: `platform/input/port_input_devices.cpp::MenuButtonsActive`
  previously required `!PortTouch::ControlsEnabled()`. `platform/port_touch.cpp::Settings::controls`
  defaults to true on desktop too (only `aim` has a mobile-specific default). Therefore
  `ApplyMenuButtons` removed all Menu actions despite the separate-menu setting being on.
  It now uses last-used touch activity, as MP1 does: `NoteDevice` activates touch,
  real controller/keyboard/mouse input deactivates it; virtual pads, stick drift and
  synthetic touch/pen mouse events do not. Builds without touch always report inactive.
  The context path recognizes paused menus; map and scan-text contexts need a separate
  completeness review. (Superseded 2026-10-29: the front end was *not* recognized, as reported
  against 0.6.1. The title and file select run as an in-game CMFGame in the 'FRND' world, so
  `GameContexts` returned gameplay there. Its `PlayerInPlay` now excludes 'FRND', as
  `port_touch.cpp`'s copy already did, so the main menu reads the Menu rows. Code-traced and
  linked with the port tests passing; not run on a Steam Deck.)
- Trigger capture: Aurora's `PADGetRawState` reports an analog trigger axis and synthetic
  half-pull/click buttons. The recorder previously enumerated both the buttons and
  axes, retaining every distinct input until release (`UpdateCapture`).
  Thus one right-trigger pull recorded `RT + RT click + RT`, exactly as reported.
  Capture now skips the four synthetic trigger-button aliases and records the positive
  analog axis alone. Runtime pull/click support and manual bindings are unchanged;
  chords involving distinct controls still record normally.

These findings come from source tracing against v0.6.0 and the port branch, not a Steam Deck run.
Verification: game linked; `port_radial_mouse_tests`, `port_input_engine_tests` and
`port_input_bindings_tests` passed (these cover the aim/input machinery, not the camera
integration); matching build reported `87 files OK`; generated source manifest check passed.
`port_controller_regressions` additionally passes touch/physical transitions, desktop/F1 gates,
virtual-pad/drift/synthetic-mouse filtering, full-trigger capture across thresholds, both
triggers together, shoulder+trigger chords, ordinary stick axes and keyboard/mouse inputs.

## 2026-10-29: issue #2 diagonal movement/jumps

Input normalization did not fix the force law: each horizontal axis independently added
the retail friction-compensation offset and independently saturated its force. A diagonal
could therefore receive two full forces even with unit-length input. Direct-aim movement
now evaluates the forward law once along the input direction, with planar damping/speed
limiting instead of component damping and a forward-only cap. Strafe gets the same 0.8
full-travel scaling as forward. Jump and double-jump acceleration use the same planar
movement magnitude rather than only the forward/back component. These changes are PC-only
and confined to unlocked direct aim; native controls and locked-on dash code are unchanged.

Verified: game linked; four targeted input/aim tests passed; matching build `87 files OK`;
generated source manifest check passed. The mouse test reproduces the old diagonal force
boost and checks the production force/damping helpers across sixteen headings, three stick
magnitudes, ground friction and air drag over 240 ticks each.

Headless native-game check: `tools/drivers/direct_movement.con` with
`tools/drivers/direct_movement.input`, a fresh user directory, `MP2_NEW_GAME=1`,
`WORLD=0b_sand_hall`, `MP2_MOUSE_AIM=1`, `MP2_FRAME_LIMIT=0`,
`MP2_TICKS_PER_FRAME=1`, `FRAMES=740` (RUNNING.md's console driver recipe).
The fixture changes facing so all three input directions jump along the same world-space
path from the same grounded start. At +15 ticks the horizontal distances were 1.001,
1.001 and 1.003 units, with the same sampled height (21.041, from 19.399); at +30,
1.423, 1.423 and 1.427 units. The diagonal boost is gone. Later samples hit corridor
geometry, so they are not an unobstructed landing-range measurement. An initial attempt
teleported above the room and never actually jumped; that run is not jump verification.
Device-specific Steam Deck and Android testing remains unperformed. Nothing posted on GitHub.

## 2026-10-26 (port orchestrator): widescreen gaps, ball glow, suit colours, FOV, scan copies

Widescreen, MP1's way. `82b768ae8` (fix/wide2) closes the 4:3 gaps MP1 had already fixed: splash
logos keep their 4:3 shape and the face reflection's ortho widens with the view (`4f84326e0`, MP1
`f5796fc2e`); movies fit the view with their shape kept (`b0bdb0627`, MP1 `10d237a43 6b7384a85`);
credits text is laid out in the movie's 4:3 width (`511ba7bfe`, MP1 `cfa8e060b`); the scan window
keeps its share of the width below 4:3 (`cdeb6495b`, MP1 `629170f37`); a cinematic cut blends from
its unwidened FOV (`0a01d41a7`, MP1 `227c7fdaa`); the arm cannon is drawn at retail FOV, anchored to
the 4:3 bottom below 4:3 (`7ebd852b8`, MP1 `5fb236a91 1a83a296d af5e42ade`). `7b82b73c9` widens the
portal warp/world transition and keeps retail's cinematic framing instead of cropping it; `fcd80eb77`
keeps that width below 4:3 (Vert+). `a3dbfec15` (fix/wide) makes the pause and map frames fill a wide
window (`CGuiFrame::SetSpreadPanel`: the window grows, the side panel slides, text keeps its shape,
`MapScreenWidth` follows the spread).

Game-side fixes. `66ca362a3`: the FOV option's Retail entry restores the game's 55 immediately
instead of waiting for a slider move. `4cc49dc49` (merge `68d3a77a4`): the morph ball's model shadow
passes the actor-light array slot, so the ball is shaded again instead of flat bright (GOTCHAS, "A
model shadow's light mask"). `a5d3587f6` (fix/suit1): suit colours apply live while paused — Apply
now/reloadmods are serviced in the pause loop and the pause Inventory's Varia model is recoloured and
refreshed — plus the Light Suit main slot, kVersion 4, cannon textures reverted (`689a5fe4e`).
`c1dfbb37b` (fix/scanbands): scaled A8/R8/G8/B8 EFB copies sample Nearest, so the scan visor's index
map is no longer averaged into wrong colours; I8 copies stay filtered, being luminance images not
indices (`cba2b0837`).

Phone verification pending: none of the above has been run on Android (touch layouts at 16:9/21:9, the
pause Inventory recolor and the ball shadow there).

## 2026-10-25 (port orchestrator): batch34, last behaviour rows

Integration `integ/batch34` (`../wt-mp2-batch34`, from port 92bcb4775) merged kral2/kral2b, oct3, spa3/spa3b/spa3c,
fish2/fish2b and wisp3/wisp3b: FishCloud 56, Kralee 18, OctapedeSegment 24, ScriptPlayerActor 33 and WispTentacle 35
behaviour rows all marked; progress 5880 markers, behaviour 5491/5491 (100%). An astra (`openai/gpt-6-astra#xhigh`)
fresh-context review of every new marker found three bad proofs, all reworked: spa3 C30 count rewrite (now a real
count-3 RenderShadowBuffer with the beam model aliased as grapple), fish2 Touch 740/750 (real normalization-false
arms, now forced via `fishcloud_touch_norm_force.py`), kral2 244C (block3->5 initiallyInvisible ternary, proven by
`tools/scan_kralee_props.py`). Gates: build-port-link links, 87 files OK, gen_files_cmake --check, smoke59b vs
smoke58d no regressions. Evidence in `../evidence-archive/{kral2,oct3,spa3,fish2,wisp3}`.
Caveats: many forced arms have no natural trigger (no shipped grapple model for ScriptPlayerActor, WispTentacle
last-touched player/bomb); WallCrawler/Ripper/ScriptCoin still never spawned; H/T rows (copies 2056/4743) don't count.

## 2026-10-20 (port orchestrator): lanes in flight

Read a lane's result with the openchamber tool (`session.messages`, `lastAssistant`), review with
`tools/review_decomp_diff.sh <base>` from its worktree, accept, then send the same session its next slice.
Worktrees: `~/.local/share/opencode/worktree/3e7f972e5f2a67a4bf166b19e99a591913d8d4a4/wt-mp2-*`.

**Next-slice preamble (2026-10-21; send with each lane's next slice until all have had it):** first
`git merge port` (brings `8721dda7e`); fast runs `MP2_FRAME_LIMIT=0 MP2_TICKS_PER_FRAME=8` with
`tools/drivers/console.py` `@f<n>` scripts; reach states with console `fsm <id> <state>` / `place <id> player [d]`
/ `damage` / `send` (forced arms count when the branch table says `forced (<cmd>)`, see
DECOMP_WORKFLOW.md); put drivers in `tools/drivers/`; ask for missing console commands and shared-header
accessors in the report; also write the final report to `/tmp/opencode/reports/<lane>.md`.
Batch 7 (branch `batch7` in `../wt-mp2-batch5`, head `07d7159e1`, 10 heads from the table; host/link/matching
green 12:13) merged into port `54e8498b0`: 1785 markers, 15.21% fns / 20.13% bytes. smoke7 (`/tmp/opencode/smoke7`)
vs smoke6: 0 new errors, 17/17 rc 0; sandboss stalled once at frame 240 (rc 124, blank frame), passed solo: watch it.
Parallel smoke runner (3-4 Xvfb displays + ticks-per-frame + `@f`) next, checked against smoke7.
Orchestrator queue: grapple points never become the orbit target (CScriptGrapplePoint.cpp:68 only kMT_Orbit vs
CPlayerOrbit.cpp:1004/1070 kMT_Target only; SwampBoss-stage2.md §7.17), a shared-code bug to fix on port.
Lane watcher: `~/.cache/mp2-orch/watch.sh` (lanes file beside it), detached loop (`loop.sh`) logs events to `events.log` there.

**Accepted since batch 7 (batch 8 candidates, 2026-10-21 14:15):** CommandoPirate `ed504da00` + `24fca504f`
(2474/7A98 ported unmarked), DarkSamus `482a38568`+`4154642ea`+`5d31354a9` + darksamus2 `c9eb05d69` (1B658 138A4 1382C 13750 1367C), SwampBoss2 `1fb9cf495` + `26715ee9c`
(55B0), SwampBoss1 `46aeee651` + `23a9594eb` (belly cap fix, 35DC 3320 348C), EmperorIngStage1 `6386fff8f` + `527356dac` (tentacle anim search order),
Ing `329c5a27c`..`15b1c9b33` + orchestrator `9f7134a47` + `04abccad1` (B8C0 BCB4; aim form-9 fix) (fn_29_4604's three missing steps; the lane twice
claimed them ported without the code), DigitalGuardian `6c6eae0c7` (incl. shared fix `e403a065e`
DolphinCModel UnlockTextures), Splitter `601ceaac7`, bg2 `6c8bfd383` + `396e8ec01` (module done: only copies left unmarked), ep2 `ef9806f4a`,
CommandoPirate `d72c5d88a` (961C ported, 2474 time fix), SwampBoss2 `83d44d889` (5830 50AC 527C). New lane: Sandworm
(`../wt-mp2-sandworm`, classification first). Console `fsm` sub-machines + `damage ... powerbomb` merged `d272a3f96`; Ing lane next: `damage <ing> 100 powerbomb` for fn_29_B3C4's burning arm.
**Lane sessions were deleted from the DB (14:05)**: fresh lanes spawned as subagents of this orchestrator; ids in
`~/.cache/mp2-orch/lanes` (the table's session column is stale). Surviving old sessions: Splitter (re-sent),
EmperorIng1 (busy), DarkSamus (busy, old orchestrator's child). Shared asks queued for port: ELoopAttackType 5/6
+ CBCLoopAttackCmd skip-into setter (CommandoPirate), `;`-separated console commands per poll (Splitter),
fluid plane `Burn` on patterned actors (Ing fn_29_B3C4 burning arm). Grapple/orbit: not a bug (L pressed before the 20-update candidate rebuild); console `grapple <id> [d]` merged `cc780b874`.

Batch 5 merged into port `3bb8177db` (via branch `batch5`, worktree `../wt-mp2-batch5`): playerturret
`c377344ed`, ing `8855b1674`, spacepirate-vt `c42d82fce`, emperoring `542d6c9b8`, emperoring3 `f786b983e`,
boostguardian `da45064c5`, digitalguardian `227e42fb1`, commandopirate `cf9eb1995`, splitter `b17910bb4`
(+ `e4aead7b6` drops DG's duplicate `CCollisionActor::SetExtendedTouchBounds`). Host/link/matching
green, smoke5 17/17 clean (`/tmp/opencode/smoke5/`), 1159 markers. Upstream build must run before the
port build in a fresh worktree (writes `build/G2ME01/include/*.inc`; needs `extern/musyx` checked out).

Batch 6 (2026-10-21, merged into port as `3b2effc06`, 1677 markers): merged emperoring3
`43eb8312e`, emperoring `5a594830e`, boostguardian `673fbcb56`, minoring `c8d342614`, spacepirate-vt
`33291f63c` (conflict in PortCSpacePirateStates.cpp: kept port's WarpIn fix `4d6c4d036`, dropped the
stale fn_72_7078 marker), wallcrawler2 `036ca13db`, commandopirate `6ea58f47a`, digitalguardian
`c56aff752`, splitter `29e4bf6aa`, darksamus2 `f0de7edab`. Matching 87 OK + sha1 OK, host + link green
(07:05). smoke6 17/17 clean (rc 0, 0 caught, 0 reach-stub; stand-ins down in boostg, digig, emp1,
emp3, ep). Batch 7 = lane commits after these bases, merged by hash.

Older list (superseded by batch 6):
spacepirate-vt `33291f63c` (8 combat marks); wallcrawler2 `036ca13db` (obstruction scan, 3 marks);
emperoring `bd4c2c25a` (7EA8, 7F8C cases, 1D38, Render 531C); splitter `b1f682ff7` (1188/58E8, F1C0,
654C, 1790, AC68, 8D50, AAC4); commandopirate `843ebf292` (4D84); boostguardian `999b51bdf` (D4F8 events,
C1B0/C05C, C850, puddle vulnerability 0x1178); digitalguardian `8826277c0` (8090, 7D8C, 5910, 5384,
7CB4, 5274/266C/2E60, 5838 marked); emperoring3 `e3971fcd3` (6904/67C0 beams, full-TUniqueId fix).

Port fix 2026-10-20 (scan-visor lag, after the OOM fix `1f1c5c9a3`): `DrawScanVisor` builds a new
CGraphicsPalette (new TLUT id) every frame, so Aurora made a render-scale palette texture every frame;
`resolve_dynamic_palette_texture` now takes over an older identity's texture for the same copy (desktop:
1 alloc on entry, then 0/120 frames). The two scan-visor EFB CPU read-backs (full render-scale frame
each, every frame) are dropped: Aurora draws both from the copy textures (A/B identical; only
`MP2_NO_COPY_TEX=1` uses the bytes). Desktop-verified; tablet not yet tried (ask before installing).

| lane | session | branch @ base | task |
|---|---|---|---|
| BoostGuardian (Opus) | — (done) | boostguardian @ `673fbcb56` (204/318; 103 cited, 2 folded; loaders hand-checked, isEchoEmitter + 19 sound ids -1 defaults; 12AB0 compares correct) | merge in batch 6; 9 fns between tsv 309 and 318 unexplained |
| EmperorIng3 (Opus) | — (done) | emperoring3 @ `43eb8312e` (182/270; 88 cited, 0 todo; matching build OK) | module complete: merge in batch 6 |
| DigitalGuardian (Opus) | `ses_ea312bc93ffdb6liLXfqe2O49p` | sandboss @ `f4626cd11` (accepted for batch 7: port-only CVisualKnowledge (DOL 0x801F55F0..; shared files.cmake/gen_files_cmake.py line — orchestrator merges); A138/9E88 ported; 10/10C cited as DOL copies (unmarked); marked 17C98 171F8 1524C) | unmorph SIGSEGV CPlayer::Render→CTexture::Load; long fight → 14470, IsDead/IsIdle, 15284, A138/9E88 arms |
| Splitter (Opus) | `ses_ea312bc7fffemfIq7YQeApP3b6` | elitepirate @ `fcf5b7329` (accepted for batch 7: head undocks on chassis Death (EB04 clears 0xC6C; chassis dies only via A0A0 cannonball damage in LaserSweep); ported EB04 5D50 A0A0 42F8; marked 5AD8; boost = B) | undocked head 2994, PathFind 4E90, 3ADC 3D20 2280 565C 570C; HeadExplosion BEEC; real cannonball kill; beam render later |
| CommandoPirate (worker) | `ses_ea2c75221ffeCKO3r32cSZZVdR` | commandopirate @ `a0f626797` (0 markers; accepted for batch 7: CKnockBackMgr accessor in TARGET_PC (87 OK), ShieldCharge 8028 ported, branch table; charge band 10-20 check at 0xB370 failed) | drive band 10-20 → full shield cycle; port 7A98 + 2048 null-test skeleton |
| SpacePirate (PARKED) | `ses_ea3206eb3ffeIXL6w8JDRAddQp` | spacepirate @ `8a5800003` (84/262; accepted for batch 7: WarpIn summary corrected, case-0 materials right (0x7374 vs Deactivate 0x7528), 7078 a/b/c/d table; open: GRNT/FOLW and blocked-box arms) | parked: remaining bodies gated by shipped data |
| SwampBossStage1 (worker) | `ses_ea3206eafffeTqAzGkbdyimVXR` | swampboss @ `b209619df` (114/251; accepted: arena chain mapped (LAYER3 warp pre-loads → no Arrived; recipe send u044d Open + u0476 SetToZero); charge = A, 19.9 dmg; live-aim driver 37/330 hits, belly 21.58/50) | xd10 decay check; better hit rate → ExposeBelly; mark 35DC 3320 348C 5784 9A44 |
| SwampBossStage2 (worker) | `ses_ea2c7e7d2ffejX82iejJ2M25fe` | swampboss-s2 @ `475d1886b` (accepted: 5030/527C match but never run (anim event 0x1b never fires); give grappleBeam works, lock-on won't take the grapple point; Blow stun-in-window fires but window not caught) | port 50AC recursion + 55B0; why event 0x1b never fires; grapple lock-on condition |
| EmperorIngStage1 (worker) | `ses_ea24d8402ffe9XMGEDPkZNQs0q` | emp1b (decomp/emperoring1b) @ `1c654bc31` (91/241) | classify the 8 unexplained fns; 4954 FSM1 code-node dispatch gap; next 5 with branch tables |
| IngBoostBallGuardian (worker) | `ses_ea21cc6d0ffec8C8RnDduH1WKJ` | bg2 (decomp/boostguardian2) @ `1c654bc31` (204/318) | classify unmarked fns (host copies vs behaviour); next 5 with branch tables |
| ElitePirate (worker) | `ses_ea2119ceeffe01muX6hLQf0aFV` | ep2 (decomp/elitepirate2) @ `1c654bc31` (104/241) | classify unmarked fns; next 5 with branch tables |
| WallCrawler (worker) | — (done) | wallcrawler @ `d156dd4b3` (25/57, 32 cited; 1310 constrained arm ran: PillBug planarConstraint 1/2 in Agon 0b/0d_sand_hall; constraint 3 has no shipped instance) | module complete: merge in batch 7 |
| ScriptPlayerTurret (worker) | — (done) | playerturret @ `f679121be` (16/59 marked, 40 cited; 84C unmarked: split-screen MP unported in the harness; 1BE8 unmarked: kSM_Stop never sent) | module complete: merge in batch 7 |
| Ing (worker) | `ses_ea2a5b996ffep8QSEsdcQ7an9k` | ingfam (decomp/ing2) @ `67b20dbfb` (129/274; accepted: A.21 branch tables for 45BC 4604 426C 3948; area-57 Ings stay ing spots (StartAsIngSpot) in 9 runs) | diagnose ing-spot wake-up triggers vs asm; kill corporeal Ing → B3C4; Agon grapple |
| EmperorIng1 (worker) | — (done) | emperoring @ `5a594830e` (91/241; 140 cited; 4954 needs FSM1 code-node dispatch, 7EA8 no tentacle sound event; 8 fns between survey 233 and port_progress 241 unexplained) | merge in batch 6 |
| MinorIng (worker) | — (done) | minoring @ `c8d342614` (128/217; 28 dead, rest cited; 204C query-success arm never ran: path query returns kR_NoSourcePoint in area 13, check it is not a port bug) | merge in batch 6 |
| Inglet fix (claude) | `ses_ea4b8efdcffeJYrVuvmgGX4q6a` | merged to port as `2294724f1` (CFoldySurface byte-swap) + `67f337a57` (Aurora dstAlpha alpha pass) | done; the Aurora pass touches every blended dstAlpha draw: include in the next smoke. Puddles are unscannable in retail too |
| DarkSamus (Opus) | `ses_ea2935cc8ffeCvp1QjKlpVMOxG` | darksamus2 @ `ef73b3963` (62 markers; accepted for batch 7: DiveAttack 11FA4 + helpers, BreakDiveAttack 11C54, 100DC/FF20 ported; marked E910 12B00 12E90 12EF8 18628 18FCC 11C54; dive recipe: damage u0732 570 → final stage) | IsPlayerAimLineClear always true; stand-ins MissileJump 177BC, MeleeAttack 17BC0, BoostBallInto B164, SweepBeam CA0C, 1B658, 138A4; 11FA4 open arms |

## 2026-10-20 (thirtieth session): the scan visor's highlights flashed - one shared copy destination

The report: "the scan visor overlay colours still are not perfectly right"; asked which part, "highlights
on scannables"; asked how, "they flash".

- **What flashed.** The visor's palette-index map (an A8 viewport copy sampled as a C8 texture through
  the scan palette) alternated between the real indices and the *scan window pane's* RGB565 bytes, so
  the highlight tint on scannables changed from frame to frame. Measured with a new
  `MP2_DEBUG_SCAN_PALETTE=1` check - every populated index is `paletteIndex << 2`, so any index that is
  not a multiple of four is foreign data: **~34,500 of 356,608 bytes on roughly half the frames** while
  the visor was up; 0 on the others.
- **Three causes, each fixed:**
  1. `PortEfbCopy`'s staging slots were keyed by the destination pointer alone and had their geometry
     overwritten by every `Request`, so the visor's `Fill` could consume the pane's completed read-back
     (two copies into the same buffer per frame: `796x448 tx=1` and `208x92 tx=2`, both logged).
     A slot is now the whole copy identity - shared pointer, size, texel size - with its own staging
     buffer, and `Fill` takes the texel size too (from the destination format in
     `LoadDolphinSpareTexture`).
  2. **Aurora resolves a copy into a texture keyed by the destination pointer.** Both consumers copied
     into `CGraphics::GetDolphinSpareBuffer()`, so whichever copy was recorded last owned the resolved
     texture for both - the reason the pane showed the visor's indices as noise and vice versa.
     `PortEfbCopy::Destination(shared, w, h, texelBytes)` now hands each consumer a stable private
     destination: the scan visor's A8 viewport copy (`CCubeRenderer::DrawScanVisor`) and the window
     pane's RGB565 copy (`CPlayerVisor::DrawScanEffect`) `GXCopyTex` into it, `Request`/`Fill` it, and
     bind their texture from it.
  3. A first attempt bound the texture to a *shadow copy* while the copy still landed in the shared
     buffer; Aurora then read the linear guest bytes as tiled C8 and the overlay became diagonal
     stripes (`/tmp/opencode/final_22.png`). That is why the fix is a per-copy *destination*, not a
     shadow of the bytes.
- **Verified on the desktop** (`Xvfb :86`, `tools/run_gameplay.sh`, `MP2_INPUT=LEFT@600+4`, plus a
  turning-camera and a holding-`L` run): `bad=0` on every logged frame, no dropped slots, no torn
  reads; screenshots show the pane's magnified scene clean (`/tmp/opencode/vf_30.png` - it had white
  streaks before) and the highlight stripes steady in colour. Gates: `python3 tools/gen_files_cmake.py
  --check` rc=0, and the upstream matching build after the three upstream-file edits -
  `main.dol` sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, Game Code 94.14% fuzzy / 63.29% matched
  (unchanged), every edit inside `#ifdef TARGET_PC` with the non-PC statements preserved verbatim.
- **Still open, same class:** the camera filters, `CPlayerGun` and the particles also copy into the
  shared spare buffer without a `Destination`, so they can still take each other's resolved texture
  (unchanged by this session, and no longer able to reach the visor's or the pane's copies). Fold them
  into `Destination` the same way when one of them shows the symptom.

## 2026-10-20 (twenty-ninth session): the F1 Teleport and items pages, Prime 1's shape

- **This commit:** the F1 overlay gets MP1's two debug pages, in this game's terms.
  - **Cheats page, Teleport block** (where MP1 keeps it, below the items; the Game page keeps
    this port's own area/world cycle buttons). A room combo (`IGetAreaAlways` names, so unloaded areas are
    named too; slots with no MREA are hidden) with a filter field, `Reload` (a world warp to the same
    world and room - the area warp deliberately ignores a request for the room it is already in),
    and a world combo built from the memory card's world table (`CSaveWorldMemory::GetFrontEndName`,
    with the GuiSys markup and the hand-placed line break stripped: the entry is
    `&line-spacing=75;Temple\nGrounds`).
  - **Cheats page, items.** Echoes' abilities and keys as checkbox grids, missiles / power bombs /
    dark and light ammo / energy tanks sliders bounded by `GetPowerUpMaxValue`, `Full health` and
    `Grant everything`. A grant sets the capacity to the game's own maximum for a counter (1 for an
    ability, 0 to remove it) and the amount to the slider's value: `AddPowerUp` for the capacity
    difference, then `SetItemAmount` for the amount. That is MP1's `SetPowerUp`/`SetPickup` shape.
    A first version moved both fields by the amount's delta, and a review caught what that does:
    sliding missiles from 255 to 55 lowered the capacity to 55 and left the amount at 255, and
    unticking an item whose amount was already 0 did nothing.
- **The first version kept a `CWorld*` and segfaulted on a world warp from the menu** - the
  outgoing world is freed while the overlay still draws the frames before the restart. The menu's
  own world warp is what found it (`[warp] 3bfa3eff: the outgoing world's pak stays readable`, then
  a fault); the console's `world` with the overlay closed survived, which is why it looked flaky.
  The room list is now a snapshot copied in `SetGameState`, keyed on `CWorld::IGetWorldAssetId()` -
  not on `gpGameState`'s id, which names the destination on the frame the warp is served while the
  outgoing world is still the live one (that key left Torvus Bog's page showing Temple Grounds'
  rooms).
- **Measured** (run on Xvfb :441, `MP2_SKIP_FRONTEND=1`, a copy of the `/tmp/opencode/locktest`
  card): the room picker lists `0  01_Temple_LandingSite` … 62 areas of Temple Grounds, and picking
  one warped (`[warp] area 9: loading` -> `[area] 9` -> `the game's own transition arrived in area
  9`); the world picker warped to `3dfd2249` and the page then read `0  00_Elev_Swamp_Temple_B`;
  `Grant everything` moved health to 1499/1499, missiles/power bombs/dark ammo to 255/10/250 with
  every box ticked; clicking the missiles slider at a quarter then put `55 / 255` on the HUD, and
  unticking and re-ticking Power Beam followed in the grid. Screenshots `/tmp/opencode/final_game.png`,
  `final_cheats.png`, `p2_torvus.png`, `r3_hud.png`.
- **Pages.** The F1 window now has the Prime 1 port's page set, as far as this port has the
  features behind each: Game (state and the area/world cycle buttons, Speedrun, Cutscenes),
  Controls, Video, Cheats (items then Teleport), Save states, Randomizer, System, Debug. MP1's
  Remastered, Mods, Archipelago and Tracker have no feature here, so no empty pages. `Debug` holds
  the free camera and photo (moved off Game), the world/area and game-update readout, and the
  console's commands; `Save states` is the old Game block on a page of its own; `Randomizer` reports
  the loaded seed and generates one with the logic fill (`PortRandomizer::SeedFilePath`,
  `GenerateSeed`; the dump log is no longer read).
- **The randomizer's user directory was the Prime 1 port's** (`MP_USER_PATH`, else SDL's pref path
  for "Metroid Prime"), so a dump or seed went somewhere the port's card and settings never look.
  It now reads `MP2_USER_PATH` first; the Randomizer page showed `/tmp/opencode/ov_user/...` under a
  run with that variable.
- **Layout.** The first version split the two: the warp pickers on the Game page and the items on
  Cheats. MP1 has both on Cheats, in the order health/grants, abilities, counters, Teleport, and
  this now does too (the port's `Previous/Next area/world` buttons stay on Game).
- Not covered: the Android page layout, and the items page in the front end beyond its
  "Waiting for gameplay..." guard.

## 2026-10-19 (twenty-eighth session): widescreen HUD spread, the MP1 way

- **`6ae31083e`, merged `f209d2f97`:** MP1's widescreen HUD. A first attempt spread widgets by their
  x position and detached the energy number from its bar; that was thrown away. MP1's mechanism is a
  per-camera opt-out (`SetAspectMatch(match, spread)`, default spreadable) plus a per-widget move in
  `CGuiFrame::Draw` after `CGuiCamera` widens the projection. `CSamusHud` marks the HUD and helmet
  cameras spreadable; the pause and message screens keep the game's proportions. Setting
  `MP2_HUD_WIDE` (key `hud_wide`, F1 > Video > Display, default on).
- Measured before/after: MP2's HUD never spread before this (the pause frame spans x 197-1088 of 1280
  at 16:9, which is MP1 with HudWide off). Now with it on the radar and energy readout sit at the left
  edge and the ammo/missile cluster at the right; with it off the HUD is 4:3 centred; the pause screen
  is identical either way. Screenshots `/tmp/opencode/hw2_{hud,pause}_{on,off}.png`.
- Merged-tree gates: host build 0 FAILED (binary 15:39), `gen_files_cmake.py --check` rc 0, every
  touched upstream file strips CLEAN, and the matching build still reads 94.14% fuzzy / 63.29% matched.
- Not covered: the selected-visor HUD frames are not marked spreadable (`CSamusHud` exposes only the
  HUD and helmet cameras), MP1's explicit anchor clusters are not ported, and the spread ignores a HUD
  scale below 100%. Nothing was walked in gameplay, per usual: the screenshots are of a standing player.

## 2026-10-19 (twenty-seventh session): every world swept

- **Method.** I used a scratch copy of `tools/sweep_areas.sh` that sets `MP2_WORLD="<world>,<first area's MREA>"` instead of
  `MP2_TELEPORT`, because the teleport only indexes the save's own world and wins when both are set. Settings: `NEXTAREA` walk,
  `GODMODE=1`, `SHARDS=4 DWELL=900 TAIL=900`, save `/tmp/opencode/ss_user`, and the binary from 15:39 (`c334bc9ae`).
  The logs are in `/tmp/opencode/wsw/log_w_<world>_<shard>.log`.
- **Worlds.** The console's `worlds` command lists five campaign worlds, FrontEnd04 (`69802220`) and six 1-area
  multiplayer maps. All of the 1-area worlds load, and boots of `233e42be` and `0x69802220` exit 0.

  | World | Id | Areas | Reached | Verdicts |
  |---|---|---|---|---|
  | Temple Grounds | 3bfa3eff | 63 | 0-53, 56, 57 | shard 3 STALL at 57->58; 54/55 not in the walk; 59-62 not covered |
  | Agon Wastes | 42b935e4 | 72 | 72 | all exit=0; reach stubs at 7 and 37 |
  | Torvus Bog | 3dfd2249 | 70 | 70 | all exit=0; reach stub at 46 |
  | Sanctuary Fortress | 1baa96c2 | 67 | 67 | all exit=0; area 16 POOL |
  | Great Temple | 863fcd72 | 12 | 12 | all exit=0 |

- **Totals across every log.** There were no `[port] caught`, `[player] killed`, missing-resource, `[model]`, assert or `[end]`
  lines. The only `[warning]` is the shutdown `Device lost: Device was destroyed`.
- **Temple Grounds 58 is a harness trap, not a game wall.** The log reads `[warp] area 58: its validation was held by a dock;
  releasing it` and then `[harness] no game update for 30.0 s (6322 updates so far, limit 13500)`, exit=124. A lone
  `WORLD=3bfa3eff,85499aa7` run reproduces it after 122 updates. The screenshot shows "Save progress to Memory Card in
  Slot A? Yes/No": the arrival spawn is inside a save station, and the sweep's `B` does not answer that prompt. Under gdb
  the main thread was idle in the frame limiter (`CGraphics::EndScene`).
- **Top blockers, earliest first:**
  1. Agon 7: `CParasite::ProjectVectorToPlane` reach stub (the parasites' plane projection is a no-op).
  2. Agon 37: reach stub for `TStateMachineStateBase<CPatterned>::~TStateMachineStateBase()`.
  3. Torvus 46: the `CParasite::ProjectVectorToPlane` stub again.
  4. Sanctuary 16: the id pool holds at `[ids] frame 14700: 982/1024 ids allocated, 982 objects` until frame 15000. That
     follows `[warp] dropped the areas around 16 (650 object ids free)`, so one more spawn burst there could exhaust the pool.
  5. Tooling: the save prompt at TG 58 (above) stops the sweep. `ResolveWarpAsset` reads an all-digit hex id as decimal
     (`MP2_WORLD=69802220` → `0x042918ec`, "neither the save nor the disc's MLVL names an area"; `0x69802220` works).
     The warp walk also never rolls credits, because slot 53 logged no `[end]`.
  `DebugDrawAABox` also hits its stub (Sanctuary 0, TG 44/48, Agon 47/59), which is harmless.
- **Coverage limit.** This sweep tests arrival plus about 15 s of standing gameplay under god mode. It does not test
  traversal, doors, elevators, bosses or the ending. Next: real `ProjectVectorToPlane` and state-dtor bodies, a `B`/`Yes`
  answer for save prompts in `sweep_areas.sh`, a hex-always parse in `ResolveWarpAsset`, and TG 59-62.

## 2026-10-19 (twenty-sixth session): interpolation phase 6, save-state resume and door locks

- **Phase 6** (`9b651429a`, MP1's section 3 "swooshes, electric effects and beam weapons"):
  `CParticleSwoosh` draws trails between the last two ticks (segments matched by age, spin the short
  way, the actor's shift dropped while it blends), `CParticleElectric` blends its line transform, and
  `CPlasmaProjectile` blends transform, length, width and angle (its Echoes `Render` did not fit MP1's
  edit, so the PC version is duplicated ahead of the original under `#else`). Beams opt out of the actor
  blend through `CActor::PortSetOwnPresentation`. `CWaveBuster`/`CFlameThrower` have no Echoes
  counterpart. `platform/include/port_tick_pair.h` and `port_tick_pair_tests` come from MP1. Uses the
  existing Smooth effects setting, so there is no new flag. Not seen on screen: both runs idled in area 0.
- **Quick resume, door locks, a patient restore** (`65a05ac34`, merged `f117ba308`): the resume state
  (`<user>/savestates/resume.mp2ss`) is written on a clean exit when `MP2_RESUME=1` and loaded at the next
  launch when the front end is skipped and no `MP2_WORLD`/`MP2_TELEPORT` was asked for; it is deleted once
  used and refused if its world is not on the disc. Doors keep their lock state in the room capture
  (applied with `kSM_Lock`/`kSM_Unlock`; one friend line in `CScriptDoor.hpp`). `TickRoom` now gives up only
  after 1,200 *idle* frames (a pending layer, a loading area or a progress change resets the count), with a
  hard cap. Verified end to end: `resume: wrote ... (Area 0)` then `loading it instead of the card's save`
  and `Samus is back in Area 0; the resume state is used up`, both runs exit 0, 0 unmatched. The door-lock
  apply and the real window-close path were not exercised.
  Superseded 2026-10-25 (qr1): the resume file is kept until the next snapshot, snapshots are taken
  during play, and a boot prompt asks; see platform/README.md "Quick resume".
- Merged-tree gates: host build 0 FAILED (binary 15:14), matching build "87 files OK",
  94.14% fuzzy / 63.29% matched. Every touched upstream file strips CLEAN.

## 2026-10-19 (twenty-fifth session): the glyph keys were right all along

- **Echoes uses Prime 1's button textures.** Opening the pause screen (START) and diffing its texture
  dumps against a gameplay run leaves 19 textures, and four are the hashes already in `kKeys`:
  `32x32 bb21e8755f36b2f0` (`AButtonIn`), `32x32 e6dbfd18d4666ee7` (`BButtonIn`), and the two `64x80`
  `AButtonOut`/`BButtonOut` sets. Keys are content hashes (`XXH64` of the decoded bytes,
  `extern/aurora/lib/gfx/texture_replacement.cpp:428`), and the art is shared between the games.
- So the earlier "the binding icons replace nothing in Echoes" was wrong: every run up to then
  dismissed the HUD memo (`MP2_SKIP_INTRO=1`) and never drew a prompt, so the textures were never
  loaded. Evidence: `/tmp/opencode/pause.png`, `/tmp/opencode/pause_only.txt`.
- The `MP2_DEBUG_PROMPTS` hook (`36775d730`) and its constructor variant were reverted: only one
  `CGuiModel` is ever built in a run (id `ffffffff`), so Echoes' prompt widget is not a `CGuiModel`
  and the hook told us nothing about it.
- **Left to do:** run `tools/make_prompt_glyphs.py` to produce a `bindings/` set, point `MP2_TEXTURES`
  at it, and confirm the pause screen's bottom bar shows keyboard glyphs instead of A/B/C/Y.

## 2026-10-19 (twenty-fourth session): the texture dump path, and where Echoes' prompts are

- **`f558e5b3f`:** a texture dump dropped every file. `dump_editable_texture_dds` wrote into
  `<cache>/texture_dumps`, but only `load_replacement_directory` ever created that folder, so a run
  with no pack installed - the one way to author a pack - had nowhere to write. The dump now creates
  the folder, and a dump skipped because the format did not convert logs why (once per format/size).
  Verified: `MP2_DUMP_TEXTURES=1` writes 147 `tex1_*.dds` into the run's own cache folder
  (`/tmp/opencode/c_run/texture_dumps`, because `tools/run_gameplay.sh` sets `MP2_CACHE_PATH` from
  `CACHEDIR` - the `MP2_CACHE_PATH` a caller passes is overridden, which is why the first attempts
  looked empty).
- **Echoes' button prompts are not 32x32 textures.** Of the nine 32x32 dumps from a Temple Grounds
  boot (`/tmp/opencode/glyphs_sheet.png`), none is a button: the game draws prompts from GUI *models*
  (`model_abutton` in `CSamusHud.cpp:157`, `CHudDecoInterfaceScan.cpp:108`), whose texture is not one
  of them. Building Echoes' `kKeys` therefore needs a dump taken while a prompt is up, plus the model's
  own texture size/hash - still open.


Older sessions: `docs/STATUS-history.md`.

The openchamber tool rejects every non-string argument (2026-10-20), so lanes run as subagents now; old lane sessions can't be resumed from here.
- 2026-10-21 16:40 batch9 merged (081b71c14): CommandoPirate a957ff47c, EmperorIng1 298b62b1c, Splitter a619f5a69, DarkSamus2 ce575f3c8, Ing blob 9f8fd3003, Ing 646a9fabd, Sandworm cad0531f5, SwampBoss2 4a51bc63c, DigitalGuardian f8ec985d5; smoke9 no regressions.
- 2026-10-21 17:20 batch10 merged (f09b42959): CommandoPirate df8fbb5b6, DarkSamus2 5fe53f11c, EmperorIng1 af5535c8f (done), Splitter 4a756509f, Ing 1a0ba410f, SwampBoss2 32c062e08, BoostGuardian 75bd1ac4d (done), Sandworm 36fb82f7c, SandBoss 5a5858859 (done), SpacePirate 727d520b8, DigitalGuardian 0d273717a + aurora 32 MiB uniform buffer (82b1c482c); 1890/4730 fns. smoke10 vs smoke9: turret (GPU reset 16:56, innocent context) and ing (X display shared with another run) pass solo (smoke10b); agon54 exit 137 = shutdown hang twice, same as spacepirate (sp-hang lane). New console `noise` (196d15fee).

## 2026-10-21 (port orchestrator, late): batches 12 and 13

- Batch 12 (25b367dda) and batch 13 (0d9af897f) merged after asm review by a dedicated Opus review lane and orchestrator checks; smoke12b/smoke13 no regressions (`--jobs 4` once gave flaky exit-137 shutdown hangs on ing/ep/plain/emp3, clean at `--jobs 2`).
- Audit-session fixes routed or applied: PlayerTurret refire `<= 0` (a19f25ae8), DarkSamus CanMissileDeflect `> 0` and RollReflectChanceForHit's first roll (f5802a210), CommandoPirate's five 662C/8028/86B4 items, the DigitalGuardian knee knock-back, the 177C8/114C8 stand-ins, the SpacePirate weapon switch, Hurled, Ing timer, trooper alpha and charge list.
- MiniMax misreads caught in review (`lis` as a small value, `__shl2i` input read as a high word, an early return skipping retail's tail) are now in LANE_BRIEF "Decode traps".

## 2026-10-19 (twenty-third session): HD textures and prompt glyphs, ported and wired

- **`d8a795ffc`:** `platform/port_textures.cpp` and `platform/port_prompts.cpp` (126 and 361 lines,
  reduced rewrites of MP1's 357 and 876, and in no CMake target) are replaced with ports of MP1's
  files and added to `mp_port_audio`. `Initialize` runs once after `aurora_initialize` in
  `platform/main.cpp`, `Poll` at the end of `PortPumpEvents`, and `PortPrompts::NoteTouchInput` in
  `platform/port_touch.cpp`. `MP2_TEXTURES`, `MP2_USER_TEXTURES`, `MP2_TEXTURE_DEVICE` and
  `MP2_DUMP_TEXTURES` replace MP1's `MP_*` names; `MP2_DUMP_TEXTURES` now feeds Aurora's
  `allowTextureDumps`. `tools/make_prompt_glyphs.py` and its 181 CC0 icons are copied, not run.
  `platform/README.md` documents the four settings and the `bindings/` folder.
- Verified: `gen_files_cmake.py --check` rc 0, `metroid_prime2_port` 0 FAILED, boot exit 0 with
  `[textures] device 'keyboard'; built-in set: (none); user pack: /tmp/opencode/ss_user/user_textures`
  and `[prompts] no texture folder; binding icons off`.
- **Open:** `kKeys` in `port_prompts.cpp` still holds MP1's asset hashes (`AButtonIn`,
  `AButtonOut`, ...), so the binding icons replace nothing in Echoes. Echoes draws its prompts from
  GUI *models* (`model_abutton` in `CSamusHud.cpp`, `CHudDecoInterfaceScan.cpp`), not from the
  standalone textures MP1 keys on, so MP2 needs its own key list - a texture dump while a prompt is
  up, or the model's TXTR asset ids - before this does anything visible. `MP2_DUMP_TEXTURES=1` wrote
  no dumps in a 400-frame boot (`/tmp/opencode/dump_run.log`), so that route needs work too.
- **Open:** the desktop's touch overlay pads show as an Xbox controller, so the texture set resolves
  to `xbox` on a PC boot; harmless with no packs installed. `PortPrompts::KeyboardActive` has no
  caller here.

## 2026-10-19 (twenty-second session, continued): the two "problems" closed

- **The shard's 54/55 gap is the ending, not a bug.** Areas 53, 56 and 58 of this save are
  `game_end_part3/4/5`: entering 53 starts the ending and the game walks its own areas, so the sweep's
  tokens never reach 54 or 55. `tools/sweep_report.py` now says "the ending took over at area N
  (CREDITS)" and lists the un-walked slots as the ending's own, instead of calling the first one
  "missing" (commit `ef51d7f24`, checked on both the shard log and the earlier hang log).
- **The 162 unmatched objects are fixed** (`cb36d6d52`): the restore now activates each layer its saved
  objects live in (through `CStateManager::PortWarpToLayer`, one per tick, waiting on the new
  `PortLayerRequestPending`) before applying that area, because a layer switched on in the live room
  before the save is not always in the reload's layer bits. Measured: `/tmp/opencode/layer_run2.log`
  reports `0 unmatched` (was 162) and logs `area 0 layer 3 activated`, where before the whole layer was
  missing. One caveat from the same run: `1 of 2 areas` - area 1 had not loaded by the frame limit, so
  its objects were not applied that run.
- Still open: the phone check of Android fullscreen and the F1 two-finger scroll needs the phone
  unlocked.

## 2026-10-19 (twenty-second session): the shard-4 timeout, and a warped hand-over that waits

- **Shard 4's `exit=124` is the ending, not a hang.** A fresh run of `SHARD=4 SHARDS=5` on `port`
  (`/tmp/opencode/log_shard4b_4.log`) reached area 56, printed `[end] the credits are rolling`, and
  `tools/sweep_report.py` calls area 56 `CREDITS` with no walls. A `CCredits` window means no
  `CStateManager::Update`, so the run can only end on its timeout.
- **What did look like a hang** was the earlier randomizer-build run (`/tmp/opencode/log_rando_4.log`):
  the hop into area 56 was handed to the game's own transition ("681 ids free is too tight for the
  direct route") and `[area] 56` never followed, with no credits line. Not reproduced before or after:
  forced hand-overs into 52->53 and 55->56 arrive normally on both builds, and the fresh shard run
  went through 56.
- **Hardened, unconfirmed:** `CMFGame`'s `kFS_InGame`/`kTP_Load` now drops the portal transition when
  the hop is a warp's (`CStateManager::PortIsWarpHandoff`), because `kTP_Load` waits on
  `CPortalTransition::IsReady()` and that cannot become ready while the port holds the destination's
  stream - the same reason `PortPlaceArrivalTick` ends such a transition after arrival. A
  `PrepareAreaTransition` refusal (the other silent way this flow can stall) now logs every 120
  retries. Commit `9cd10f103`; both files strip CLEAN.
- **Separate finding, not fixed:** the shard reached only 52, 53 and 56 of its 52..62 slice. Between
  `[area] 53` and `[area] 56` no warp request was served (`[warp] serving a request` counts 7 against
  10 `NEXTAREA` tokens), so the area changed without the port's warp path. The sweep's coverage check
  reports `first missing 54`, which is the sweep-as-measurement problem `tools/sweep_areas.sh`
  warns about elsewhere, not (yet) a game fault.

## 2026-10-18 (twenty-first session): FOV culling, Android fullscreen and scroll, MP1 parity audit

- Fixed: first-person FOV above ~75 deg (more on a phone) culled geometry. `CFrustumPlanes`
  scaled the vertical half-angle by aspect before taking the tangent, so the side planes flipped once
  `fovy/2 * aspect` passed 90 deg (about 83 deg at 2.16:1). TARGET_PC now uses
  `nearZ * tan(fovy/2) * aspect`, the width `CGraphics::SetPerspective` builds. Commit `b04621e20`.
- Fixed: Android window was not fullscreen (SDLActivity.onCreate forces `setWindowStyle(false)`);
  `MetroidPrime2Activity` now calls `setWindowStyle(true)` after `super.onCreate`.
- Fixed: F1 two-finger scroll called `ImGui::GetIO().AddMouseWheelEvent` from the Java UI thread;
  `PortOverlay::Wheel` now queues steps atomically and `Draw()` applies them on the game thread.
  Commit `d3e178014`. Neither Android fix is verified on the phone (screen locked, no save).
- Matching build unchanged: 94.14% fuzzy, 63.29% matched. Release APK re-signed with the debug key and
  installed in place (`adb install -r`).

### Parity with Prime 1 (read-only audit, 2026-10-18)

- DONE since the audit (2026-10-19): save states with room state (F5/F9, 8 slots + undo; see
  `platform/port_savestate*.cpp`), the randomizer seed loader and no-logic generator
  (`docs/RANDOMIZER.md`), and the LiveSplit client with an F1 Speedrun section
  (`platform/port_livesplit.cpp`, `platform/README.md`). Photo mode and free camera are also in tree
  (`platform/port_photo.cpp`, `platform/port_freecam.cpp`).
- ABSENT, by value on a phone: Remastered ground movement and moving platforms (needs MP2's
  ground mover); Archipelago (needs a websocket client and an MP2 location table); randomizer
  logic (seeds can be unbeatable: no-logic shuffle over the Temple Grounds dump only).
- ABSENT, smaller: audio volume and mute row; GPU/Vulkan driver choice (`platform/main.cpp:754`
  is fixed to `BACKEND_AUTO`); sim rate; original-experience preset; Discord presence (no
  backend); mods folder (no loader).
- Unbuilt: `port_textures.cpp` and `port_prompts.cpp` are in no target (HD texture sets and
  device button glyphs need wiring).
- PARTIAL: desktop F11 fullscreen toggle (only Android sets it); widescreen HUD spread; gyro aim
  (built, never run with a controller); save import (no folder discovery, no region conversion).

## 2026-10-18 (twentieth session): frame interpolation phases 0-2

- Ported MP1's smooth-frames system (`../MetroidPrimePort/docs/FRAME_INTERPOLATION.md`), in commits
  `c2e49cf25` (phase 0), `1932fa3dc` (phase 1, actor transforms), `0a4a58698` (phase 2, skinned poses).
- Phase 0: fixed 60 Hz clock (`x7c_tickClock`, `GetTickInterpolation()`, `PortTiming::FixedStepClock`),
  camera before/after snapshot blend (`CCameraManager::SetPresentationInterpolation`), `smooth_frames`
  (default on, env `MP2_SMOOTH_FRAMES`, F1 Video > Frame rate, console `interp`). Blend runs only with
  the 60 fps cap off. Tick generation is `PortTiming::BeginTick()`/`TickGeneration()`.
- Phase 1: `CActor` previous transform and `CPortActorRenderScope` (view shift at draw), setting
  `actor_interpolation` (off by default). Tests: `port_presentation_rules_tests` (copied from MP1).
- Phase 2: `CAnimData` keeps last tick's pose per bone and `PortPresentedPose()` blends it while
  drawing; setting `pose_interpolation` (env `MP2_POSE_INTERPOLATION`, F1 "Smooth animation", off by
  default). Four skinned draw sites in `CModelData.cpp` use it under TARGET_PC.
- Verified (phase 2): `gen_files_cmake.py --check` rc 0; `metroid_prime2_port` builds with 0 FAILED;
  host tests rc 0; matching build 94.14% fuzzy / 63.29% matched; strip-diff CLEAN for the four
  upstream files; desktop boot with `MP2_POSE_INTERPOLATION=1` exit 124, no crash lines.
- Not verified on screen: no save file exists (`/tmp/opencode/u_a10`, `mp2card/user` absent), so
  gameplay cannot be reached. Front-end boot only.
- Phase 3 (`a84d788b0`): `CElementGen` draws between its last two tick states (retail sub-frame path
  fed the presentation factor, global translation blended, newborns shifted, lines at the presented
  position); `CEnvFxManager` rain/underwater step back, snow per flake. Setting
  `particle_interpolation` (env `MP2_PARTICLE_INTERPOLATION`, F1 "Smooth effects", off by default).
  Verified: gen --check rc 0; link 0 FAILED; host tests (presentation rules, input engine, input
  bindings) rc 0; matching build 94.14%/63.29%; strip-diff CLEAN on all upstream files; boot exit 124.
  Not ported: projectiles, swooshes, electric effects, beams (phase 6) - check before claiming done.
- Phase 4 (`92acd3eb5`, MP1's "look input per frame", its phase 1): `PortMouse::PresentedAimDelta` previews
  the pending mouse/gyro/stick turn; `CCameraManager::GetPresentedLookRotation` turns the presented view by
  it. Setting `look_interpolation` (env `MP2_LOOK_INTERPOLATION`, F1 "Smooth look", ON by default as in MP1,
  active only with smooth frames on and the cap off). Verified: link 0 FAILED; matching build 94.14%/63.29%;
  strip-diff CLEAN; boot exit 124. Not checked on screen (no save). Pointer cursor still steps per tick.
- Phase 5 HUD/2D (`bb7161ca6`, MP1 doc section 5): reticles present at their between-tick position
  (`CActor::PortPresentedPoint`), GUI clock lags by (1-t)/60 s (steps per tick with the cap off), HUD
  lag/bob blends the last two ticks during the draw. Resets (section 6) need no new code: stale pairs,
  teleports, 45-degree turns and camera switches already snap. Verified: 0 FAILED link; presentation
  tests pass; matching build 94.14%/63.29%; strip-diff CLEAN on 13 files; boot exit 124. Not checked on
  screen (no save). Caveat: GUI animation no longer speeds up with the frame rate when the cap is off.
- Next: the sweep (MP1 doc section 7). Needs a playthrough or save to be meaningful, so it is not run.
- Gotcha: `/tmp/strip.py` (crude) dropped everything after `#else`; use a stripper that keeps the
  `#else` branch, or its CLEAN results mean nothing.

## 2026-10-18 (nineteenth session, later): frame cap and render scale

- MP1's 60 fps cap (`frame_limit`, default on, env `MP2_FRAME_LIMIT=0|1`, F10 toggle, F1 Video >
  Frame rate) paced by `PortPaceFrame` in `platform/port_events.cpp` at the top of the main loop
  (`src/MetroidPrime/main.cpp`, TARGET_PC only; the loading loop is not capped). Headless runs are
  now capped too unless `MP2_FRAME_LIMIT=0`, so harness timings in AGENTS.md may change.
- Static render scale (`render_scale`, env `MP2_RENDER_SCALE`, 0.5-4, F1 Video > Quality). Default
  0 = native, not MP1's 1.0 (MP2 has always drawn native and photo mode assumes it). Photo capture
  keeps its own scale. Dynamic resolution and the sim-rate setting are not ported.
- Verified: port build, gen --check, host tests, matching build 63.29% matched, desktop boot with
  `MP2_RENDER_SCALE=2` for 60 s with no crash. Not checked on screen.

## 2026-10-18 (nineteenth session, later): aspect ratio

- MP1's aspect setting ported: 4:3 / 16:9 / Follow window (default, as MP1). Settings key `aspect`,
  env `MP2_ASPECT` / `MP2_WIDESCREEN`, F1 > Video > Display combo, console `aspect [4:3|16:9|window]`.
  Render target is resized through `CGraphics::PortResizeFrameBuffer`; world viewports in CMFGame and
  GUI cameras are widened (`CGuiCamera::Draw`) so menus keep their proportions. Window opens 1280x720
  for widescreen, 960x720 for 4:3.
- Not ported: the Widescreen HUD spread (MP1's HudWide and per-screen SetAspectMatch).
- Verified: port build, gen --check, host tests, matching build (63.29% matched), strip-diff of the
  four upstream files; a 90 s desktop boot ran with no crash. Not yet checked on screen.

## 2026-10-18, nineteenth session: auto-aim

- F1 > Controls > Buttons "Auto-aim" (`MP2_AUTO_AIM=0` turns it off, `auto_aim` key, default on).
  Off, `CPlayer::UpdateAimTarget` clears the aim target and keeps only an orbited object as the
  target, as MP1's `UpdateAimTarget` does (`PortOverlay::AutoAim()` gate under TARGET_PC).
  Auto-aim on is the old behaviour.
- Gates: strip check of `CPlayerOrbit.cpp` equals HEAD's non-PC text; `metroid_prime2_port` builds;
  `gen_files_cmake.py --check` rc=0.
- Not yet checked in game: a shot with the setting off flies at the crosshair with no homing.
- Pointer aim (MP1's Trilogy style): F1 > Controls > Options > Aiming, `MP2_POINTER_AIM=1`,
  `MP2_POINTER_AIM_BOX` (20..95, default 70), `MP2_POINTER_AIM_TURN` (30..720, default 180).
  Mouse, twin-stick or touch aim must be on. `PortMouse::UpdateAim` steps the pointer; the gun and
  shots aim at the ray's hit (`CPlayer::UpdateGunTransform`, TARGET_PC block); the Remastered cursor
  sits at the pointer. Build-verified only: not run in game. Not ported: inter-tick smoothing,
  Also: console `pointeraim [on|off|<x> <y>]`; shots fly to the pointer's
  hit point (`CPlayerGun::PortAimShotAtPointer`, `CPlayer::UpdateGunTransform` TARGET_PC). Inter-tick
  smoothing does not apply: MP2 has no frame interpolation, so the presented pointer is the ticked one.
- Gyro aim (MP1's, pointer only): F1 > Controls > Options > Aiming, `MP2_GYRO_AIM=1` (default off),
  plus `MP2_GYRO_SOURCE`, `MP2_GYRO_SPACE`, `MP2_GYRO_AUTO_CALIBRATE`, `MP2_GYRO_SMOOTHING`,
  `MP2_GYRO_TIGHTENING`, `MP2_GYRO_CURSOR_ANGLE`. Reader in `PortInputDevices::PollGyro`
  (`platform/port_gyro_motion.cpp`, GamepadMotionHelpers 10 vendored in `extern/`, MIT). Built and
  gated only: no controller was available. Also ported: gyro sensitivity, "gyro also turns the
  camera", hold/pause/toggle modes for the GyroAim action (R3 or left Ctrl by default), Recalibrate
  with a 5 s countdown, and the status readout. Open: the GyroAim default left Ctrl is also Morph
  Ball's key in the optional `kMouseKeyboard` preset (`platform/port_controls.cpp:332`); the engine
  defaults do not clash. Choose a different gyro key or keep MP1's default when that preset is used.

## 2026-10-18, eighteenth session: remastered charge (Power Beam only)

- F1 > Gameplay "Remastered charge" (`MP2_RAPID_CHARGE=1`, `rapid_charge` key): holding fire with the
  Power Beam fires MP1's quick shots (`skPortRapidChargePower`), and the charged release uses that
  table's timing. Other beams are unchanged: `PortRapidChargeFor` returns null for them.
- The no-Charge-Beam branch in `UpdateNormalShotCycle`'s caller (`Main`) follows MP1's
  `ProcessNormalState`. It is Power-only in effect because `mPortRapidCharge` is only set for the
  Power Beam.
- Gates: strip check of `CPlayerGun.cpp` and `CPlayerGun.hpp` equals HEAD's non-PC text;
  `metroid_prime2_port` builds; `gen_files_cmake.py --check` rc=0.
- Not yet checked in game: muzzle flash during quick shots (`mChargeEffectVisible` is false while
  `kCP_ChargeRequested`), and the damage scaling of a charged release during `kCP_Charging`.
- Separate vertical speed: F1 > Controls > Controller settings, "Separate vertical speed"
  (`PortMouse::SetStickAimSplit`, `MP2_STICK_AIM_SPLIT=1`), with "Vertical stick aim rate" when on.
  `AddStickAim` uses the Y rate for up and down. Build-verified only; the stick rate is not persisted
  in settings.cfg, and neither is this one.
- Button glyphs is blocked, not a settings row: `platform/port_prompts.cpp` is in no CMake target,
  its `Initialize`/`Poll` are not called, and there is no `tools/make_prompt_glyphs.py` or icon set.
  The row needs those first.

## 2026-10-18, seventeenth session: photo mode stages 2a and 2b

- `c0ac5167f` stage 2a: supersampled capture (`RequestCapture`, PNG via `aurora::request_screenshot_png`),
  console `photo [1|2|4]`.
- `c368b4530` stage 2b: F1 Game page Photo block (FOV 20-120 or game default at 0, roll, speed,
  supersample, show Samus, Take photo). FOV reaches `CStateManager::SetupViewForDraw` under TARGET_PC.
- **Checked on screen**: the FOV slider changes the picture (wider view after the click; camera pose
  unchanged). The slider's exact value was not read back.
- Decided: photo mode is reached from the F1 menu only. No pause-menu entry.
- Polish (`564da61bd`, `29270a223`): roll slider ±180 to match `SetRoll`; a "Game FOV" button returns
  the override to 0; a failed capture says "see the log"; the unread toast message is gone. Checked
  on screen: the roll range, the button's reset to "game (55)", and `photo 1` saving
  `screenshots/metroid-prime-2-photo-*.png` (686x480).
- No toast: the panel's Saved/failed line and the log carry the result.

## 2026-10-18, fifteenth session

- F1 menu ported from MP1: `61a8244fb` (page window + Prime theme on desktop), `0ca0a556b`
  (sub-tabs, search jumps, theme, pad nav, settings dirty state, vendored Aurora imgui.cpp change),
  `7d7fb1e57` (touch aim speed / spring swipe in touch.cfg).
- Checked on the desktop: Controls sub-tabs and the Video Display sub-tab render.
- Not checked on screen: System page, sub-tab search jumps (`MP2_DEBUG_SEARCH_AUDIT`), phone F1 page.
- Open: NEEDS-BACKEND rows, photo mode key choice (F8 vs warp keys F5-F10), save states (no
  serializer), gyro reader/aim consumer, pointer menus.

## 2026-10-18 (fifteenth session): stage C, the F1 Controls page

- **The Bindings page is MP1's remap UI and presets** (`1e436a53c`): `platform/input/port_input_remap_ui.cpp`
  (minus gyro, menu/map, pointer and turbo rows) and `platform/port_controls.cpp` (presets `classic`,
  `mouse`; pad `gamecube`, `remastered`, `modern`, `southpaw`). Bindings live in `controls.toml`.
  Removed: `platform/port_binds.{h,cpp}`, `platform/port_remap_ui.{h,cpp}`, console `bind`/`binds`,
  `keybinds.cfg`.
- **Verified**: `port_input_engine` and bindings host tests pass, `gen_files_cmake.py --check` rc 0,
  `metroid_prime2_port` relinks from current sources.
- **Not yet verified**: the phone's F1 page window (search, Controls, scale) with the phone free.

## 2026-10-18 (fourteenth session): the two open bugs closed, and the settings file in

**The Sanctuary area-return crash is gone, on the phone** (`0c33bd5a6`, confirmed rather than
changed here). Sanctuary Fortress is world `42b935e4`; the console's `world 42b935e4` loads area 0
(`57ce3a52`, 552 sections / 525 surfaces), `door` stands the player in front of the nearest door,
`press a 90` breaks it and `stick 0 127 240` walks through, so the transition is on foot and the
area behind is occluded, not warped away. Area 0 -> 1 -> 0 gave, on the return,
`AddStaticGeometry: aram=1 constructed=0 instances=1 occl=1` and
`FillInStaticGeometry (552 sections, already converted)` with `525 surfaces, 0 bad` — the same
counts as the first load — and the whole session's log has **zero** `[model]`/`[res]` refusal lines
and no fault. The Temple Grounds pair (areas 51 and 48) came back the same way earlier in the same
run. That process had `MP2_DEBUG_AREA=1` in force — the `[area]` lines are behind it
(`CGameArea::FillInStaticGeometry`), so the flag is what makes the phone run readable.

**The music loop race is fixed and quiet** (`00869fb76`). On the phone, `port_paths.txt` with
`env.MP2_DEBUG_AUDIO=1`, `env.MP_AUDIO_STATS=1` and `env.MP_AUDIO_DUMP=<files>/music.raw` captured
443 s (56.7 MB) of the Temple Grounds save's own track (`/audio/SplinterHive-132.dsp`,
`loop=1 [0x6280,0x1af400]`). Four wraps happened; each is the documented shape — a refill that
reads to the end (`read=0x2400 loopLen=0x6b00`) and continues from `loopStart=0x6280` — and the
run has **zero** `dsp read completed ... raced` lines, `pending=0` on every update and the halves
alternating without a hiccup. Offline, 221 adjacent 2-second blocks contain no near-duplicate pair
(the pre-fix symptom was one half replaying the previous one), no 0.25 s window is silent after the
music starts at 2.9 s, and the largest adjacent-window RMS jump is 0.017 — an envelope, not a
seam. `ffmpeg -f s16le -ar 32000 -ac 2` turns the dump into a wav; `music_full.wav` and a 34 s
`music_loop_seam.wav` around the first wrap are in `/tmp/opencode` for a listen.

**The settings file is in** (`8fff230de`): `<user>/settings.cfg`, 14 keys, `MP2_<KEY>` wins over
the file, and the write rule that matters — an entry remembers the value it started with, a save
writes only entries that differ, and nothing is written (or created) when nothing changed. The
reverted attempt's bug (a second run writing defaults back) cannot recur. Detail in the commit; the
console's `settings [<key> <value>]` lists and sets.

**Correction (same session): port, do not reimplement.** Three pieces of this session were
written from scratch where the Prime 1 port already has them, and are being replaced by ports of
its files, in this order:

- The aim crosshair (`a53472d61`): MP1's `CTargetReticles.cpp` cursor code transplanted -
  `PortXhairVertex` and the arm/centre station tables, `PortDrawRemasteredCrosshair`,
  `PortRemasteredCrosshairColor`, `PortUpdateRemasteredCrosshair` and its three members - with its
  settings' meanings (classic|remastered, size 25..100%, opacity 0..100, custom colour) instead of
  an ImGui cross I invented. The classic model does not resolve in this port, so `remastered` is the
  default. One deliberate difference: the cursor sits on its own 1.25 m plane in front of the
  camera, because the game's reticle position is tens of metres away (its model is scaled to
  compensate) and made the ported geometry several times too large.
- The device layer (`59f2e4a00`): MP1's 1,209-line `port_input_devices.cpp` ported over the
  reduced version, with the Aurora calls it needs (`PADGetRawState`, `PADGetDefaultMapping`,
  `PAD_NATIVE_BUTTON_TRIGGER_*`) and MP1's `port_input_map.h` verbatim. The drops (save states,
  photo, pointer menus, beam chords, turbo, gyro, the Original toggle, MorphedBall/Map contexts,
  MP1's tick clock) are listed in the commit message with their reasons.
- The settings file: MP1's `debug_ui/settings.cpp` mechanism (`SettingKey` table, `SettingsText`,
  `SaveSettings` writing only when the text changed, and its `settings self-test`) and its
  `port_env.h`, replacing my `AddBool`/`AddInt`/`Resolve` registry. In flight.

- The F1 window (`f21e742a9`): MP1's `debug_ui/search.cpp` ported into
  `platform/port_overlay_search.cpp` with its settings index (`port_settings_index.inc`), plus the
  Aurora ImGui item hooks it needs (`extern/aurora/extern/imgui_item_hook.cpp`), and MP1's
  `UpdateUiScale`/`ApplyThemeIfChanged` in `platform/port_ui_common.cpp`. The filter/section
  mechanism and the console's `filter` are gone. Verified on the desktop (the window's page tabs and
  "Search settings (Ctrl+F)" field, `morph` listing Fast morph and Spring Ball with their page
  paths, a click jumping to Gameplay with the row shown, `uiscale 150` growing the window); the
  phone's page window with the search still needs one run with the phone free.

Stage C (MP1's remap UI and presets) is done, see the fifteenth session above; the 22-row Bindings
bridge it replaced is gone.

**The input engine import reached pad 0.** `cc210f983` is stage A: MP1's `port_input_engine` and
`port_input_bindings` with `port_strings.h` and both host tests, in `mp_platform`, nothing calling
them. `431f82995` is stage B: `platform/input/port_input_devices.cpp` reads port 0's SDL gamepad
(Aurora's mappings and deadzones), the keyboard, the mouse buttons and the extended buttons into a
`RawState`, builds the port's keyboard layout with `controls.toml` overlaid, runs the engine and
synthesises a `PADStatus`; `PortPumpEvents` hands that to Aurora through a new
`PADSetPortOverride` in the vendored Aurora, and the virtual pad still merges afterwards, which is
why the console's `press`/`stick`/`cstick`, `MP2_INPUT` and the phone's touch pad are untouched.
The override stays set until the port replaces it: the one-shot version (MP1's) lost the keyboard
in the real game, so the game evidently reads the pad more than once in an iteration.

Desktop, all with `MP2_CONSOLE=4799` and `tools/keytap.py` sending real keys to the Xvfb window: a
held `w` moves the player (`-106` to `-80` after `face 180`, which is what proved the first
"no movement" reading was a wall), `x` reports `game input A=1` in `status`, `bind A b` writes
`controls.toml`, `b` then fires and `x` does not, and a relaunch loads it
(`[input] .../controls.toml loaded (1 profiles)`). `press a` and `stick` from the console still reach
the game. On the phone (APK rebuilt and installed): the touch stick moves the player and a press on
the touch A pill gives `game input A=1`, so the overlay's virtual gamepad survives the new path.

## 2026-10-18 (thirteenth session): the phone becomes the target

The APK was built and installed (debug-key signed, because the installed copy is; the release key
would need an uninstall that deletes the app's private disc image and card). Everything below was
then driven by what the phone showed.

**Diagnostics** (`eea16f8a1`): the Android fault handler walks `_Unwind_Backtrace` and prints
`module+0xoffset (symbol)` frames, and `platform/port_breadcrumb.cpp` keeps a 48-entry ring of recent
actions (texture binds/frees, resources in no loaded pak) dumped as `[crumb]` lines before the
re-raise. It turned "SIGSEGV in resolve_static_texture" into "this 642x450 texture, this pointer" in
one run. Plus texture-safety fixes it exposed: `texture_source_is_mapped` checks every page of the
source, and `CTexture::UnloadBitmapData` re-loads the emptied object into the GX slots holding it.

**The morph transition** (`917f82173`): the crash was `CCameraFilterPass::DrawRandomStatic` passing
`((rand()+0x1f)&~0x1f)+0x8000` as texture data - low RAM on the console, garbage on the host. The
port owns a noise block now. The Morph Ball's missing inner glow was `CMorphBall::Render` never
drawing `mBallInnerGlowGen`; retail's table is plain white and its condition is "opaque and not
hacked". The later "lights up like a boost charge" report is retail's own pulsing glow (checked
against Dolphin) - the port's shell reads more golden, which makes it stand out.

**Colours** (`e32a7bc36`, `61336daf4`): Aurora reads every TLUT big-endian, and two palettes were
built on the CPU in host order - `CCubeRenderer::DrawScanVisor`'s scan lookup table (every scannable
object pink instead of blue) and the font palettes (`CTextRenderBuffer::GeneratePalette` reads disc
order; `CRasterFont::DrawString` builds its table in memory and needs the swap on the write side).
HUD text is white/light blue now.

**Scan visor**: the code was complete - `DrawScanEffect`, `CScanDisplay`, `CHudDecoInterfaceScan` all
match retail's calls, and `DrawDarkEffect`/`DrawEchoEffect` are empty in retail too. Scanning never
started because Scan Item reads the *analog* L trigger and the virtual pad only set the digital bit
(`251c2b04d`).

**Music** (`5a1361d56`): silent for three reasons - `SRSFHeader` read raw (magic `'SR'`, version
check failed, stream dropped), the stream pointer passed as MusyX's 32-bit `user` (a SIGSEGV on the
audio thread at the first refill), and MusyX reading the ADPCM predictor/scale from byte 3. Music
plays now (rms 1100-6600, MusyX peak 0 -> ~5000); its loop point is being fixed.

**The phone's F1 and touch controls** (`bed7f6ce7`, `7b88f433f`): the on-screen pad is MP1's
`TouchControlsView` copied, with all 48 natives implemented in `platform/port_touch_jni.cpp` (wheels,
Missile, L/R, START, the editor, aim, spring flick, hide/restore; minimap taps, pointer menus, the
toast, `touch:` bindings and wheel icons answer safely). The F1 window is MP1's full-screen page
window inside the SDL safe area with the Prime theme from `ui_common.cpp`, a page list, thumb-sized
rows at display scale clamped to 2, and `port_remap_ui`'s Controls > Remap page (click-to-arm
capture, per-row Default, Reset all, conflict markers). Still missing from menu parity: the search
bar, a UI scale control, the settings file, and video/audio pages with real backends.

**Aim and the Sanctuary crash** (`a35193272`, `0c33bd5a6`): a touch aim drag snapped the camera
back to level on release because `PortMouse::UpdateAim` treated touch as idle once `TouchAimActive()`
went false and synchronised to the free-look camera; MP1's direct aim is persistent relative deltas,
so a latch now keeps the aim the owner after the lift (-48.7 deg held at -48.6 after release on the
phone). And the Sanctuary door crash was not disc data: `FillInStaticGeometry` converts surface
headers and halfword sections in place, an occluded area's ARAM token stores the *converted* bytes,
and the return path converted them again - every material index became its byte-swapped self.
`CPostConstructed::mGeometryConverted` skips the second pass, and the model/resource parsers now
refuse data they cannot use with a one-line `[model]`/`[res]` message naming the asset instead of
dereferencing it.

**Settings file**: a lane started `PortSettings` (`<user>/settings.cfg`, env wins over the file) and
was cancelled mid-work; it compiled but a second run wrote the defaults back over the file
(`MP2_FAST_MORPH=0`), so it was reverted rather than committed. It needs a proper pass, including a
save only when a value actually changed.

**Not a bug**: the see-through Worker Splinter in Temple Grounds area 12 - its model (716998c9) is
additive with no depth write in the disc data itself (the game's creature log calls it a mucus
layer), unlike the ordinary Splinter's opaque model.

## 2026-10-18 (twelfth session): a log file, the automapper, and the F1 menu on the phone

Three things the phone asked for, all committed (`72d9e0d01`, `283278ddd`, `952ec24f0`).

**A log file.** `<user>/metroid_prime2_port.log` now gets everything the process prints - the
port's lines, Aurora's, SDL's, and what is printed just before an abort - with the previous run kept
as `metroid_prime2_port.old.log`; 64 MiB cap; `MP2_LOG_FILE=0` turns it off, F1 > Testing has the
row, `logfile [0|1]` the console command, and both say it takes effect next launch. On the desktop a
forked copier writes the pipe to the file and to the terminal; on Android the existing
stdout/stderr pump into logcat feeds it, and the file goes to the app's *external* folder
(`Android/data/org.metroidprime2.port/files`) because the private user dir cannot be read without
root - so `adb pull` works. Verified on the desktop (file created, rotation on the second run,
`MP2_LOG_FILE=0` writes nothing) and on the phone (`adb pull` gives a 229-line run log).
The `[aram]` staging lines are now behind `MP2_DEBUG_ARAM=1`: they were 253 of the 405 lines in a
30 s desktop run and thousands on the phone, enough to bury anything worth reading.

**The automapper.** The minimap drew as a fan of stretched lines over the whole view, and opening
the map screen crashed the phone in `aurora::gx::texture::resolve_static_texture`. Two causes:
`CMapArea::PostConstruct` converts the area's vertex floats to host order but `Draw` then used the
3-argument `CGX::SetArray` (the disc path: size 0, data marked big-endian), so Aurora uploaded
nothing or swapped it again - the vertex-array gotcha in AGENTS.md; the area vertices and the door
corners in `CMappableObject` now go through `SetArrayNative` with their real sizes. And
`CInGameGuiManager::DestroyAreaTextures`, then `CTexture::UnloadBitmapData` itself, freed texture
data while Aurora's FIFO thread was still resolving texture objects - `GXDrawDone()` before the
`CARAMToken` is dropped covers every free in one place. Verified on the desktop (minimap correct in
areas 0 and 5, map screen renders and closes, upstream text identical with TARGET_PC blocks
stripped) and on the phone (minimap correct, map screen no longer crashes).

**The F1 window on the phone.** It had no touch path at all, which is why the overlay carried no
MENU pill. Now it does: a MENU pill (in every layout, saved and movable like the rest) toggles the
window, and while it is open every touch except the pill is handed to SDL - whose touch-to-mouse
path is what ImGui reads, the way Metroid Prime 1's port does it - so coordinates and click timing
are SDL's own. Injecting ImGui events from Java was tried first and got both wrong. The pill is
hit-tested against the rect from its last draw, two fingers scroll the list (`nativeDebugWheel`),
and a drag on the window's empty space scrolls it too, which is why the window is no longer
movable. Verified on the phone: MENU opens and closes, a tap toggles the row under the finger
(Log file), a drag scrolls to the Saves section, and the map screen opens without the crash.

**The gray triangle** (`4c834b5ad`): not the map screen at all, though that is where it looked like
it came from - the Landing Site's fish swarm skins each boid on the CPU from the model's position and
normal arrays, and those stay in disc (big-endian) order in memory because the GPU's guest array path
swaps them itself. Read as host floats they are NaNs and values near 1e6, so every boid had garbage
vertices: a ball of strands on the desktop, one huge gray triangle on the phone's GPU (which
rasterises non-finite vertices lavapipe drops). That also explains the user's two clues - turning the
camera away cannot fix geometry that is rebuilt wrong every frame, and changing area drops the
swarm's model. `SwarmRenderHelpers.cpp` now swaps positions and normals, and
`CSkinnedModel::GetSkinnedPosition/GetSkinnedNormal` - what `CAnimData`'s bounds and
`CActorModelParticles`' emitters read - get the same swap, since they read the same arrays.
Verified on the phone: gameplay and the map screen are clean, the swarm draws as small white specks,
no crash in the run. Still open there: the Landing Site's radial "strand ball" is a separate effect
that has not been checked against retail, and the touch control set is still M3's (MP1's missile,
L/R buttons and triggers, beam/visor wheels, pointer menus for the game's own screens).

## 2026-10-18 (eleventh session): save tools (import/export/backup), and what of the slice does not fit

The fifth slice `docs/QOL.md` listed, taken as far as MP2's own types carry it: the card's save file
can now be read, exported, imported back and backed up before every write. MP1's other two pieces -
save states and quick resume - **do not fit this tree** and are named below rather than faked.

**The format, as it actually is.** `CMemoryCardSys::CCardFileInfo` is the authority (its read is
`StartRead`/`PumpCardRead`, its write `WriteFile`/`BuildSaveSlot`), and a save file is:

- 0x40 bytes: the GCI directory entry (game `G2ME`, maker `01`, name `MetroidPrime2`);
- 0x2000 bytes: the header block (comment, banner, icons, CRC) - irrelevant to the tools;
- two copies of `mAlignedUserDataSize` (0x2000 for this game), each a `uint` CRC then a `uint`
  generation (`BuildSaveSlot`), then 0x1ff8 user bytes;
- in the user bytes: `SSaveHeader` (signature, save index, three present flags, the `SAVH` marker -
  15 bytes), 192 bytes of system options, one 32-byte game-options block per slot, 32 bytes of
  global options (0x14f in all), then one `SGameFileSlot` (0xa38, starting with `GMST`) per present
  slot. `CMemoryCardDriver::ReadFinished` reads exactly this, and slot listing hands each buffer to
  the game's own `CGameState::LoadGameFileState`, so `save info` prints what the file select would.

**Two endiannesses, decided by the CRC.** The CRC and generation are written through a `uint*`
(`BuildSaveSlot`), so a save this port wrote has them **host-endian**, while one a console or Dolphin
wrote has them big-endian. `Inspect` computes the CRC over `block + 4` (the same call the writer
makes) and accepts the copy if the stored word matches either order; the order that matched is also
how the generation is read. The newer copy is then chosen by the greatest stored slot timestamp, not
by generation: the generations alternate 0/1 by construction, so the greater number is not the newer
copy (a save written during this session left generation 0 in one copy and 1 in the other, and the
1 was the new one only because the previous save happened to be 0).

**What is ported.** `platform/port_save.cpp` + `platform/include/port_save.h`:
`Directory`/`ImagePath`/`IsImage` (Aurora's `aurora_card_get_path`/`aurora_card_get_type`),
`Files` (the card folder's GCIs, by directory entry rather than file name), `Inspect`, `Export`,
`Import`, `Backup`/`Backups`/`BackupsDirectory`, and `BeforeCardWrite`. The console grows one
command with subcommands - `save [info] [<file>]`, `save export [<path>]`, `save import
<path|latest>`, `save backup`, `save backups` - and the setting is `MP2_CARD_BACKUP=1`, F1 > Saves
("Back up the card"), `cardbackup <0|1>` on the console, hooked at the top of
`CMemoryCardSys::CCardFileInfo::WriteFile` under `#ifdef TARGET_PC`, which is the one place both a
first create and every later write pass through. Backups, exports and replaced files live in
`<user>/save-backups` (`card-<stamp>.gci`, `export-<stamp>.gci`, `replaced-<stamp>.gci`); the newest
20 `card-*` copies are kept. An import never deletes: the file it replaces is copied to
`replaced-<stamp>.gci` first, and Aurora is remounted (`aurora_card_remount`) so the front end sees
the new file.

**Verified on the desktop** (`MP2_USER_PATH` a copy of the phone's card, `MP2_SKIP_FRONTEND=1`,
`MP2_CONSOLE=4780`, `tools/mp2con.py`):
- `save info` printed `copy 2 of 2 (generation 1)`, `slot 0: world 3bfa3eff, 0% items, 0% scans,
  00:00:02, 99 energy, 0 tanks`, matching the boot's own `[skip] card slots 0=3bfa3eff 0% 00:00`
  (two independent readers agreeing on the same file); `save info <old-backup>` printed copy 1 of 2,
  generation 0, 00:00:00.
- `save export` produced a byte-identical file (`cmp`), `save export <dir>/` kept the card's own
  name, and `save import latest` replaced the card file with a `replaced-*.gci` kept and the card
  unchanged afterwards (`cmp` after the round trip).
- `save backup` and `save backups` listed the copies; with 27 `card-*` files present a backup pruned
  to the newest 20 (oldest removed).
- The write hook: `MP2_INPUT="STATION@100,A@220+3,..."` with `MP2_CARD_BACKUP=1` fired
  `[save] card backup before the write to 'MetroidPrime2': .../card-20261018-093126.gci` on the save
  the save station's own A press made, and the card's copy 2 came back with generation 1 and a newer
  timestamp. Note that this A press leaves the run **paused in the save screen**, where no game
  update runs: the console stops answering (30 s, `[harness] no game update`), which is the same trap
  the sweep notes describe.
- Gates: `tools/gen_files_cmake.py --check` rc=0; the upstream matching build
  `Game Code: 94.14% fuzzy, 63.29% matched` rc=0; the only upstream file touched,
  `src/Kyoto/DolphinCMemoryCardSys.cpp`, strips to its `HEAD` non-PC text exactly (checked with a
  `#ifdef TARGET_PC` stripper, not by eye).

**What does not fit, and why (this is the part worth not inventing):**
- **Save states** (`port_savestate.cpp`, 1200 lines in MP1): a whole live world - actors, colliders,
  effects, pools - serialised and restored. MP2's `CGameState::PutTo` is the *save* state (world
  layers, player state, options), not the runtime; there is no equivalent serialiser to build on, so
  this stays the largest single item and is not started.
- **Quick resume** (MP1: a snapshot written on quit, loaded at launch): it is a savestate with a
  trigger, so it inherits the same wall. MP2's resume today is the card's own save, which
  `MP2_SKIP_FRONTEND=1` boots straight into (slot from the card header, or the first non-empty one) -
  that is the honest MP2 answer until savestates exist.
- **Region conversion** (MP1's `port_save_convert`): MP1 converts GM8E/GM8P because both releases
  share a save layout. MP2's save carries the world table's `SAVW` hashes and area/layer data that
  differ by release, so `Inspect` recognises `G2ME`/`G2MP` and *refuses* a foreign one with a clear
  error instead of converting it.
- **Raw card images**: Aurora may mount a raw image instead of a GCI folder. `Import` handles that
  (`aurora_card_raw_insert` + remount), but `Files`/`Inspect`/`Export`/`Backup` need paths, so on an
  image they report "the card has no save file". Neither this desktop nor the phone uses one.
- **Slot-level tools** (copy one save slot over another, erase one, rename a file): not done. It
  means rewriting the user data, both CRCs and the present flags offline; the front end already does
  it, and a port copy would have to reproduce its option-block shuffling. Next slice if wanted.
- **Dolphin-emulator and Trilogy-save folders**: MP1-specific discovery and formats; MP2's own save
  formats there are unknown, so nothing was guessed.

## 2026-10-18 (tenth session): gameplay tweaks

The fourth slice `docs/QOL.md` listed, and the first one with game-side hooks in *four* upstream
files: fast morph, spring ball, toggle lock-on and sticky charge. MP1 has all four, but its shape did
not transfer - MP1's morph transition is one duration scalar and its ball jump is a C-stick path in
`CMorphBall`; MP2's morph is `mMorphTime`/`mMorphDuration` set from four places in
`CPlayer::UpdateMorphBallState`, and its C-stick is read by the ball camera's aim code, not the ball.

**The settings.** Four statics in `platform/port_overlay.cpp` with `EnvFlag` reads for
`MP2_FAST_MORPH`, `MP2_SPRING_BALL`, `MP2_LOCK_ON_TOGGLE`, `MP2_STICKY_CHARGE`, getters/setters in
`platform/include/port_overlay.h` (plus `kFastMorphDuration` 0.2 s and `kStickyChargeAfter` 0.35 s),
one F1 > Gameplay row each with its env var and a one-line reason, and four console commands in
`CmdGod`'s shape - no argument toggles, an argument sets, so a headless run drives them one update
later. Nothing is saved; there is still no settings file.

**Fast morph** - `CPlayer::UpdateMorphBallState` computes `morphDuration`/`unmorphDuration` (1 s, or
0.6/0.5 s in multiplayer) and two screw-attack durations from `gpTweakBall`; the hook shortens the
first two when the option is on and leaves the screw attack's own transition alone, because those
are a different move with their own authored times. `mMorphTime` is also the morph factor the body
and camera interpolate on (`CPlayer.cpp`, lines ~3225 and ~3356), so the shorter duration shortens
the whole transition rather than cutting it off. Measured on the desktop by polling `status` in real
time around an `X` key press (a batch of console commands is *not* one command per update - the
console drains the queue within one update, which is what made the first attempts read nonsense):
morph 0.80 s off, under 0.1 s on, unmorph 0.80 s off and instantaneous on.

**Spring Ball** - `CMorphBall::ComputeBallMovement`, which the C-stick had no claim on: the ball
camera interpolates its own look-at and never reads `ARAUp`, and `PortMouse`'s twin-stick path is off
while morphed (`CPlayer::MouseAimAllowed`). The gate is `mBallState == kBS_Normal`, on the ground, not
morph-transitioning, movement allowed, not attached to an actor, Morph Ball Bombs owned, and 40
frames of cooldown (randomprime's number, in seconds). It calls the game's own
`CPlayer::BombJump(GetTranslation(), mgr)` and then does the two things MP1's version does around it:
it puts back the horizontal velocity `BombJump` zeroes (it is written for a bomb going off *under*
the ball, and a spring jump that stops the roll feels broken) and sets `kMS_FallingMorphed`, which is
what tells the physics the ball has left the ground on the tick it was still standing on it. Verified
by reading `status`'s z every 40 ms around `cstick 0 127 20`: -48.44 -> -45.04 and back with the
option on; not a millimetre with it off.

**Toggle lock-on and sticky charge** - MP1's two state machines, taken verbatim into
`platform/include/port_hold_toggle.h` (`Toggle` with its debounce and its "a latch that loses its
lock lets go", `Sticky` with its 0.35 s threshold and its swallowed release), with
`tests/port_hold_toggle.cpp` ported too and registered as `port_hold_toggle_tests` beside
`port_touch_pad_tests`. They are applied in `CStateManager::ProcessInput` to
`mFinalInputs[controller]` right after the pad has been read: the helper finds the player whose
`GetPlayerSelection()` is that controller (a controller index is not a player slot - `ProcessPlayerInput`
maps it the other way), and does nothing when input is disabled so a cutscene or menu resets both.
Two `PortSetL`/`PortSetA` setters in `CFinalInput` write the bits back; L sets the digital click *and*
the analog trigger, because MP2's orbit command can be bound to either and both are L on the pad.
Verified in area 11 (warped there with the console: area 0's Temple Grounds room has no `CPatterned`)
with two Dark Troopers in view. With the toggle on: holding L for 0.6 s gives `orbit 1` during the
hold and **both stay** after the key is up for as long as it was sampled (2.6 s); a tap then reads
`orbit 0, L=0` and does not re-lock; the next tap reads `orbit 1, L=1` again. With it off, L is a
plain hold (orbit 1 only while held). Sticky: a 0.6 s A hold then release leaves `A=1`; a tap
releases it (that is the charged shot) and the bit drops; with the option off the bit drops at
release.

**Review pass.** A fresh-context read of the commit found no functional bug and two soft spots,
both fixed: the state machines are shared, so a second controller that owns no player would have
reset pad 0's latch every tick (the helper now ignores a controller with no player), and the
latches were live through the scan display's soft pause (they now need `kGS_Running`). The second
one is deliberately narrow: the pause and map screens stop game updates altogether, so nothing can
run there and a latched L resumes with the game - the next press lets go. Spring Ball's side
effects (the velocity restore, `kMS_FallingMorphed`, the cooldown) would also apply on a `BombJump`
that returned early, but `mBombJumpState` is never set to `kBJS_BombJumpDisabled` in this tree, so
that branch is unreachable today.

**Instruments this slice needed.** `status` now prints `orbit N  game input L=x A=y` - the game's L/A,
not the pad's, which is the only way to see a latch from outside. The console's virtual pad could
only hold the main stick, so Spring Ball had no headless trigger: `PortInput::Hold` now takes both
axis pairs and `port_console.cpp` grew `cstick <x> <y> [updates]` beside `stick`. `tools/keytap.py`
did the rest - the morph button, which the console names `Y`/`X` for enter and exit, and the L/A
presses.

**Gates.** `gen_files_cmake.py --check` rc=0, `gen_console_names.py --check` rc=0, upstream matching
build `Game Code: 94.14% fuzzy, 63.29% matched` rc=0 (twice: after the lane's edits and after the
line-length pass). Each of the four upstream files was stripped of its `#ifdef TARGET_PC` blocks and
diffed against `git show HEAD:<path>` - all four identical, including the blank lines, which is why
the guarded blocks sit tight against the preceding brace rather than after a blank line.

**Not done / left open.** MP1's spring ball also fires on a gyro flick and a bound "spring ball"
action, and halves the jump inside a half pipe; none of that is here. The lock-on latch's
"target dies -> let go" branch is unit-tested only: it needs a kill, which the console cannot do
without a message id, and `kill` kills the player. Sticky charge is gated on owning Charge Beam
(`kIT_ChargeBeam`), which the test save has. The half-pipe interaction above is likewise unverified:
`kBS_Normal` and `OnGround` together looked unreachable there, but nobody drove a half pipe.

Re-verified after the review fixes on the same desktop run (area 11, a Dark Trooper in view): a
0.6 s L hold reads `orbit 1, L=1` and keeps both for the rest of a 2.4 s sample, a tap releases it,
and sticky A stays latched through 1.6 s of sampling with the beam visibly charging in a screenshot
(`/tmp/opencode/aim2.png`).

## 2026-10-18 (ninth session): keyboard bindings

The input slice `docs/QOL.md` listed third: rebinding over Aurora's pad key tables, with the F1
editor row, `bind`/`binds`, and one file under the user directory. MP1's `port_input_settings.cpp`
was the reference only for the shape of the editor - its settings model (profiles, toml, action
binding, touch layout) is far wider than this tree's, so the take is the rebinding part alone.

- **`platform/port_binds.{h,cpp}`** own the map. The table is 22 rows - the 12 pad buttons
  (A/B/X/Y/START/Z/L/R/UP/DOWN/LEFT/RIGHT) then the 10 stick inputs (`STICK_*`, `CSTICK_*`,
  `TRIGGER_L`, `TRIGGER_R`) - each with the port's built-in key, the pad input it drives and the
  action name the console and the file use. `Install()` replaces the literal `port_events.cpp` used
  to hold; `PADSetKeyButtonBindings`/`PADSetKeyAxisBindings` take the whole table on every change,
  because it is 22 tiny calls and tracking which row moved buys nothing.
- **`keybinds.cfg`** beside the memory card and `touch.cfg` (`MP2_USER_PATH`, else Aurora's
  prefer-path): `<ACTION>=<key>` per row, keys as SDL scancode names ("X", "Left Shift", any case)
  or `MOUSE_LEFT/MIDDLE/RIGHT/X1/X2`, `none` to unbind, `#` comments. Missing file = the built-in
  layout (deleting it is the reset), unknown rows and keys are skipped one log line each, and writes
  are atomic (tmp + rename). Read once on the game thread's first frame: the key names are SDL's, so
  it cannot be read in `main` before Aurora is up - `PortBinds::Initialize` only stores the path.
- **Aurora's own `keyboard_bindings.dat`** is neither written nor relied on. It is binary, has no
  names and Aurora loads it before the port's map replaces it; `keybinds.cfg` is the port's file.
- **F1 > Bindings** shows all 22 rows with their keys, two rows side by side (buttons left, stick
  inputs right, `SizingFixedFit` + `RowBg`). Clicking a row's key button arms it - the button reads
  "press a key..." with the active colour and the tooltip follows - and the next key-down the event
  pump sees is bound and saved. `PortBinds::OnKeyDown` returns true while a capture consumed the key,
  so the pump `continue`s past its hotkey switch: binding F5 does not warp the world on the way in.
  `Escape` cancels, a second click on the armed row cancels, closing the window disarms (`Draw`'s
  not-visible path), and "Reset to defaults" rewrites the file. A port hotkey still binds; the log,
  the console reply and the window all say the hotkey will fire with it.
- **Console**: `bind <action> <key>` (the key is the rest of the line, so `bind START Left Shift`
  needs no quotes), `bind reset`, and `binds` (the table plus the file's path). `Set` returns whether
  the file was written, so the reply distinguishes "saved" from "in memory only".
- **`tools/keytap.py`** is the instrument this slice needed: XTEST key presses on a display
  (`XAUTHORITY` unset, since a bare Xvfb needs none). `MP2_INPUT` and the console's `press` write the
  *virtual* pad, past the keyboard map, so nothing else in the tree can test a binding. A `--hold`
  of 20 ms was too short to be seen: SDL can process the press and the release in one frame's event
  queue, and the pad then reads the key as up. That looked exactly like "the binding did not work"
  and cost the first pass at the pause-screen test.
- **Verified on the desktop** (`MP2_TELEPORT=0`, area 0, `MP2_CONSOLE=4780`, the run driven from
  `tools/mp2con.py`, `tools/keytap.py` and an XTEST click helper; screenshots `/tmp/opencode/b*.png`
  and `c*.png`): `binds` lists the built-in layout and the path; `bind A g` saves `A=G` in the file
  and applies (`bind START Left Shift` proves the joined multi-word name, `bind A none` the unbound
  row, `bind reset` the whole table); `bind nosuchrow x`, `bind A nosuchkey` and bare `bind` answer
  the error text; the F1 row arms on a click, captures `A=G` with the tooltip following, Escape
  leaves `X=C` in place and Reset restores every row (the file's `STICK_UP=U` gone); `STICK_UP=u`
  walks forward and `W` then does not; after a restart `[binds] ...: 22 bindings` and the loaded
  `STICK_UP=U` still walks; `bind START p` then a real `p` opens the pause screen (the Inventory/
  Options/Data Network screen in `c4_pause_p.png`).
- A fresh-context review found no functional defect and three small ones, two of which went in: a
  UTF-8 BOM on `keybinds.cfg`'s first line is skipped (an editor writes one, and it otherwise lost
  exactly the first binding), and a key SDL cannot name (scancode 0: some media/Fn keys) is refused
  while the row stays armed, since it would be saved as `none` and be gone after a restart. The
  third - the F1 capture sees key-downs only, so a mouse button is bound from the console - is a
  hint in the window rather than code: a click that armed or pressed a row would otherwise also bind.
- Gates: `tools/gen_files_cmake.py --check` rc=0, `tools/gen_console_names.py --check` rc=0, the
  upstream matching build `Game Code: 94.14% fuzzy, 63.29% matched` rc=0. Every changed source is a
  platform file, so no upstream file changed and no `#ifdef TARGET_PC` block was needed.

## 2026-10-18 (eighth session): the rest of the graphics toggles

Three settings, the shapes `MP2_FOV` and `MP2_NO_HELMET` established: a field in
`platform/port_overlay.cpp` with its `MP2_*` env var and F1 row, a console command so a headless run
can drive the same thing, and one `#ifdef TARGET_PC` hook per upstream file.

- **Arm cannon offset** (`MP2_GUN_OFFSET="<right>,<forward>,<up>"`, `gunoffset <r> <f> <u>`, F1 >
  Render). Applied in `CPlayer::UpdateArmAndGunTransforms` where the game adds the tweak gun
  position: the user offset goes into `gunOffset` (so the shots leave from the moved muzzle) and into
  `grappleOffset` whenever the game is placing that arm itself. The morph-ball branch is untouched -
  it uses its own fixed offset. The overlay's three sliders plus "Reset arm cannon"; values are
  clamped to +-1 m and a non-finite one becomes 0, because the value lands in a transform.
- **MSAA and anisotropic filtering** (`MP2_MSAA=1|4`, `MP2_ANISO=1..16`, `msaa` / `aniso`, F1 >
  Render). Aurora took both only in `AuroraConfig` at init, so the vendored tree gained MP1's shape:
  `aurora_set_graphics_quality` queues `msaa << 16 | anisotropy`, `begin_frame` hands it to
  `webgpu::set_quality`, which synchronises, drops the bind-group caches (they hold samplers built for
  the old anisotropy) and rebuilds the framebuffers at the current size when the sample count changed.
  `PortOverlay::Draw` sends it only when the values changed, so an untouched port issues nothing.
  Anisotropy needs no rebuild: samplers are cached by their descriptor, which contains the anisotropy.
  The port's EFB read-back already reads the *resolve* target, so the scan visor and the camera
  filters survive 4x MSAA (checked with `visor scan`; `scan_peek` still reports the EFB as
  multisampled, which is its own older instrument, not the copy path the port uses).
- **HUD scale** (`MP2_HUD_SCALE=50..100`, `hudscale`, F1 > HUD). The whole HUD is scaled about the
  screen centre, rather than MP1's per-element "shrink toward the nearest edge", because MP2's GUI
  frame has no per-widget spread machinery to hang that on. `CSamusHud::Draw` sets a flag for the
  length of its own body (an RAII scope, so every early return clears it); `CGuiCamera::Draw` scales
  its orthographic extents - or widens the perspective FOV - by `100/percent` while it is set. The
  HUD frame, the radar and the scan interface all draw through a `CGuiCamera` inside that body, so
  they scale together; the helmet, memo screens, pause screen and inventory draw outside it and keep
  their size.

**Verified on the desktop** (`MP2_TELEPORT=0`, area 0; screenshots under `/tmp/opencode/qol_*.png`,
`v2_*.png`): the gun moves with `gunoffset 0.3 0.2 -0.1` and returns with `gunoffset 0 0 0`; the HUD
at 70% shrinks toward the middle with the helmet frame unchanged; `msaa 4` and `aniso 1` log
`Graphics quality: 4x MSAA, ... anisotropy` and the frame is unchanged apart from its edges; all four
from the env at start-up (`MP2_MSAA=4 MP2_ANISO=8 MP2_HUD_SCALE=80 MP2_GUN_OFFSET="0.25,0.2,-0.1"`)
apply before gameplay; the F1 rows read those values back; and `MP2_HUD_SCALE=60` with `MP2_INPUT=START@400`
draws the pause menu at the authored size. Gates: `gen_files_cmake.py --check` rc=0, matching build
`Game Code: 94.14% fuzzy, 63.29% matched` rc=0, and `CPlayer.cpp`, `CSamusHud.cpp` and
`CGuiCamera.cpp` strip to their pre-change non-PC text exactly.

## 2026-10-18 (seventh session, continued once more): hide helmet, hide visor effects

Two more graphics toggles, both env-only plus a checkbox in the overlay's HUD section:
`MP2_NO_HELMET=1` (the world drawn without the helmet frame) and `MP2_NO_VISOR_FX=1` (without what
`CPlayerVisor::Draw` paints for the active visor). The helmet is gated where the game already gates it,
`CInGameGuiManager::Draw`'s `mHelmetVisMode != 0 && notInCine` branch; its non-PC body is repeated
under `#else` rather than wrapped, because an added `&&` would change the matching build's bytes for
it. The visor hook is an early return after the light setup in `CPlayerVisor::Draw`, so the draws
after it keep the state they expect. Echo and dark visor effects are empty stubs in this tree, so the
second flag currently suppresses the scan visor's own frame and edges - the scan *window* is the
HUD's scan interface and stays, which is what a screenshot with both flags shows.

Verified on the desktop, each against the default frame: helmet gone with everything else intact
(`/tmp/opencode/hud_nohlm.png` against `fov_55.png`), and the scan visor with `MP2_NO_VISOR_FX`
(`/tmp/opencode/hud_nofx_scan.png`) keeps the scan window and the tint while the visor's own framing
is not drawn. Gates: `gen_files_cmake.py --check` rc=0, matching build still 94.14% / 63.29%, and
both upstream files strip to their pre-change non-PC text exactly.

## 2026-10-18 (seventh session, continued again): the first-person FOV setting

The first of `docs/QOL.md`'s graphics toggles: `MP2_FOV=<deg>` (F1 > Render > "Field of view (1st
person)", 0 keeps the game's 55). The first-person camera takes its FOV once, when it is built
(`CCameraManager.cpp:108` reads `mFirstPersonFov`), so a live change is applied in
`CCameraManager::UpdateCameras` (`#ifdef TARGET_PC`): set `mFirstPersonFov` (what a rebuilt camera
reads) and `CGameCamera::SetFovAndTarget` on the camera in use, skipping a cinematic camera, which
animates its own FOV from its spline. Verified on the desktop with `MP2_FOV=90`: `status` reports
`camera first-person fov 90.0` and the frame is visibly wider
(`/tmp/opencode/fov_55.png` against `/tmp/opencode/fov_90b.png`).

**A documentation error fixed on the way:** `docs/QOL.md` said the F1 overlay's settings live in
`platform/debug_ui.cpp` and are saved to `port_settings.ini`. They do not: `debug_ui.cpp` is in no
CMake target at all (it is Metroid-Prime-shaped, like `PortBoot.cpp`), so a setting added there
compiles nowhere and fails to link (which is exactly how this was found). The live overlay is
`platform/port_overlay.cpp`, and it keeps no settings file - every knob is an `MP2_*` env var.
Both files' docs are corrected.

## 2026-10-18 (seventh session, continued): `MP2_TELEPORT` at boot rendered nothing

**The white screen was a scripted camera filter left applying its flash.** A boot-time
`MP2_TELEPORT=<area>` landed the player correctly and the game ran (60 fps, 210 draw calls, 192k
vertices a frame) but the screen was a flat white or black - the HUD drawn over it, no world, no
gun. It was not area 5: the same hold appeared after warping back to area 0, and the same area 5
*reached from gameplay* rendered perfectly. The finding that named it: an `MP2_DEBUG_FILTERS=1`
instrument in `CCameraFilterPass::DrawFilter` printed an opaque white `kFT_Add` full-screen filter
every frame, and a gdb breakpoint on it led to
`CStateManager::DrawAdditionalFilters` -> `mCameraFilterPasses[player][0].Draw()`: one of the 11
per-player camera-filter passes, which only a level script drives (`CScriptCameraFilterKeyframe`,
`kSM_Increment` sets the filter over `mTimeIn`, `kSM_Decrement` takes it away over `mTimeOut`).
`send <keyframe> Decrement` to the area's white keyframe made the world appear instantly.

**Fix: the boot request waits for gameplay, which is the state a console `warp` runs in.** Served at
frame 0 - `mGameState` is already `kGS_Running` and `IsFullyInitialized()` already true during the
save's own load, so state alone cannot tell the two apart - the warp's hand-over to the game's own
transition left the destination's scripted flash applied. It is now held until
`PortHarness::GameUpdates() >= 120` (the point the harness itself calls "gameplay reached"), and
`[warp] serving a request at frame 120` says where it went. `PortWarp::PendingIsBoot()`
(`platform/include/port_warp.h`) is what the game asks; `MP2_INPUT` tokens and console warps are not
boot requests and keep their exact timing. Also on the arrival path, a warp now ends a
`kTT_Portal` transition the hand-over may have left running (it never fired in these runs; a warp is
a teleport, so no portal wipe has anything left to do).

**Verified:** `MP2_TELEPORT=5` at boot now renders the area (screenshot, `/tmp/opencode/dfr2.png`);
`MP2_TELEPORT=5` plus `MP2_INPUT=NEXTAREA@900+1` still hops to area 6 at frame 899 and area 6 renders
(`/tmp/opencode/sm1.png`); `MP2_TELEPORT=0` is unchanged. Instruments added while hunting, all
`#ifdef TARGET_PC` and off unless set: `MP2_DEBUG_VIS=1` (what `CStateManager::GetVisSetForArea`
decided, per area), `MP2_DEBUG_TRANS=1` (`CMFGame`'s flow state, transition phase and fade),
`MP2_DEBUG_FILTERS=1` (every full-screen camera filter with its colour). `status` grew the camera
(kind, FOV, position), the area's fog, the alive/visible area list with the current area's occlusion
state, the escape timer, the world transition type, the viewport, and the frame's draw calls and
vertices - each of those was a question this hunt had to answer.

Gates: `tools/gen_files_cmake.py --check` rc=0 and the upstream matching build still
`Game Code: 94.14% fuzzy, 63.29% matched`; the four upstream files touched
(`CStateManager.cpp`, `CMFGame.cpp`, `CCameraFilter.cpp`, and the earlier `CFilteredDelayAux.cpp`)
strip to their pre-change non-PC text exactly.

**The console's fourth wave** (`platform/port_console.cpp` only, no upstream file): `world <id>
[area]` is MP1's `warp <world>` under its own verb (this tree's `warp` is the area warp the sweeps
use, and renaming it would break them), `worlds` lists the memory card's world table - id, area
count, and the STRG asset holding the name - and `strg <id|name> [index|name]` reads a string table
by asset id or resource name. Two things `strg` needed: the token has to outlive the command (the
load is asynchronous, so a token that dies with it cancels it, and every call answers "still
loading"), and the wide strings are narrowed to ASCII for the socket. Verified on the desktop:
`worlds` (12 worlds, Temple Grounds current), `strg 0x7f47f635` -> "Sanctuary Fortress",
`strg STRG_AllPickupsFound_2` -> "100% of all pickups acquired!", and `world 1baa96c2` switching
Temple Grounds -> Sanctuary Fortress (67 areas, area 0 renders).

## 2026-10-18 (seventh session): the console's second wave, and a client that had never worked

**Seven commands, and the names they print come from the enums.** `platform/port_console.cpp` gains
`objs [filter]`, `obj <id>`, `send <id> <msg>`, `visor [name|0-3]`, `beam [name|0-3]`,
`tp <x y z>` and `items` (the list is in `kCommands`, the help block and `platform/include/
port_console.h`). Three details carried the work:

- **Class names are RTTI**, demangled with `abi::__cxa_demangle` (`typeid(ent).name()`), the same
  shape MetroidPrimePort's console uses - this tree has no per-class name table and does not need
  one. The scan prints `CScriptDoor`, `CPlayer`, `CScriptRelay`, ...
- **Message, state and item names are generated**, not typed: `tools/gen_console_names.py` writes
  `platform/console_names.inc` from `EScriptObjectMessage`/`EScriptObjectState` (CEntityInfo.hpp) and
  `EItemType` (CPlayerState.hpp), keeping only explicit initialisers, first name wins on a duplicate
  value (`kSS_APRC`/`kSS_Approach`), values at or above `0x80000000` are the negative ones and are
  skipped. `--check` is a gate like `gen_files_cmake.py --check`'s, so an upstream sync that adds a
  message shows up as a stale file rather than as a mislabelled connection.
- **Ids are hex, and both of the ids a session reads resolve.** `objs` prints `TUniqueId::value`
  (`u046e`) because `Value()` masks to 10 bits and two objects can share it; the argument is parsed
  as hex (with or without `u`/`0x`), and `obj`/`send` fall back to the *unique* object whose
  `Value()` matches, so the id in a port log line (`[door] 006e got kSM_Open`) can be typed straight
  in. Ambiguous matches are refused by name.

**Visor and beam switching go through the game's own paths.** `visor` calls `StartTransitionToVisor`
and then the holster/draw the game does after a switch (`CPlayer::UpdateVisorState`,
`CPlayer.cpp:1330`); `beam` calls `CPlayerState::SetCurrentBeam`, which `CPlayerGun::HandleWeaponChange`
reads on the next update (`CPlayerGun.cpp:2555`) - the same path the `kIT_SwitchWeapon*` items take.
Neither forces a visor or beam the save does not own: it answers `=> err: ... not owned`, because a
switched-to beam the gun will not draw is a confusing state, not a useful one.

**`tools/mp2con.py` hung on every command in its default mode, before this session.** It read until
the terminator *and* kept reading because the `break` sat inside `if quiet:`; the port keeps the
connection open until the client hangs up (one client at a time), so a non-quiet run always ended in
`TimeoutError`. It now counts replies (`=> ok`/`=> err:` lines) and stops when the last command has
answered, and reports a bare timeout as "no reply from 127.0.0.1:<port>" instead of a traceback. The
console's own commit (`0e267e186`) was verified with `nc`, which has the same "wait for EOF" shape
and hid this.

**Verified on the desktop**, `MP2_CONSOLE=4780`, `MP2_TELEPORT=0` (area 0), `MP2_GODMODE=1`:
`objs door` (7 doors, class names demangled), `objs player`, `obj u046e` / `obj 046e` / `obj 0x46e`
(one door, three spellings) and `obj 400` -> `u0400 CPlayer hp 99/99`, `send u046e open` ->
`[door] 006e got kSM_Open` (and its partner door `01b5`), `visor scan` -> screenshot with the scan
tint, the scan window and the gun holstered, `visor combat` -> screenshot with the gun back, `tp`
8 m with the facing kept (screenshot plus `status`), `items` (11 lines), and the refusals:
`beam light`/`visor echo` not owned, `send ... NosuchMessage`, `obj 9999`.

**One thing found on the way, not caused by the console:** a `MP2_TELEPORT=5` boot lands the player
in area 5 at the area's first spawn point and renders *nothing* - white screen, the HUD's map markers
only, no energy bar and no gun - while `[ids]` counts and `status` are normal (`235 objects`, health
99). `MP2_TELEPORT=0` in area 0 renders correctly from the same card, and a fresh area-5 boot that
never touched the console shows the same white frame, so the console is not involved. Next session's
first look: whether the area's geometry paks are loaded after that warp at all (`MP2_DEBUG_AREA=1`,
and the `[resloader]` warnings).

**The console's third wave: the pad, the inventory, and a bow towards the camera.** `press <a+b+...>
[updates]` and `stick <x> <y> [updates]` hold buttons and the main stick on the same virtual pad
`MP2_INPUT` writes (`platform/port_events.cpp` grew `PortInput::Hold`, in `port_input.h`; the hold
merges into the script's state, so the two can be held at once, and a run with neither leaves pad 0
to the real controller exactly as before). `give <item> [n]` / `take <item> [n]` take `items`' own
names and go through CScriptPickup's path (`AddPowerUp` + `IncrPickUp`, so an energy tank heals and a
suit upgrade drops), `face <yaw deg>` turns the player through `CPlayer::Teleport` (0 = +y, 90 = +x),
and `screenshot` asks Aurora for the F12 capture and says where the log names it. Verified:
`stick 0 127 120` walked the player 5.6 m (position before and after), `press x 30` entered the morph
ball and the next one left it, `press a 40` fired, and — the regression that matters — an
`MP2_INPUT=DOOR@900+1` script still fired its token while console holds were in use, so the two
share the virtual pad without disturbing the sweep's driver.

**`give` + `visor` found a real crash on the first try: leaving the echo visor ended the run.**
`CPlayer::UpdateEchoVisorEffects` (`CPlayer.cpp:4459`) adds a filtered-delay aux effect while the
visor is up and removes it when it goes down; the removal reached
`ShutdownFilteredDelayAux` -> `CMemory::Free` on buffers that `rs_new` allocated, and on the host
`rs_new` is *the host's* `new[]` (`CMemory.hpp`'s TARGET_PC `#define rs_new new`), i.e. malloc
memory - so the game allocator walked back from a malloc block into its own metadata. ASan named it
in one run: "heap-buffer-overflow, READ of size 8, 56 bytes before 19840-byte region", allocated by
`PrepareFilteredDelayAux` and freed by `CMemory::Free`. Fixed in `CFilteredDelayAux.cpp` with the
port's own idiom for this (`#ifdef TARGET_PC` -> `::operator delete[]`, whose `PortDelete`
(``PortMwccNew.cpp`) routes by address - the shape `CFrameDelayedKiller` and `CDvdFile` already use).
Verified: echo -> combat -> echo -> combat with no fault in the normal build, and the ASan build
reports nothing at all in that sequence.

**Gates:** `tools/gen_console_names.py --check` rc=0, `tools/gen_files_cmake.py --check` rc=0, and the
upstream matching build still `Game Code: 94.14% fuzzy, 63.29% matched` - no `src/` or `include/`
file was touched this session, so the non-PC text is untouched by construction.

## 2026-10-17 (sixth session): the scan visor's desktop crash placed and fixed; the intro text skipped

**The scan visor's device loss was a Z8L depth copy resolved through the colour blit.** Entering the
visor lost the GPU two frames in (`VK_ERROR_DEVICE_LOST`, GPUVM at `0x80011ca000`, `SQC (data)`), and
every earlier attempt to place it failed because it is not in the visor's own drawing:
`CStateManager::CapturePlayerTextures` copies the 32x64 scan-target region three times - the id
buffer as `GX_CTF_A8`, then the same region as `GX_TF_Z16` and `GX_CTF_Z8L` for the depth buffers.
Aurora has depth conversion pipelines for Z8/Z16 but none for **Z8L**, so with a scaled destination
("the port scales copy destinations to the render target") that copy fell into
`tex_copy_conv::blit` - a *colour* passthrough pipeline - with the **depth** view bound to it. An
invalid descriptor, a garbage texture fetch, a GPUVM fault. `RADV_DEBUG=hang` agreed: the failing
draw is a one-sample passthrough fragment shader, and `MP2_NO_BLIT=1` (a knob since removed) made the
run play to its frame limit, which placed it. It is visor-specific only because those copies exist
only there.
- **Fixed on both sides.** The port no longer issues the two depth copies (their only product was the
  player's CPU depth buffers, and `PortScanTarget::FillBuffers` already fills those from the same
  read-back as the id buffer; the textures they resolved into were never sampled), and Aurora refuses
  to route a depth source through the colour blit - it warns once and leaves the target unresolved.
- **Also fixed in passing:** `gfx::find_bind_group` checked its lookup with `CHECK`, which compiles to
  nothing under NDEBUG, so a cache miss returned `end()->second` - a garbage bind group handed to the
  GPU. A lookup now refreshes the entry's frame and a miss logs and returns an empty bind group.
- **Verified:** `MP2_INPUT=LEFT@200+2` enters the visor, the run plays to its frame limit and exits 0,
  and the visor draws correctly - tinted world, scan window with its reticle, HUD
  (`/tmp/opencode/visor_desktop3.png`, the first desktop capture of the scan visor, matching the
  phone). Commit `875a1fa82`.

**MP2_SKIP_INTRO now skips the level-load text too.** A skip-intro run still showed the opening text
cards - "--- Data Confirmation ---", "Last transmission received from a rogue planet ... Aether",
"Mission File 02546" - typed over black, and *that* was what the run waited for: the first game update
at 33.9 s, gameplay at 36.0 s. A new `MP2_DEBUG_IOWIN=1` names the windows on `CIOWinManager`'s draw
stack once a second, which placed the cards in `CMFGameLoader::Draw` -> `CWorldTransManager`: the
*level-load text* `CMFGameLoader` enables (`STRG_IntroLevelLoad`) when the save has no powerups and
the world is the default one - a `kTT_Text` transition, so the loader blocks until it ends.
`PortHarness::IntroSkipLevelText` (true while `MP2_SKIP_INTRO` is armed) stops the loader enabling
either that text or the world-name card an area load shows; the transition still runs and
`UpdateDisabled` finishes it in two seconds, which is what lets the game start.
- **Verified on the desktop, same save:** with `MP2_SKIP_INTRO=1` there are no cards at all, `the
  first game update is at 2.7 s`, `gameplay reached at 4.7 s (frame 121)` (was 33.9 / 36.0 s) and the
  area plays (`/tmp/opencode/u_10.png`); with `SKIP_INTRO=0` the cards are back unchanged
  (`/tmp/opencode/v_10.png`). Commit `876f43f40`. With the cinematic skip and the memo dismissal that
  were already there, a skip-intro run now lands in gameplay with nothing of the intro left.

**Gates this session:** `tools/gen_files_cmake.py --check` clean, and the upstream matching build green
after each of this session's port edits in upstream files (`Game Code: 94.14% fuzzy, 63.29% matched`,
`rc=0`), all of them inside `#ifdef TARGET_PC` with the non-PC text byte-identical.

**A skipped intro cinematic now ends its scene** (`0201ba38e`). Stopping the cinematic camera and
handing over control is not the same end state as the scene playing out: whatever the sequence drives
keeps going under the first-person view. An `MP2_SKIP_INTRO` request now takes the debug setting's
strong path (`sHarnessCinematicSkip`): the scene's own skip first, and when a sequence has no skip
function, the flow *stays* in `kFS_CinematicSkip` - where it already runs at max speed without
drawing - so the sequence finishes by itself instead of being cut off. Camera-only is now the last
resort after 30 s of flow time. Phone: one skip request, `[skip] cinematic 0123: the scene's own
skip`, gameplay at 4.8 s, no second cinematic camera (the previous build re-requested camera 00f4 -
it had handed over control mid-sequence). Desktop unchanged (`the scene's own skip`, 4.8 s).

**Fixed: shooting a door into another room crashed the phone build** (`71f296b71`). The cause was an
**in-place `memcpy` in `CGameArea::DecompressAreaData`**: an uncompressed MREA chunk is expanded where
its source already sits, with the two ranges overlapping by a few hundred bytes (source ahead of
destination). That is undefined for `memcpy`, and which direction a libc copies in decides whether the
area's section data survives - glibc's x86-64 copy goes the safe way, which is why the desktop only
ever showed the *symptom*, a surface pointer in `CCubeModel`'s constructor that is not in the area's
buffers, while the phone (bionic) died. AddressSanitizer on the desktop replay named it: 11
`memcpy-param-overlap` reports, every one in that function, e.g. destination `[0x…5280,0x…9280)`
against source `[0x…5434,0x…9434)`; `memmove` for the port, `memcpy` unchanged for the console (`#else`).
After it: the ASAN build reports nothing on the whole journey, and the phone ran the full route three
times with no crash.
- **The replay recipe, on the desktop** (`USERDIR=/tmp/opencode/mp2card/user`, the same card the phone
  has - the two are byte-identical): `MP2_INPUT=DOOR@300+1,A@320+150,SUP@500+300,DOOR@1000+1,A@1020+400`
  - break the door in front of you, walk through into the neighbouring area, warp to *its* door and
  break that, which is what rebuilds the original area's geometry.
- **If it ever bites again:** `MP2_GUARD_GEOMETRY=1` swaps a surface pointer that is not in the area's
  mapping for the model's first surface (a wrong-looking model instead of a dead process), and
  `MP2_DEBUG_AREA=1` prints what it found.

**Previously (instrument, now explained): shooting a door into another room crashes the phone build.** `CCubeModel::CCubeModel` reads a
surface pointer that is not in the area's buffers while `CWorld::PropogateAreaChain` adds the newly
visible area's static geometry (fault `0x72c1753804`, surface `0x72c17537f8`, every section the fill
logged at `0xb4...`). Reproduced on the phone in ~35 s with `MP2_INPUT=DOOR@300+1,A@320+400` from the
save; the instrument is `MP2_DEBUG_AREA=1` (`CGameArea::AddStaticGeometry`/`FillInStaticGeometry` and
`CCubeRenderer::AddStaticGeometry` log the area, the ARAM transfer result, whether the models were
constructed, and the section/surface pointers, naming an out-of-buffer one), plus `MP2_DEBUG_DOORS=1`
and `MP2_WARP_DOOR=<id>` for `DOOR@frame` (commit `41a55178d`). The crash log names the failing fill:
the *original* area's second `FillInStaticGeometry` (`aram=1 constructed=0 instances=1`).
- **Ruled out: the in-place re-conversion.** That path converts the surface sections in place on every
  fill (`CCubeSurface::ConvertSurfaceHeader` and the surface-group halfword swap), so a second fill of
  already-converted buffers would double-swap them - but the second fill read the *same* surface count
  as the first (1858), so the buffers had been re-read from ARAM and were fresh. No double conversion.
- **The lead now:** every section pointer the fill captured sat in the area's own mapping
  (`0xb4...`), and the `CCubeModel` constructor is reached only after the whole fill returned and
  `CCubeRenderer::AddStaticGeometry` had run `CCubeModel::MakeTexturesFromMats` over the area's
  material section. So the bad entry (a pointer whose high half is not the area's) appeared
  *between* capture and use, inside that window - heap corruption from the material/texture step, or
  a write past the end of the surfaces vector. Next: log the instance's entries *inside*
  `AddStaticGeometry` (index and value of the first out-of-mapping entry, before the constructor runs)
  to tell "captured bad" from "corrupted afterwards".

**The phone, with both fixes (`tools/android_apk.sh :app:assembleRelease -PandroidNodStub=false`).**
`the first game update is at 2.7 s`, `gameplay reached at 7.0 s` (was 34.0 s / 36.0 s), and the run
plays (`/tmp/opencode/phone_skipint.png`, `ph_2.png`). Also on that build: the **flickering
camera-attached triangle** the person described is still there - one screenshot (22:23) shows a flat
grey wedge over the left half of the viewport with a hard diagonal edge, two seconds later it is gone.
It is neither the cutscene geometry (`457c86fe2`) nor anything this session touched, and the person's
own later read is that it may not be the cutscene at all; it stays the one unexplained rendering
artifact in ordinary phone gameplay.

**Next.**
- The person's scannable-highlight check on the new APK.
- The other CPU-bytes consumers the EFB copy can now serve cheaply: `CFluidPlaneCPU` (reads the
  half-size RGB565 copy as data), `CCubeRenderer::GetRenderToTexBuffer`'s reflections, and the
  mip-chain path in `GenerateScreenMipmaps`.
- M4, the release APK polish.

## 2026-10-17 (fifth session): the EFB-into-CPU copy - the scan visor's spare buffer

**What it is.** The console's `GXCopyTex(dest, clear)` copies a region of the framebuffer into main
memory at `dest`, in the format the preceding `GXSetTexCopyDst` chose. Aurora resolves the same copy
into a *texture*, which serves every consumer that samples it as a texture, but the game also treats
some of those destinations as bytes: the scan visor uploads the spare buffer as a C8 texture whose
texels are palette indices, the camera filters upload it as a half-size RGB565 backdrop, and the scan
window's pane does the same at its own size. The port now mirrors those copies for the CPU:
- `PortEfbCopy::Request` (new, `src/MetroidPrime/PortEfbCopy.cpp`) is called immediately before the
  `GXCopyTex` it mirrors. It issues `GXRequestEfbCpuCopy`, a FIFO command that takes the copy's
  geometry, destination size and texel format from the `GXSetTexCopySrc/Dst` state that is current at
  that point, so a call site cannot disagree with the copy.
- `aurora::gfx::efb_copy` (`extern/aurora/lib/gfx/efb_copy.cpp`) is the read-back, in the shape of
  `scan_peek`: the request rides on the pass the copy closes, the pass's *resolve target* (the same
  pixels and format the texture path produced) is copied to a mappable buffer at encode time, mapped
  in `after_submit`, and converted on the CPU to the copy's texel format - A8/R8/G8/B8/I8 as one
  byte, RGB565 big-endian, which is what the game's upload of console data expects. One to three
  frames of latency; a seqlock hands the bytes to the game thread.
- `PortEfbCopy::Fill` is called from both `CGraphics::LoadDolphinSpareTexture` overloads, just before
  the upload, and copies the latest completed read-back for that buffer and size into it. Nothing is
  written from the render thread, so an upload never sees half a copy.
- Three call sites, one mechanism: `CCubeRenderer::DrawScanVisor` (A8, viewport - the visor's palette
  indices), `CCameraBlurPass::GetFbCopy` (RGB565 half, or I8 full - the camera filters) and
  `CPlayerVisor::DrawScanEffect` (RGB565 - the scan window). The results screen reads the same buffer
  and is covered by the same fill once a matching copy has been requested at its size.
- Knobs: `MP2_EFB_COPY=0` turns the whole thing off (the A/B), `MP2_DEBUG_EFB_COPY=1` logs the fill
  rate, and `MP2_NO_COPY_TEX=1` keeps a copy out of Aurora's copy-texture map so the game's own
  upload of the CPU bytes is what the GPU samples - which is how the desktop verification isolates
  the new path.

**Verified.** Desktop, camera filter, end to end: with `MP2_NO_COPY_TEX=1` the pause screen's
backdrop is a blurred frame with the copy on (`[efb-copy] first spare-buffer copy landed (320x224,
143360 bytes, sequence 2)`) and **black** with `MP2_EFB_COPY=0` - `q1_2.png` against `q2_2.png` in
`/tmp/opencode`. Gameplay and the same pause screen with the copy-texture path left on are unchanged
(`r1_a.png`, `r2_a.png`). Phone (release build installed, `MP2_INPUT=LEFT@600+2`): the visor opens,
`[efb-copy] 120 fills from 357 requests (640x448 into ...)` shows the A8 viewport copy landing every
frame, the frame rate is 60 fps in the visor (300 game frames per 5.00 s, the same as before the
visor), and the A/B with `MP2_EFB_COPY=0` shows the broken overlay - a large flat olive wedge over the
scene (`phone_off.png`) where the copy on gives a coherent tinted view (`phone_v3.png`).
**Not verified:** scanning an actual scannable and reading its highlight colour on the phone - the
save's view here has nothing scannable in the scan window (`[scan] no target: every one of the 2048
id pixels is zero`), and the desktop cannot show the visor at all (below). That check is the person's.

**Correcting the record on the visor's desktop device loss.** It is **not** fixed by `5e3e122ee`
(zeroing the spare buffer) - it was fixed in the sixth session, at the top of this file: a Z8L depth
copy resolved through the colour blit. Reproduced today, three times, with `LEFT@200+2`/`LEFT@600+2` in areas 0
and 2: `VK_ERROR_DEVICE_LOST`, GPUVM fault at the same address `0x80011ca000`, `SQC (data)`, every
time - and it still happens with the visor's overlay draw skipped, `MP2_SCAN_PEEK=0`,
`MP2_NO_COPY_TEX=1` and `MP2_EFB_COPY=0` all at once, so it is in the visor's own rendering and not
in any EFB-copy path. `RADV_DEBUG=hang` puts the fault in a plain one-sample fragment shader (a blit),
i.e. a bind group whose texture or view is already gone; the address is stable across runs and is not
in the driver's address-binding report. The earlier "it plays to its frame limit" reading came from
runs with `input=0 bytes` - they never pressed LEFT, so they never entered the visor. The same goes
for the "flat white viewport" screenshot (`visor4_a.png`): that run had no input either, so the white
is the save-load fade, not the visor. The real desktop symptom is the crash; the real phone symptom
(the wrong highlight colours) is what this session's copy fixed.

**Gates.** `tools/gen_files_cmake.py --check` clean (the new port source is in `PORT_ONLY`, so
`files.cmake` gained one line); the upstream matching build is green after this session's four port
edits in upstream files - `Game Code: 94.14% fuzzy, 63.29% matched`, `rc=0` - and every one of them is
inside `#ifdef TARGET_PC` with the `#else` text byte-identical to the revision before the session
(`DolphinCGraphics.cpp` keeps its non-PC `GXInitTexObj`/`GXInitTexObjCI` calls verbatim in an `#else`
branch for exactly that reason).

**Next.**
- The visor's desktop device loss (above) - the one thing standing between the desktop and a
  screenshot of the scan visor.
- The phone check the person can do in a minute: scan a scannable and see whether its highlight is a
  single colour from the palette rather than garbage.
- The other CPU-bytes consumers the same mechanism can now serve cheaply: `CFluidPlaneCPU` (reads the
  half-size RGB565 copy as data), `CCubeRenderer::GetRenderToTexBuffer`'s reflections, and the
  mip-chain path in `GenerateScreenMipmaps`, which are still on the texture path.

## Current state as of 2026-10-17 (moved out of `AGENTS.md`, verbatim)

## Status (2026-10-17, fourth session: M2/M3 verified, three crashes/bugs fixed, PC-first loop)

Detail, including the repro commands for every open wall, is in `docs/STATUS.md`.

- Gates green: `tools/gen_files_cmake.py --check`, and the upstream matching build after this session's
  one port edit in an upstream file (`Game Code: 94.14% fuzzy, 63.29% matched`, `rc=0`). Commits:
  `befa10ead` (sweep tools), `c600728c6` (ball camera fix), `6f7d192ea` (`B` presses, RELOAD verdict).
- `tools/sweep_areas.sh` + `tools/sweep_report.py` walk all 63 Temple Grounds areas in 4 shards in ~10
  minutes and print a per-area verdict table. Press `B`, never `START`, never `A` (see Gotchas).
- Fixed and verified twice: the handed-hop crash in `CBallCamera::FindDesiredPosition` (a dangling
  reference in `CCameraManager::GetLastCameraTransform()` that GCC folded to null). `MP2_WARP_HANDED=1`
  now forces the handed route, which makes that path a 90-second reproduction.
- Open, worst first: (1) ~~areas 14, 21 and 35 stop updating~~ - **resolved `05efe2ecc`: the player was
  dying** in the dark-world areas and the run then sat on the lose-game movie's "continue" screen
  (`docs/STATUS.md` wall 1 has the backtraces); the sweep heals twice per area now, has a `DIED`
  verdict, and a shard over areas 12..23 is clean; the sub-question is whether `A` picks "continue"
  there, which would make the RELOAD verdict's "walks into a save station" story wrong; (2) area 53's
  hop never lands in a long run - **resolved `a5b08f7ae`**: the destination was stuck in
  `kP_WaitForValidation` because a dock in area 52 preloads 53 and holds its validation until a door
  the warp never opens, and `PortWarpSettle` now releases that hold. Reaching area 53 also turns out
  to *be the ending* (the credits; `TELEPORT=53` is that, not a stall), so the report still owes a
  `CREDITS` verdict; (3) ~~an `MP2_INPUT` token before the game's
  first update is deferred and never served~~ - **resolved `4401a70e2`**: a token's frame is a game
  update now (`platform/README.md`, "Input"), so frame 0/1 is the state the game's first update reads
  and a token whose frame has gone by fires at the first frame the script sees; the counter still
  advances through a pause, so a script can press `B` to dismiss one.
- Unverified as before: hazards as gameplay, the logbook, escape/endgame, lock-on handoff, morph ball
  and gyro input, the empty-`optional_object` audit.
- **The swarm vertex explosion is fixed** (`23c928626`): the flying bugs of the Temple Grounds crater (and every other swarm and the fish cloud) drew through the guest array path with host-skinned arrays. The user's clue - "caused by the tiny flying bugs" - is what pointed at `CFlyerSwarm` and `CSwarmBasics`; the nest's web beside them is a real object.
- This session, on the phone: **M2 and M3 verified** (the SAF picker copies a picked ISO to `files/disc.iso`
  and the next launch boots with no picker; the strict first-update proof passes; touch alone walks and
  fires). **A crash fixed**: morphing then unmorphing SIGSEGV'd because `CSamusFaceReflection`'s
  destructor is `inline` in another TU and the port filled the missing symbol with a logging link stub,
  which left the face model's resources locked; defined in the class's own TU now (`bc4c733c3`), and the
  same sequence leaves the process running. **`MP2_SKIP_INTRO=1`** (`75874aff5`): with
  `MP2_SKIP_FRONTEND=1 MP2_SKIP_CINEMATICS=1 MP2_TELEPORT=<area>` a run reaches a controllable player
  with no input - 36.0 s on the phone, 39.1 s on the desktop, against ~3 minutes of hand-pressed `A`
  before. `MP2_SKIP_FRONTEND` still gives up on an empty card, and the 34 s before the first update is
  loop-bound ARAM staging, not phone I/O (both in `docs/STATUS.md`).
- **The HUD-over-cutscene and the wedge beside it are fixed** (`457c86fe2`): the port's cinematic skip
  stopped the camera but not the scene, so the intro played on under the first-person camera with the
  HUD and visor over it - the wedge was cutscene geometry, not a broken model. **The swarm boids no
  longer explode** either (`23c928626` + `651988088` + `95d6dc3ca`), and a `TARGET_PC` guard that had
  leaked out of its block (matching build 63.29% -> 63.25%) is back. Iterate on the desktop build;
  the phone is for the final APK check (`docs/STATUS.md` has the run and shot commands).
- Open, and the reason the `MP2_DEBUG_DRAW`/bounds instrumentation exists: first-person world geometry
  that renders as a vertex explosion (dark maroon slabs and white
  strands; `/tmp/opencode/ve_fp3.png` and `m2.png` are phone frames of it). Not a reach stub (`0 of 45`
  are referenced), not a missing resource, and the face reflection only draws on a visor transition. The
  phone reproduces it on every run in that area.
- The Android APK the user asked for: plan and the M0-M3 detail in `docs/ANDROID.md` (section 4).
  **M0 done**: `tools/android_probe.sh` cross-builds this tree for arm64-v8a into
  `build/android-aarch64/libmetroid_prime2_port.so` - `AArch64`, exports `SDL_main`/`aurora_main`,
  `llvm-nm -u` all platform symbols, offline (Dawn/SDL/Rust caches in `build/android-deps`). The
  REL arena gate passed **on the user's phone** (32 MiB arena at `0x12000000`, the console's
  `0x81000000` base honoured exactly, no `MAP_32BIT`; fallback is a `/proc/self/maps` gap scan,
  covered on Linux by `port_arena_placement_tests`). **M1 done**: `android/` + `tools/android_apk.sh`
  build and install `org.metroidprime2.port`, which mounts the user's own disc through the real
  Rust nod on the phone (86 modules initialised, card loaded, renderer up, game loop running); fd
  1/2 are mirrored to logcat and the dirs resolve from `files/port_paths.txt`. Two known Android
  issues are recorded in `docs/ANDROID.md`: a one-off Dawn Vulkan-surface SIGABRT when launched
  with the screen locked, and the debug variant's `-O0` musyx inline link error.
  **M2 verified on the phone**: the `env.NAME=value` bridge in `files/port_paths.txt` (the `set`
  lines, `paths chosen` and `disc mounted` are in `docs/ANDROID.md`), the SAF picker with its
  G2ME01-header check and `.part`-then-rename copy (picked from `/sdcard`, `copying the disc
  image, 100%`, `disc image picked: content://…, copied to …/files/disc.iso`), and a relaunch
  after that boots with **no picker**. The strict `[harness]`/`[ids]` "first update" proof passed
  too: `the first game update is at 33.9 s`, `[ids] frame 0: 352/1024 ids allocated`.
  **M3 done and device-verified** (this session): `platform/port_touch.{cpp,h}` + MP1's
  `platform/touch_pad.{h,cpp}` + `tests/touch_pad.cpp` + `android/.../TouchControlsView.java`
  (the SDL virtual pad, a third aim source in `PortMouse`, the settings in `<user>/touch.cfg`, the
  JNI bridge, and a phone-only "Edit" pill for the layout editor). On the phone a run with
  nothing but touch walks the player (`[touch] axis 1 -1.00`) and fires (`[touch] button 0
  down/up`), the pad attaches as `Metroid Prime touch gamepad`, and the editor opens, switches
  GameCube/twin layout and saves it. `port_touch_pad_tests` covers the pad on the host;
  `MP2_DEBUG_PAD=1` prints one line per touch event for a device log. Next: **M4**, the release
  APK polish.

---

Gates: `tools/gen_files_cmake.py --check` clean, and the upstream matching build green after this
session's port edit in an upstream file (`Game Code: 94.14% fuzzy, 63.29% matched`, `rc=0`). Commits:
`befa10ead` (the sweep tools), `c600728c6` (the ball camera fix), `6f7d192ea` (`B` presses, the
RELOAD verdict).

**Still open, and how to settle each**
- **A flickering triangle from the viewport's lower-left, camera-attached** (the person's words), seen
  before the cinematic fix. That fix explains one wedge - cutscene geometry under the first-person
  camera - so this may be the same thing. It is **unconfirmed**: the person has not yet played the
  build with `457c86fe2` on the phone. If it persists in ordinary gameplay it is a second bug; the
  cheapest reproduction is the desktop crater run (`USERDIR=/tmp/opencode/mp2card/user TELEPORT=0
  tools/run_gameplay.sh`), not the phone.
- **The packed-UV lead is closed, as an instrument bug rather than a game bug** (`6b33c3336`): the
  materials whose TEX0 indices only fit the packed array are the ones flagged for it, and the draws
  that looked wrong are the world-shadow pass's black material, which returns before any array is
  chosen and whose texgen reads positions only. The scan now checks each surface against the array its
  own material selects.
- **The areas 14/21/35 update stall** (below) is being worked with `CIOWinManager`'s win list as the
  instrument, which is the thing to read at the stall rather than the functions that stop being called.

**Fixed this session, all verified on the desktop build** (rendering, cinematics and HUD questions
reproduce there, so they are not worth driving through the phone):
- **The HUD drawn over the landing cutscene, and the large translucent wedge beside it, are one bug**
  (`457c86fe2`). The port's cinematic skip stopped the cinematic *camera* (`StopCinematics`) and left
  the *scene* playing, so the intro ran on under the first-person camera with the HUD, the visor and
  the faceplate over it - the wedge is cutscene geometry seen from the wrong camera, not a broken
  model. Retail's skip (`CStateManager::SpecialSkipCinematic`) also fires the scene's skip
  connections, so it jumps to its end; that is what the port calls now, with `StopCinematics` as the
  fallback for a scene with no skip function. Controlled A/B: with `SKIP_INTRO=0` the cinematic has
  no HUD at all, with `SKIP_INTRO=1` it had the whole HUD over it, and with the fix `SKIP_INTRO=1`
  skips the intro outright and reaches the crater at the same time (33.9 s / 35.9 s).
- **A port edit that leaked out of its `TARGET_PC` guard** (`95d6dc3ca`). The swarm residency guard
  added a helper and an `if` in `CSwarmBasics.cpp` without the guard, and the upstream matching build
  dropped from `63.29%` to `63.25%` matched - less than one file, which is exactly why the number is
  worth reading after every upstream edit rather than glancing at "files OK". Guarded, and checked by
  stripping `TARGET_PC` blocks from all 18 files this session touched and diffing against the
  pre-session revision: identical everywhere.

**The scan visor** (the person's report: "you can't lock onto things to scan"):
- **The lock-on's missing piece is fixed** (`e8de72e7b`): MP2 picks its scan target from a 32x64 region
  whose ids live in the framebuffer's destination alpha, copied into CPU buffers and histogrammed.
  The copy went through the console `GXCopyTex` path while Aurora resolves that into a *texture*, so
  the buffers were never written. Measured first: the ids *do* reach the EFB alpha (`31/2048 pixels
  non-zero, palette 4` in area 2), only the copy-out was missing. `scan_peek` mirrors Aurora's
  `depth_peek`; one to three frames of latency, which the consumer does not mind.
  **Still unproven end to end**: the `[scan] palette N selected object X` line has not been seen,
  because on the desktop the visor could not be entered at all (below) and on the phone the build with
  the fix has not been tried yet.
- **The spare buffer is zeroed at allocation** (`5e3e122ee`): entering the visor lost the device
  (GPUVM, `SQC (data)`), and the uninitialised spare buffer was the suspect, since the visor binds it
  as a C8 texture and samples its bytes as palette indices through a 248-entry TLUT; zeroing is what
  the console's heap had and keeps the worst case inside the palette. **The crash is not fixed,
  though** - see the fifth-session section at the top: it reproduces with every EFB-copy path off,
  and the verification runs behind that commit never entered the visor.
- **The shared feature - an EFB-into-CPU read-back at viewport size to fill the spare buffer - is
  in** (fifth session, top of this file): the scan visor's overlay (`CCubeRenderer::DrawScanVisor`),
  the camera filters (`CCameraFilter.cpp:569-676`) and the scan window all read it through
  `PortEfbCopy`, with the same encoder-task shape and a frame of latency.
- **Confirmed on the phone (the person's report)**: scanning works - objects now lock on and scan -
  but **the highlight colours are wrong**. That is this same missing spare-buffer copy: the visor
  colourises by sampling it, and on the phone it holds uninitialised host memory, so the palette
  lookup gives random colours. `e8de72e7b` fixed the *targeting* (which reads the separate 32x64
  region), and `5e3e122ee` stopped the desktop dying on the same buffer; neither fills it.
- **Correcting an earlier note**: `MP2_INPUT` D-pad tokens *do* switch the visor (`LEFT@200+20`
  works). What looked like the press failing was the visor opening onto its broken overlay - a white
  viewport with the HUD intact, which is index 0 of the palette after the zeroing above. The
  `MP2_DEBUG_SCAN` selection line staying silent is a separate question about that diagnostic's call
  site, not evidence the visor never opened.

**Open, and it explains why the scan visor's bug survived so long: entering the scan visor kills the**
**desktop build about two frames in** with a GPUVM fault (`VK_ERROR_DEVICE_LOST`, `SQC (data)`). The
read-back lane hit it while trying to prove a scan target gets selected, and reproduced it with the
**pre-change** binary, with its own peek disabled, with the scan-window copy disabled, and in areas
0/1/2/3 - so it is in the scan visor's own rendering and predates that work. On the phone the visor
renders, which is why the person could see the visor at all. Worth fixing first, because it is the
reason desktop verification of anything scan-related was impossible: the swapchain/device loss also
means a scan-visor run cannot be screenshotted. **Still open as of the fifth session** (top of this
file), which ruled out every EFB-copy path as the cause and left a blit sampling a destroyed bind
group.

**This session (2026-10-17, fourth).** Verified on the user's phone rather than reasoned about:
- **M2's two open checks pass.** The SAF picker copies a picked ISO to `files/disc.iso` (`disc image
  picked: content://... , copied to .../files/disc.iso`), the next launch boots with **no picker**, and
  the strict first-update proof prints `[harness] the first game update is at 33.9 s` / `[ids] frame 0`.
- **M3's touch controls work with nothing but touch**: a held press on the drawn A pill advances the
  game's own boot screen, the floating stick walks the player (`[touch] axis 1 -1.00`), A fires, and the
  Edit pill opens the layout editor, switches GameCube/twin stick and saves it.
- **Morphing and unmorphing no longer crash** (`bc4c733c3`). The face reflection's destructor is
  `inline` in another TU, so no symbol was emitted and the port filled it with a logging link stub; the
  stub left the face model's resources locked and the second reflection (built on unmorph) died with
  SIGSEGV. Measured before: `[reach-stub 0001] _ZN20CSamusFaceReflectionD1Ev` then signal 11.
- **The swarm boids no longer render as exploded geometry** (`23c928626`) - the maroon curtains and
  pale slabs that covered the Temple Grounds crater's view. Cause: `CSwarmDisplayList::Draw` bound the
  swarm's host-skinned position/normal arrays through the *guest* array path (size 0, big-endian), so
  Aurora uploaded nothing or byte-swapped floats. One fix covers `CSwarmBasics`, `CMetareeSwarm`,
  `CPlantScarabSwarm`, `CIngBlobSwarm` and `CFishCloud`, which all draw through that class. The white
  radial structure left on the wall is the bugs' nest, not a bug - the user confirmed it.

**Regression sweep after this session's fixes (63 areas, 4 shards, `TAG=postfix`).** 48 of the 53
areas the shards reached are `OK`, **including 14, 21 and 35** - the old "stop updating" wall, which was
the player dying, now plays 3000/1200/900 updates and reports `OK`, so the death diagnosis and the
second `HEAL` fixed it and area 35 (unverified when that fix landed) is confirmed. **No crash and no
reach stub in any area**, so the nine fixes this session changed nothing for the worse. Two things the
table leaves open:
- **Area 46 ends in `STALL`**, and the log's last line before the harness report is
  `[warning] [aurora::gpu] Device lost: Device was destroyed` - four shards were running software
  Vulkan in parallel, so this reads as the renderer losing its device under that load rather than a
  game stall. Worth one confirmation run on its own (single shard, `AREA=46`) before it is called
  either way.
- **Area 53 is still the hop that never lands** (`asked for but never arrived: [53]`), the same gap as
  before; shard 3 played to its frame limit, so this is the long-standing wall and not a new one.

**The sweep exists and walks the whole world cheaply.** `tools/sweep_areas.sh` boots the save's own
world, teleports each shard to its first area and walks its slice with `NEXTAREA` tokens, all through
`tools/run_gameplay.sh`; `tools/sweep_report.py` turns the shard logs into a per-area table (verdict,
updates played, id high-water, missing resources, hop route, reach stubs) plus a coverage block from
the sweep's manifest. Four shards, all 63 areas, ~10 minutes of wall clock. Press `B`, never `START`
and never `A` - the gotcha above says what each does to a run.

**Verified this session**
- **The handed-hop crash is fixed** (`c600728c6`). `CCameraManager::GetLastCameraTransform()` returned
  a reference into a temporary that had already died; GCC folded it to a null pointer and
  `CBallCamera::FindDesiredPosition` read the transform through address 0. Verified twice on Temple
  Grounds 10 -> 11 with 588 ids free, the handed route: the natural replay of the original sequence and
  a forced one (`MP2_WARP_HANDED=1`, added this session - it makes this path a 90-second reproduction
  instead of ten hops of accumulated core). Both arrive, spawn and run on; before the fix both died.
- 29 areas played clean in a single pass (0..14, 16..21, 32..35, 48..52): hops land, the post-arrival
  drop leaves 600-900 ids free, the id count stays flat, no missing resources, no crash.

**Walls the sweep found, worst first**
1. ~~**Areas 14, 21 and 35 stop updating.**~~ **Resolved (`05efe2ecc`): the player was dying.**
   A dead single player takes retail's game-over path - `CMFGame::EndGame` sets `kRM_EndMovie2`,
   `CMFGame` leaves the `CIOWinManager` stack, and `CMainFlow` puts `CPlayMovie(kWM_LoseGame)` on it,
   a "continue from last save" screen that takes no `B` - so the run sat there and the harness's
   "no game update for 30 s" was a symptom, not a hang. Measured under gdb with a backtrace per
   `AddIOWin`/`RemoveIOWin` and the two window lists side by side: in area 21,
   `[player] killed in area 21 at update 1194 by weapon type 17 - the dark-world atmosphere ...`.
   Areas 21 and 35 carry the dark flag (as do 13 and 28), and one `HEAL` per area left ~23 s between
   heals against a death ~19 s after one. The sweep now heals twice per area, the report has a `DIED`
   verdict that outranks `STALL` and `RELOAD`, and a shard over areas 12..23 exits 0 with every area
   OK. **Open sub-question**: does `A` pick "continue" on that screen? If it does, the `AGENTS.md`
   Gotcha's "`A` walks into a save station" explanation for the RELOAD verdict is wrong rather than
   incomplete. Also unverified: area 35 with the second heal.
2. ~~**Area 53's hop never lands**~~ **Resolved, and it was two separate things (`a5b08f7ae`).**
   - **The hop**: not a stall and not a death. A per-300-frame trace put the destination in `phase 9`,
     `kP_WaitForValidation` (`CGameArea.cpp:1312`), which only advances when `mValidationPaused` clears.
     A script in area 52 sends `kSM_Load` to the dock towards 53 (`CScriptDock.cpp:177`), loading it
     ahead of time and holding its validation until the door opens - and a warp never opens the door.
     The isolated hop arrived only because it left 52 before that script ran. `PortWarpSettle` now
     clears the hold while it streams (`[warp] area N: its validation was held by a dock; releasing
     it`), and the same run logs the release, `moved to area 53`, its spawn and `[area] 53`.
   - **The "moves itself to 56 and freezes" is the end credits**: `TELEPORT=53` reaches the ending -
     `[area] 56`, ids down to 60, then no updates, with "METROID PRIME 2 ECHOES STAFF CREDITS" on
     screen (`CCredits`, reached through `CScriptSpecialFunction::AcceptCredits`,
     `CScriptSpecialFunction.cpp:1066`). Nothing is wrong; a sweep walking into 53 is watching the
     credits. **Named now**: the port prints `[end] the credits are rolling` as `CCredits` goes on the
     window stack, `tools/sweep_report.py` has a `CREDITS` verdict for that phase (it ranks below
     `DIED`, above `RELOAD`/`STALL`, and its coverage line says the slice ends at the ending), and
     `tools/sweep_areas.sh` documents the slot list including the ending.
3. ~~**An `MP2_INPUT` token that fires before the game's first update is deferred and never served.**~~
   **Resolved: a token's frame is a game update now.** The script counted `PortPumpEvents` iterations,
   so every token under the boot's ~2040 iterations landed in the boot: a warp token went to
   `PortWarp`'s deferred slot while the boot request was pending and the *next* token overwrote it
   (which is why `NEXTAREA@1400` never hopped where `NEXTAREA@3500` did), and a button's hold was over
   before the game had read a pad. Frame `N` is now game update `N` - `PortHarness::GameUpdates`, the
   same axis as `MP2_FRAMES` - frame 0/1 is the pad state the game's first update reads, and a token
   whose frame has gone by fires at the first frame the script sees instead of being dropped
   (`platform/README.md`, "Input", has the table). Verified: `MP2_INPUT="HEAL@1"` and `"HEAL@0"` both
   log `[input] HEAL at frame 1 (0.0 s)` and the game serves them *in* its first update (`[warp]
   healed` prints before `[harness] the first game update is at 34.0 s`, which the same update emits
   after the warp tick); `HEAL@2500,B@3000+4` fires on updates 2500 and 3000, which the `[ids] frame
   N` lines interleave exactly; and a 3-area sweep (`tools/sweep_areas.sh`, `DWELL=400`, build-token)
   hops 0 -> 1 -> 2 at `NEXTAREA@400`/`@800` with every token served, exit 0, `sweep_report.py` all
   areas OK. Two properties worth keeping in mind: the counter still ticks while the game is *not*
   updating (a HUD memo pauses it, and the sweep's periodic `B` is what dismisses that), and it stays
   at 1 before the first update, so a screen that precedes every game update - the front end - cannot
   be driven by frame at all: `MP2_SKIP_FRONTEND` is the way past it. Warp tokens also log where they
   fired now (`[input] NEXTAREA at frame 400`), so a log can tell a token the game ignored from one
   that never fired.

**`MP2_SKIP_INTRO=1`** drives a run into gameplay from the port side, with MP1's `MP_SMOKE_WORLD`
semantics: boot the save, serve the boot warp (`MP2_TELEPORT`/`MP2_WORLD`), wait for it to land, report.
No input is synthesised. Before gameplay it (a) answers `CMFGame::EnterMessageScreen` (the HUD-memo
pause) with the game's own dismissal, `UnpauseGame` + the HUD-message frame counter, never pausing - a
real pause menu does not go through it; (b) gives a current cinematic camera the skip request START
makes (`MP2_SKIP_CINEMATICS=1` makes unseen ones skippable; without it the save's area-0 opening stays
in a cinematic camera for 900+ updates). "Gameplay reached" = 120 consecutive updates running, in
`kFS_InGame`, no warp pending/streaming/handed over, no cinematic camera; logged once as `[harness]
gameplay reached at <s> (frame <update>)`. Budget `MP2_SKIP_INTRO_FRAMES` (default 3600 game updates),
then `gave up`. `MP2_SKIP_FRONTEND` now keeps removing "SplashScreen" windows until the chain has been
quiet for 30 iterations. Desktop, `u_a10`'s card: area 5 reached at 39.1 s, area 0 at 36.0 s; the card
shows no memo, so (a) was only exercised with an injected memo (dismissed, game kept updating).
Area 0's opening visor boot-up ("SCANNING") is still on screen at 53 s, but stick input turns the
player then (the minimap rotates).
**Gap:** an empty card still plays the front end's file select (`MP2_SKIP_FRONTEND` gives up on an
empty slot instead of starting a new game).

**The ~34 s before the first game update is now the largest delay in every run**, and it is the same on
the desktop (33.8-34.0 s, five runs) and the phone (33.9 s), so it is not phone disc I/O: it is
loop-bound - the loop runs at ~60 Hz and ARAM staging moves 64 KiB per iteration (the `[aram] ...
fileSize2=` lines step by 65536).

**Still unverified**: hazards as gameplay, the scan logbook's persistence, escape/endgame paths,
lock-on handoff, morph ball and gyro input; and the empty-`optional_object` read audit
(`CGunWeapon::IsChargeAnimOver` first).

**Android APK** (asked for 2026-10-17, same session). **M0 and M1 done**; detail and evidence in
`docs/ANDROID.md` (section 4, the "M0 detail" and "M1 detail" subsections). In short: M0 is
`tools/android_probe.sh`, which cross-builds arm64 into
`build/android-aarch64/libmetroid_prime2_port.so` (`AArch64`/`DYN`, exporting
`SDL_main`/`aurora_main`, every undefined symbol a platform one) with the bionic fixes in
`platform/main.cpp` (no `execinfo`; logcat + tombstones; explicit `<aurora/main.h>`) and
`platform/rel.cpp` (arena fallback = a `/proc/self/maps` gap scan below 4 GiB), and the arena gate
measured **on the phone** (32 MiB at `0x12000000`, `0x81000000` honoured exactly, no `MAP_32BIT`).
M1 is `android/` + `tools/android_apk.sh`: `org.metroidprime2.port` installs, loads that library
with the **real Rust nod**, mounts the user's own disc on the phone, initialises all 86 modules,
loads the card, brings the renderer up and runs the loop; fd 1/2 are mirrored to logcat and the
directories come from `files/port_paths.txt`. New tests: `port_arena_placement_tests`
(`-DMP_ARENA_FORCE_PORTABLE` runs the Android arena path on Linux). Known Android issues, both in
the doc: a one-off Dawn Vulkan-surface SIGABRT when launched with the screen locked, and the debug
variant's `-O0` musyx inline link error (worked around in `android/app/build.gradle`). Unproven on
device: the front end visually (the phone was locked) and the strict "first update" - the harness
env bridge is now in place for it (`env.NAME=value` in `port_paths.txt`), and it is proven working
on the phone, but the run itself needs the phone foregrounded, awake and untouched for a couple of
minutes. **M2 implemented**: the SAF disc picker (`EnsureDiscImage` in `platform/main.cpp`) - a
`G2ME01` header check, `SDL_ShowFileDialogWithProperties`, then a `.part`-and-rename copy into
`<files>/disc.iso`; it runs before `aurora_initialize` because the system picker would otherwise
destroy the swapchain (this Aurora snapshot has no `aurora_release_lost_surface`). Its device
verification is the one still-open item. Next: **M3**, the touch controls.

**Next steps, in order**: (1) the update stall, with the repro above and the two candidates named;
(2) the 53 -> 56 freeze if it survives (1); (3) the hazards and the layer areas of `1baa96c2` area 11
as gameplay; (4) the family bodies in the priority order; (5) a no-present option.



---

## Archived AGENTS.md snapshots (moved 2026-10-18)

The following six sections are preserved verbatim for reference. They predate the
nineteenth-session updates above; prefer those newer entries when they conflict.

## Status (2026-10-18, seventeenth session)

- **Committed**: `c0ac5167f` photo stage 2a (supersampled capture, console `photo [1|2|4]`);
  `c368b4530` stage 2b (F1 Game page Photo block: FOV, roll, speed, supersample, show Samus,
  Take photo). The FOV override is read in `CStateManager::SetupViewForDraw` under TARGET_PC.
- **Verified**: the boot enters free camera (`/tmp/opencode/fov_check.png`). Gates after the edit:
  `gen_files_cmake.py --check` rc=0, `metroid_prime2_port` built rc=0, both host tests rc=0.
  The FOV slider changes the picture on screen (`/tmp/opencode/fov_a.png` baseline, `fov_b.png` after
  the click: wider view, same pose). Slider value not read back.
- **Open (photo)**: roll panel range ±45 vs `SetRoll` ±180; no panel way back to FOV 0; failed-capture
  line when the path is empty; toast or log line only.
- **Decided**: photo mode is reached from the F1 menu only (Game page Camera row and Photo block, F8).
  No pause-menu entry for now. Open F1 items: the open items from the fifteenth session (NEEDS-BACKEND rows, gyro, pointer menus,
  phone check of the F1 page).
- **Ports**: keep `MP2_CONSOLE=4790`; the host `adb` holds 4780.

## Status (2026-10-18, sixteenth session)

- **Committed**: `2cd2d2b20` warp from the F1 Game page (F5-F10 gone); `0817708b8`→`a8e42297e`
  photo mode stage 1 (free camera on F8 and the Game page's Camera row; HUD, helmet, arm cannon hidden;
  start pose from the game camera, verified on screen at `/tmp/opencode/fc_after.png`);
  `d7bf2b7a3` Aurora PNG screenshot API (`request_screenshot_png`, `screenshot_png_state`,
  `frame_buffer_size_for_scale`, `max_texture_dimension`), not yet called by the game.
- **Gates at `d7bf2b7a3`**: port build rc=0, both host tests rc=0, `gen_files_cmake.py --check` rc=0,
  upstream matching build rc=0 with Game Code 63.29% matched.
- **Photo mode stage 2, not started**: port `../MetroidPrimePort/platform/port_photo.cpp` (523 lines,
  read-only reference) and its header. Its dependencies map to MP2 as: `PortDebug::Msaa/BaseRenderScale/
  SetCaptureRenderScale` -> a capture scale through `VISetFrameBufferScale` (vendored
  `extern/aurora/include/dolphin/vi.h`, used by `platform/debug_ui.cpp` which is in no MP2 target, so
  the call needs a home in a built file); `PortLog::Write` -> the port's log; the panel -> `PortOverlay`
  (MP2's F1 page window), not `debug_ui.cpp`; `CMFGame` Tick/Input hooks and the pause-menu entry need
  `#ifdef TARGET_PC` in upstream files; the Android share path (JNI) is out of scope for stage 2.
- **Next**: stage 2a = the capture (`RequestCapture`, supersample via the frame-buffer scale, PNG via
  `aurora::request_screenshot_png`), console `photo`, one toast; then 2b = the panel (FOV, roll, speed,
  supersample, show Samus) in `PortOverlay`; then 2c = pause-menu and F1 Game entries.
- **Rules**: `../MetroidPrimePort` is read-only (read and run only; no code, commits or pushes).

## Status (2026-10-18, fifteenth session)

- **The F1 menu is MP1's menu** (commits `61a8244fb`, `0ca0a556b`, `7d7fb1e57`): pages Game,
  Controls, Video, Cheats, System with sub-tabs; desktop opens the page window with the Prime theme
  (`overlay_windowed`, `ui_theme` in settings.cfg); F1 is the input engine's PortMenu action only;
  Save settings now + "Unsaved changes"; pad navigation via `UpdateControllerNav()`. Video's
  Display sub-tab checked on screen (`/tmp/opencode/f1_video4.png`). Gates: `gen_files_cmake.py
  --check` rc=0, `metroid_prime2_port` up to date, `port_input_engine_tests` and
  `port_input_bindings_tests` rc=0.
- **Vendored Aurora**: `extern/aurora/lib/imgui.cpp` +6 lines (SDL3 gamepad Manual mode, HasGamepad
  kept across NewFrame). Documented in the commit message.
- **Not yet done** (details in `docs/STATUS.md`): NEEDS-BACKEND rows (button glyphs, remastered
  charge, swap scan/X-ray, auto-aim, pointer aim, remastered movement, separate vertical speed,
  minimap tap); photo mode (F8 conflicts with MP2 warp keys F5-F10: key choice needed); save states
  (no serializer); gyro reader and aim consumer; pointer menus; phone check of the F1 page; the
  System page and sub-tab search jumps not checked on screen.
- **Next**: the NEEDS-BACKEND rows, one bounded lane each, with `#ifdef TARGET_PC` and the upstream
  matching build kept green.

## Status (2026-10-18, fourteenth session)

- **The two open bugs are closed, verified on the phone**, and the settings file is in.
  - Sanctuary area-return crash (`0c33bd5a6`): walked on foot in Sanctuary Fortress (world
    `42b935e4`), area 0 -> 1 -> 0 with the console (`world`, `door`, `press a 90`, `stick 0 127
    240`). The return prints `FillInStaticGeometry (552 sections, already converted)`, the same
    `525 surfaces, 0 bad`, and the run has no `[model]`/`[res]` refusal. `MP2_DEBUG_AREA=1` is the
    flag behind those lines.
  - Music loop race (`00869fb76`): 443 s of `SplinterHive-132.dsp` captured on the phone with
    `MP2_DEBUG_AUDIO`/`MP_AUDIO_STATS`/`MP_AUDIO_DUMP` - four wraps, no `raced` line, `pending=0`,
    no near-duplicate 2 s block. `music_loop_seam.wav` in `/tmp/opencode` is there to listen to.
  - Settings file (`8fff230de`): `<user>/settings.cfg`, 14 keys, `MP2_<KEY>` wins, and a run that
    changes nothing never writes or creates it. Console `settings [<key> <value>]`.
- **The input engine is in charge of pad 0** (`cc210f983` + `431f82995`). MP1's `port_input_engine`
  and `port_input_bindings` live in `platform/input/` with their host tests; `port_input_devices.cpp`
  reads SDL into `RawState`, runs the engine and synthesises a `PADStatus`; `PortPumpEvents` hands it
  to Aurora through `PADSetPortOverride` (a vendored-Aurora addition, see Gotchas) while the virtual
  pad still merges, so the console's `press`/`stick`/`cstick`, `MP2_INPUT` and the phone's touch
  overlay keep working. Keyboard bindings are `<user>/controls.toml` now; `keybinds.cfg` is dead.
  Verified on the desktop with real key presses (`tools/keytap.py`) and on the phone (touch stick
  moves, touch A fires).
- **Next**: the F1 menu parity slice (UI scale, a settings filter, and video/audio pages with real
  backends - `aurora_enable_vsync` exists; a render-scale and an audio gain need small additions),
  then stage C of the input slice: MP1's remap UI (`platform/input/port_input_remap_ui.cpp`) in
  place of the 22-row Bindings page, so contexts, chords, turbo and layers become reachable.

## Status (2026-10-18, thirteenth session)

- **The phone is the target now** and the APK is installed (debug-key signed: the installed copy is,
  and the release key needs an uninstall that deletes the app's disc image and card). Work in the
  tree since the log-file commit, all committed except the music loop: crash breadcrumbs + Android
  stack walker (`eea16f8a1`), the morph-static crash and the ball's inner glow (`917f82173`), TLUT
  byte order in the scan visor and the font palettes (`e32a7bc36`, `61336daf4`), the virtual pad's
  analog triggers (`251c2b04d`), music (`5a1361d56`), MP1's touch controls + the F1 page window
  (`bed7f6ce7`), and the Prime theme with the remap page (`7b88f433f`). Detail in `docs/STATUS.md`.
- **The F1 menu is not yet MP1's**: missing the search bar, a UI scale control, the settings file
  (everything is still `MP2_*` env vars), and video/audio pages *with backends* (vsync, fps cap,
  render scale and volumes are hard-coded in `platform/main.cpp`). The pages MP2 has no backend for
  (savestates, tracker, photo, randomizer/archipelago, mods, remastered) are out of the menu.
- **Next slice, agreed**: import MP1's input engine (`platform/input/`: `controls.toml` bindings,
  device profiles, chords/contexts, its remap UI - 4,100 lines) to replace `PortBinds`/`PortMouse`'s
  binding half and the smaller remap page now in the F1 menu.
- **Fixed since**: the touch aim's release recenter (`a35193272`), and the Sanctuary door crash -
  `FillInStaticGeometry` converted a returned area's geometry a second time, byte-swapping every
  material index (`0c33bd5a6`), with model/resource refusal lines around it. **The settings file was
  reverted**: the cancelled lane's `PortSettings` wrote defaults back over the file on a second run.
  Needs a redo (save only on change, env wins).
- **Open bugs**: the music's loop point (a lane is reading the RSF loop offset against where the
  refill wraps); the ball's shell reads more golden than retail's; the Worker Splinter's transparency
  is in the disc data (a mucus layer), not a bug.
- **Phone test recipes**: `adb shell input swipe X Y X Y 250` for a press (a plain tap can be shorter
  than a frame); the cog at the bottom-left opens F1, the page list is thumb-sized at scale 2; the
  X Morph pill is at (2334, 808) in the Default layout. The log file is at
  `Android/data/org.metroidprime2.port/files/metroid_prime2_port.log`.
- **`/tmp/opencode/apk_debug.sh`** builds a debug-signed release APK (it hides
  `android/keystore.properties`, which must be restored - a killed run used to leave it moved).

- **In from today, earlier** (each committed; detail in `docs/STATUS.md`): the save tools, the log
  file, the automapper fix, and the pointer work below.

## Status (2026-10-18, twelfth session)

- **Phone session** (commits `72d9e0d01`, `283278ddd`, `952ec24f0`): a log file, the automapper, and
  an F1 window you can open with a finger.
  - **Log file**: `<user>/metroid_prime2_port.log` gets stdout+stderr (previous run as `.old.log`,
    64 MiB cap); on Android it lands in the app's *external* folder (`Android/data/<pkg>/files`), so
    `adb pull` works although the user dir is private. `MP2_LOG_FILE=0` (F1 > Testing, console
    `logfile [0|1]`; next launch). The `[aram]` staging lines moved behind `MP2_DEBUG_ARAM=1` - they
    were 62% of a 30 s run's log.
  - **Automapper**: `CMapArea`'s vertices are host-order after `PostConstruct` but were drawn through
    the guest `CGX::SetArray`, so the minimap exploded into stretched lines; `SetArrayNative` with
    real sizes for the area vertices and `CMappableObject`'s door corners fixed it. The map screen's
    phone crash was texture data freed under Aurora's FIFO thread: `GXDrawDone()` before
    `CTexture::UnloadBitmapData` drops the `CARAMToken` (see Gotchas).
  - **F1 on the phone**: a MENU pill toggles the window; while it is open, every touch except the
    pill goes to SDL (its touch-to-mouse path is what ImGui reads - injecting ImGui events from Java
    got the scale and the click timing wrong), two fingers scroll, and a drag on empty window space
    scrolls (so the window is not movable). Verified on the device.
  - **The gray triangle** over half the phone's view was the Landing Site's fish swarm, not the map
    screen: the swarm also skins on the CPU from the model's position/normal arrays, which stay
    big-endian in memory (the GPU's guest array path swaps them itself), so every boid got NaN
    vertices - one huge triangle on the phone's GPU, a ball of strands on the desktop. Fixed with a
    byte-swap in `SwarmRenderHelpers.cpp` and in `CSkinnedModel::GetSkinnedPosition/GetSkinnedNormal`
    (`4c834b5ad`); after it the map screen opens and draws cleanly on the phone with no crash.
  - **Open**: the touch control set is still M3's - MP1's missile, L/R buttons and triggers,
    beam/visor wheels and pointer menus for the game's own screens are not ported yet. The Landing
    Site's radial "strand ball" is a separate effect and has not been checked against retail.

- **Save tools** (earlier today): the card's save file can be read, exported, imported
  back and backed up before every write. New `platform/port_save.cpp` +
  `platform/include/port_save.h`; console `save [info] [<file>]`, `save export [<path>]`,
  `save import <path|latest>`, `save backup`, `save backups`; one setting, `MP2_CARD_BACKUP=1`
  (F1 > Saves "Back up the card", console `cardbackup <0|1>`), hooked at the top of
  `CMemoryCardSys::CCardFileInfo::WriteFile` under TARGET_PC - the one point both a first create and
  every later write pass through. Copies live in `<user>/save-backups` (`card-*`, `export-*`,
  `replaced-*`), the newest 20 `card-*` kept; an import copies the file it replaces aside first and
  remounts the card (`aurora_card_remount`).
  - The layout is `CMemoryCardSys::CCardFileInfo`'s and `CMemoryCardDriver::BuildSaveBuffer`'s:
    0x40 directory entry, 0x2000 header block, two 0x2000 copies of CRC + generation + 0x1ff8 user
    bytes, and in the user bytes `SSaveHeader` (15) + 192 system + 3x32 options + 32 global (0x14f)
    then 3x0xa38 `SGameFileSlot`s. Slots are listed with the game's own
    `CGameState::LoadGameFileState`.
  - The one non-obvious fact: the CRC and generation are **host-endian in a save this port wrote**
    (`BuildSaveSlot` assigns through a `uint*`) and big-endian in a console/Dolphin one, so the CRC
    is checked both ways and the generation read in the order that matched. The newer copy is chosen
    by slot timestamp, not generation - the generations alternate 0/1, so the greater is not newer.
  - Verified on the desktop (card copy, `MP2_SKIP_FRONTEND=1`, `MP2_CONSOLE=4780`,
    `tools/mp2con.py`): `save info` agrees with the boot's own `[skip] card slots` line, an export is
    byte-identical, `save import latest` round-trips and keeps `replaced-*.gci`, a backup with 27
    `card-*` files prunes to 20, and `MP2_INPUT="STATION@100,A@220+3,..."` with `MP2_CARD_BACKUP=1`
    logged the backup on the save station's own write. That A press leaves the run paused in the save
    screen, where no update runs and the console goes silent.
  - **What of the slice does not fit** (detail in `docs/STATUS.md`): save states and MP1's quick
    resume, which both need a live-world serialiser this tree does not have (MP2's resume is the
    card's save plus `MP2_SKIP_FRONTEND`); region conversion (refused: MP2 carries per-release
    SAVW/area data); slot-level copy/erase; MP1's Dolphin/Trilogy folders. Raw-image cards: import
    only.
  - Gates: `gen_files_cmake.py --check` rc=0, upstream `Game Code: 94.14% fuzzy, 63.29% matched`
    rc=0, and `DolphinCMemoryCardSys.cpp` (the only upstream file touched) strips to its HEAD non-PC
    text.

- **In from the previous sessions** (each verified on the desktop then; detail in `docs/STATUS.md`):
  the gameplay tweaks - fast morph, spring ball, toggle lock-on, sticky charge (`MP2_FAST_MORPH`,
  `MP2_SPRING_BALL`, `MP2_LOCK_ON_TOGGLE`, `MP2_STICKY_CHARGE`, F1 > Gameplay, console
  `fastmorph`/`springball`/`lockon`/`sticky`, the first two in `CPlayer`/`CMorphBall`, the other two
  as MP1's state machines in `platform/include/port_hold_toggle.h` applied to `CFinalInput`'s L/A);
  the graphics toggles - `MP2_FOV`, `MP2_NO_HELMET`, `MP2_NO_VISOR_FX`, `MP2_GUN_OFFSET`
  (`"<right>,<forward>,<up>"`), `MP2_MSAA`, `MP2_ANISO`, `MP2_HUD_SCALE` (`50..100`) - with their
  F1 rows and the console commands `gunoffset`/`msaa`/`aniso`/`hudscale`; the EFB-into-CPU copy that
  serves the scan visor and the camera filters; `MP2_SKIP_INTRO` skipping the level-load text;
  the door crash's `memmove` fix; the TCP debug console (`MP2_CONSOLE`) with `status`, `areas`,
  `warp`, `nextarea`/`prevarea`, `door`/`dock`/`station`, `heal`/`kill`, `layer`, `god`, `skipcine`,
  `objs`/`obj`/`send`, `visor`/`beam`, `tp`, `face`, `items`, `world`/`worlds`/`strg`,
  `give`/`take`, `press`/`stick`/`cstick`, `screenshot`; `MP2_DEBUG_IOWIN`; the keyboard rebinding
  slice (`platform/port_binds.cpp`, 22 rows, F1 > Bindings, `bind`/`binds`, now stored in
  `controls.toml`, and `tools/keytap.py` for real key presses on a headless display); and M4's
  release signing
  (`docs/ANDROID.md`, and the `adb uninstall` a key switch costs).
- Settings are `<user>/settings.cfg` (14 keys, `MP2_<KEY>` wins, written only when a value
  changed) plus `controls.toml` (bindings) and `touch.cfg` (the phone's overlay); knobs that are not
  registered as settings are still `MP2_*` env vars, and `platform/debug_ui.cpp` is in no CMake
  target at all.
- Knobs: `MP2_EFB_COPY=0`, `MP2_DEBUG_EFB_COPY=1`, `MP2_NO_COPY_TEX=1`, `MP2_DEBUG_IOWIN=1`,
  `MP2_DEBUG_AREA=1`, `MP2_DEBUG_DOORS=1`, `MP2_WARP_DOOR=<id>`, `MP2_GUARD_GEOMETRY=1`,
  `MP2_WARP_HANDED=1`, `MP2_DEBUG_VIS=1`, `MP2_DEBUG_TRANS=1`, `MP2_DEBUG_FILTERS=1`,
  `MP2_FOV=<deg>`, `MP2_NO_HELMET=1`, `MP2_NO_VISOR_FX=1`, `MP2_GUN_OFFSET="r,f,u"`, `MP2_MSAA=1|4`,
  `MP2_ANISO=1..16`, `MP2_HUD_SCALE=50..100`, `MP2_CONSOLE=<port>`, `MP2_GODMODE=1`, `MP2_INPUT`,
  `MP2_FAST_MORPH=1`, `MP2_SPRING_BALL=1`, `MP2_LOCK_ON_TOGGLE=1`, `MP2_STICKY_CHARGE=1`,
  `MP2_CARD_BACKUP=1`, `MP2_LOG_FILE=0`, `MP2_DEBUG_ARAM=1`, `MP2_DEBUG_DVD=1`, `MP2_DEBUG_PAD=1`.
  A blank screen with the HUD on it is a full-screen camera filter or a stuck transition: `status`
  (draw calls, camera, transition) plus `MP2_DEBUG_FILTERS=1` and `MP2_DEBUG_TRANS=1` answer it in
  one run.
- **The QoL inventory is `docs/QOL.md`**: what the tree has, the next slices in order, and the rules
  each follows.
- Next: **photo mode / free camera** (MP1's `port_photo.cpp`/`port_freecam.cpp`), then the map and
  tracker (MP2's `CMapWorld` is a different format, so that one is a rewrite). Save states stay the
  one large item with no foothold yet (see `docs/QOL.md` item 5). Left over from earlier slices:
  key/button presets (MP1's `ApplyKeyPreset`) and a native-pad remap UI (MP1's `platform/input/`
  profile system), plus the graphics knobs `platform/main.cpp` still hard-codes (aspect, render
  scale, vsync, sim rate). All listed in `docs/QOL.md`.
- Detail, per session: `docs/STATUS.md` (top of file = newest).
