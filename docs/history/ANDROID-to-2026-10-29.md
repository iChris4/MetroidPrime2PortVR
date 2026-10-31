# Android APK for the MP2 port: plan

Plan written 2026-10-17 (orchestrator session, after the Temple Grounds sweep). Nothing here is
built yet: this tree has **no** Android integration. The document exists so a fresh session can
start the APK without re-doing the reconnaissance.

## 1. What the user asked for

An installable APK to play on their phone. Decisions taken:

- **arm64-v8a, Android 13+** (minSdk can stay 28 to match MP1's app if that is simpler).
- **On-screen touch controls** (not just a Bluetooth pad).
- **In-app file picker (SAF)** for the ISO.
- **First deliverable is a boot-and-play tech demo**, not MP1-app parity: it boots the save/disc on
  the phone and plays the areas that work today. The game is not finished (see `AGENTS.md` Status:
  actor families are still stand-ins, and the Temple Grounds sweep found walls), so "playable
  end to end" is not on the table yet.

## 2. What already exists (measured, in `../MetroidPrimePort`)

MP1's port ships this pipeline; it is the template.

- **Driver**: `tools/android_apk.sh` resolves SDK/NDK/Gradle, writes `android/local.properties`,
  then `gradle --project-dir android :app:assembleRelease`.
  NDK pinned **29.0.14206865** (r29 is a hard floor); installed at
  `~/android-ndk-cache/android-ndk-r29` and `~/android/sdk/ndk/29.0.14206865`. Gradle 8.7 at
  `~/.gradle/wrapper/dists/gradle-8.7-bin/*/gradle-8.7/bin/gradle`; AGP 8.5.2.
  Keystore: `tools/make_android_keystore.sh`, `android/keystore.properties` +
  `android/metroid-prime-port-release.jks`; `-PmpDebugSigning=true` sideloads with a debug key.
  Warm APK build: `BUILD SUCCESSFUL in 1m 15s` (51 tasks, ccache and `.cxx` caches warm).
- **App shell**: single `:app` module, native build through AGP `externalNativeBuild` (CMake),
  `applicationId org.metroidprime.port`, `arm64-v8a` only, `jniLibs.useLegacyPackaging` (so the
  224 MB unstripped `.so` gets stripped), assets for textures and a pipeline-cache seed.
- **CMake side** (`MetroidPrimePort/CMakeLists.txt`): ANDROID blocks at ~40, 47-49, 54, 87-92
  (nod stub), 270-272, 287-303 (SHARED lib + `mediandk` + `-Wl,-u,SDL_main` + adrenotools hooks),
  307-333 (dev tools out), 361-364 (OpenSSL), 386-392. Aurora defines `TARGET_PC` itself.
  Aurora's Android support is already vendored here: `extern/aurora/cmake/AuroraSDL3Provider.cmake`
  (:164-176, needs `AURORA_SDL3_PROVIDER=vendor`), `AuroraDawnProvider.cmake` (:83-90 prebuilt
  `android-aarch64` Dawn, `AURORA_DAWN_PROVIDER=auto`).
  The Gradle build reads `build/android-deps/dawn_prebuilt` (or `-PcmakeDependencyCache=<dir>`)
  when it exists and downloads the pinned package otherwise (a clean CI runner); the cache must
  hold the release `AuroraDependencyVersions.cmake` pins (`dawn-android-aarch64.tar.gz` from
  `Odrannnn/dawn` `v20261026.185535`, sha256 `6a3ac06c...`, unpacked there). The copy
  `tools/android_probe.sh` falls back to (MP1's `build/android-deps`) was still the old
  encounter/dawn build on 2026-10-27, so replace it by hand after a Dawn bump.
- **Touch controls** (all Java + JNI, no assets): `TouchControlsView.java` (3148 lines, drawn in
  code) over the SDL surface; JNI glue `platform/debug_ui/android_jni.inc` -> a real SDL virtual
  gamepad (`platform/touch_pad.cpp`) plus the port's own input layer; layouts persisted as
  `touch_layout`; a physical pad hides the overlay.
- **Device paths**: user dir via `MP_USER_PATH` -> `data_folder.txt` marker ->
  `SDL_GetPrefPath(nullptr, "Metroid Prime")` (`port_paths.h:244-262`); data folder move with
  `MANAGE_EXTERNAL_STORAGE`; cache defaults to the private folder.
- **Disc**: SAF picker via `SDL_ShowFileDialogWithProperties` -> the `content://` result is copied
  to `<user>/disc.iso` (`.part` + rename) so no grant is needed later; precedence argv > `MP_DISC`
  > remembered. This is the mechanism to copy for MP2 (`aurora_dvd_open()` in this tree only takes
  a plain path - `extern/aurora/lib/dolphin/dvd/dvd.cpp:658` -> `SDL_IOFromFile` - so *copying* the
  pick to local storage is the cheap route, not reading the URI in place).

## 3. What this tree lacks for Android

1. **No `android/` gradle project, no Java, no JNI, no CMake ANDROID blocks.** `platform/android_probe/`
   holds only a `nod` stub (`nod_stub.c`, `include/nod.h`) - the probe's other files are gone.
2. **Desktop-only host code** (from an inventory of this tree):
   - `platform/main.cpp:26,88-108`: `execinfo.h` backtrace + SIGSEGV/BUS/ILL/FPE handlers. bionic has
     no `execinfo.h`; needs a `__ANDROID__` path (logcat-only, or a tombstone).
   - `platform/glibc_compat.c`: glibc-only, self-excludes on bionic.
   - Env-var config everywhere (`MP2_DISC`/`MP2_USER_PATH`/`MP2_CACHE_PATH`/`MP2_SKIP_FRONTEND`/
     `MP2_INPUT`/`MP2_WORLD`/`MP2_FRAMES`/`MP2_DEBUG_IDS`): a JNI launcher can set env before
     `main`, or a config file + intent extras; MP1 solved this with `port_paths.h` + a marker file.
   - Keyboard/mouse bindings, F1-F12 overlay hotkeys: meaningless with touch; the touch overlay has
     to feed the port's virtual pad instead.
   - Dev/harness only: `tools/*.sh` (Xvfb, gdb, x11grab), `-rdynamic`, CTest targets, host binutils
     (`ld.bfd`/`objcopy`/`nm` in `CMakeLists.txt:162-164`) and the `link_check`/REL-stub ladder.
3. **Architecture-sensitive spots** (little-endian LP64, so most LP64 notes carry over unchanged):
   - `platform/rel.cpp:322-366` `GuestArena`: the 32-bit guest address requirement. On arm64 there
     is no `MAP_32BIT`, and the `#elif defined(MAP_FIXED_NOREPLACE)` branch *uses* `MAP_32BIT`
     unconditionally, which bionic does not define - a compile error to fix. **Fixed at M0** (a
     `/proc/self/maps` gap scan below 4 GiB, `MAP_FIXED_NOREPLACE`, then a hint at `0x10000000`,
     then a refusal that names the size) and **measured on the phone**, where a 32 MiB arena lands
     at `0x12000000` and the console's `0x81000000` module region is free and honoured exactly:
     see section 4a.
   - No x86 intrinsics or asm anywhere in the port's own code (LZO is plain C), so NEON is not a
     gate.
   - Signed `char` is already forced (`CMakeLists.txt:48-52`, `platform/compat.h:60`).
4. **`MP_SDK_HEADERS_ONLY=ON` is the only *verified* configure** in this tree; `build-port-link` is
   the full build. Whichever Android uses, the CMake dev-tool steps have to be excluded.

## 4. Milestones (each one verifiable without assuming a working device)

**M0 - arm64 cross-build links.** Add a toolchain/ANDROID path to `CMakeLists.txt` (exclude the dev
tools, host-binutils steps, tests; pick the Aurora Android providers; keep the SDK stubs the way
`MP_SDK_HEADERS_ONLY` does or build the real SDK), fix the bionic blockers (`execinfo`, `MAP_32BIT`,
any glibc shim), and produce `libmetroid_prime2_port.so` for arm64. **Done - see the M0 detail
below.**
*Verified when*: `$NDK/.../llvm-readelf -h libmetroid_prime2_port.so` says `AArch64` and the link
has no undefined symbols (`llvm-nm -u`), i.e. the same acceptance the desktop `link_check.sh` gives.
No device needed. This is the gate: if the REL arena cannot be placed, stop and report.

### M0 detail: what landed (2026-10-17)

`tools/android_probe.sh` cross-builds this tree for `arm64-v8a` into
`build/android-aarch64/libmetroid_prime2_port.so` (134 MB); `llvm-readelf -h` says `ELF64`, `DYN`,
`AArch64`, and it exports `SDL_main` (the entry SDL's Java activity looks up) and `aurora_main`.
`llvm-nm -u` lists 465 symbols and **every one of them is a platform symbol** - libc, libm, liblog,
libandroid, zlib, GLES, OpenSLES - resolved by the 9 `NEEDED` libraries (`liblog libandroid libz
libm libOpenSLES libdl libGLESv1_CM libGLESv2 libc`), with no game or port symbol among them; the
NDK links with `--no-undefined`, so a green link *is* resolution. No network: Dawn comes from the
prebuilt `android-aarch64` package and SDL from the 3.4.10 sources, both copied into
`build/android-deps/` from `../MetroidPrimePort` (`build/` is git-ignored; the script says where to
get them when the copy is missing).

The four things section 3 predicted, as they actually landed:

- **`CMakeLists.txt`**: the module-scoping step uses the NDK's `ld.lld -r --force-group-allocation`
  / `llvm-objcopy` / `llvm-nm`, because the host binutils cannot read an AArch64 object at all;
  `metroid_prime2_port` becomes a `SHARED` library with `-Wl,-u,SDL_main`, links `log android z`,
  and drops `-rdynamic`; `MP_ANDROID_NOD_STUB` substitutes `platform/android_probe/nod_stub.c` for
  nod (Aurora skips its own nod provider when a `nod::nod` target already exists). The desktop
  build, the `-rdynamic` link and the matching build are untouched.
- **`platform/main.cpp`**: bionic has no `execinfo.h`, so the crash handler logs the signal and the
  fault address to logcat and re-raises - the tombstone is the stack Android gets. The file also
  now includes `<aurora/main.h>` explicitly: that header's `main` -> `aurora_main` rename is what
  gives the shared library its entry point (the desktop links the same way `../MetroidPrimePort`
  does, and still boots).
- **`platform/rel.cpp`**: the arena's 32-bit-address fallback, see below.
- **`platform/glibc_compat.c`** needed nothing: it self-excludes on bionic, as predicted.

No GRADLE, Java, JNI, SAF picker or touch controls at M0, and **nod is the stub**: this library
links, it cannot read a disc yet. Real nod is a Rust cross-build (Corrosion,
`Rust_CARGO_TARGET=aarch64-linux-android`, the vendored rustup under
`../MetroidPrimePort/build/android-rust` has that target installed) and is M1 work.

### The REL arena gate: passed, on the phone

bionic defines `MAP_FIXED_NOREPLACE` but not `MAP_32BIT`, so `GuestArena`'s fallback now scans
`/proc/self/maps` for the lowest free 1 MiB-aligned gap that fits below 4 GiB and maps it with
`MAP_FIXED_NOREPLACE`; failing that it asks for a plain hint at `0x10000000` and takes it only if
the result is below 4 GiB; failing that it stays invalid and prints one line naming the size and
the reason. The existing `< 4 GiB` check is the single accept/reject gate.
`-DMP_ARENA_FORCE_PORTABLE` selects this same path on a host that *does* have `MAP_32BIT`, so
`port_arena_placement_tests` covers it on Linux without a phone.

The phone measurement: a throwaway debug APK (`org.metroidprime2.arenaprobe`, built in `/tmp` with
its own CMakeLists, whose only native source is this tree's `platform/rel.cpp` compiled for arm64
with **no arena macro at all**), installed on the user's Xiaomi 25102PCBEG, Android 16, arm64-v8a.
6 of 6 runs identical:

    maps: below4g_mappings=26 mapped_bytes=780750848 free_bytes=3514216448 (of 4294967296)
    maps: gap[0] 0x0-0x2000000 (32768 KiB) ... gap[4] 0x759d8000-0xebad6000 (1934328 KiB)
    arena_a: valid=1 base=0x12000000 size=33554432 below4g=1     # no preferred base
    arena_b: valid=1 base=0x81000000 preferred_hit=1             # the console module region
    arena_c: valid=1 base=0x100000 size=1048576 below4g=1
    alloc:   ptr=0x12000000 aligned=1 inside=1 below4g=1
    RESULT: PASS

So: a 32 MiB arena is placed below 4 GiB without `MAP_32BIT`, and `0x81000000` - the base the REL
tests use because a real module's `REL24` branches need the console's module region - is free and
honoured exactly in a real Android 16 app process. Caveat worth keeping: that process was fresh, so
SDL/Vulkan/Dawn were not mapped yet; a later run inside the real app would be the confirmation, and
the port does not construct a `GuestArena` at boot today (only the REL tests do).

The probe project is not kept in the tree (the durable part is
`tests/port_arena_placement.cpp`); remove the package with
`adb uninstall org.metroidprime2.arenaprobe`.

**M1 - minimal app that boots.** New `android/` gradle project modelled on MP1's (app module,
manifest, `MetroidPrimeActivity` + Aurora surface, `jniLibs`, debug signing), with paths for the
disc/user/cache dirs (fixed app-internal paths + a `data_folder.txt`-style marker) and logcat
mirroring of the port's `[warp]`/`[ids]`/`[harness]` lines. **Done - see the M1 detail below.**
*Verified when*: `adb install` + launch reaches the game's first update on the phone with a
Bluetooth pad, and `adb logcat` shows the same boot lines the desktop log shows.

### M1 detail: what landed (2026-10-17)

`android/` holds the Gradle project (AGP 8.5.2, `org.metroidprime2.port`,
`org.metroidprime2.port/.MetroidPrime2Activity`, arm64-v8a, `minSdk 28`/`targetSdk 34`, no
permissions, debug signing), driven by `tools/android_apk.sh` (SDK/NDK/Gradle resolution like
MP1's, `-PmpDebugSigning` for a sideload). Java is SDL's own `org/libsdl/app/**` (copied from the
version-matched SDL 3.4.10 source, which is why they are tracked) plus `AuroraSurface` and a
75-line activity: it names `metroid_prime2_port` in `getLibraries()`, and writes
`files/port_paths.txt` (`user=`, `cache=`, `disc=`) *before* `super.onCreate` loads the library, so
a pushed copy wins and the picker has somewhere to write.

Native side, both in `platform/main.cpp` and both `#if defined(__ANDROID__)`:

- **The log.** Android discards fd 1 and 2, so the entry repoints them at a pipe and a detached
  thread turns the bytes back into `mp2port` logcat lines (unbuffered stderr, line-buffered
  stdout, drained on all four exits). Without it a run that stops at the disc is indistinguishable
  from one that never started - which is what the first stub-APK run looked like.
- **The directories.** `MP2_USER_PATH`/`MP2_CACHE_PATH`/`MP2_DISC` still win when set, and the
  env-less app falls back to `SDL_GetPrefPath(nullptr, "Metroid Prime 2: Echoes")` - on Android
  the app's own files directory - with `user/`, `cache/` and `disc.iso` beneath it, overridden by
  `port_paths.txt`. It prints the three it chose, the way MP1 prints its memory-card path.

**nod is now the real one, and the default** (`-PandroidNodStub=true` selects the stub; the
Gradle caches are optional, missing ones are fetched, so CI needs only `rustup target add
aarch64-linux-android`). `tools/android_apk.sh :app:assembleDebug` builds it (Corrosion + the vendored Rust toolchain: this tree's `build/android-rust`, falling back
to `../MetroidPrimePort/build/android-rust`, `CARGO_NET_OFFLINE=true`, the offline
`aurora_nod`/`corrosion` FetchContent caches). Verified on the packaged, stripped library:
19 `T nod_*` symbols including `nod_disc_open_stream`, 522 undefined (all platform), zero
`nod_stub.c` strings; the stub build has 9 and 3.

**On the phone** (Xiaomi 25102PCBEG, Android 16/arm64, the user's own disc pushed to
`files/disc.iso` after `run-as`; 1.46 GB in ~25 s at 55 MB/s): the run reaches

    mp2port : metroid_prime2_port: disc mounted: /data/user/0/org.metroidprime2.port/files/disc.iso
    mp2port : metroid_prime2_port: init compiled module WallCrawler … (86 modules)
    mp2port : [info] [aurora::card] Loaded GC Card Image: …/files/user/USA/Card A
    mp2port : Initializing renderer...   then   Low latency audio enabled
    (AAudio 44100/2ch fed for the whole 35 s; the process is alive at the end, no ANR)

So the app installs, loads the library, mounts the user's disc, accepts `G2ME01`, brings Aurora and
the renderer up and runs the game loop. Not proven: the front end *visually* - the phone was
locked, so `screencap` returned the lock screen - and "reaches the game's first update" in the
strict sense, because the port's harness counters are driven by environment variables an installed
app cannot set; the env bridge (below) is what will put `[ids] frame …` in a device log.

Known issues, in order of how likely they are to be hit:

- **One SIGABRT at startup with the screen locked**: `FORTIFY: pthread_mutex_lock called on a
  destroyed mutex`, signal 6, `SI_QUEUE`, through `android::Surface::hook_perform` →
  `libvulkan CreateAndroidSurfaceKHR` → Dawn `CreateVulkanSurface` → `SwapChain::Initialize` →
  `aurora::webgpu::initialize` → `aurora_main` ← `SDLActivity.nativeRunMain`. That is Dawn
  configuring a surface from an `ANativeWindow` SDL has already destroyed, i.e. an activity
  lifecycle race, not a game bug; three relaunches with the same locked screen did not reproduce
  it. Reported here so the next device session does not re-discover it.
- **The debug variant cannot be built at `-O0`**: musyx's C99 `inline u_seqPlaySong`
  (`extern/musyx-port/src/musyx/runtime/s_data.c:524`, called at :597) has no out-of-line copy
  there, so the link fails. `android/app/build.gradle` builds the debug variant's native code as
  RelWithDebInfo instead of touching vendored musyx; `static inline` there is the real fix, for
  whoever owns that snapshot.
- **A locked phone is an awkward test target**: `adb exec-out screencap` returns the lock screen,
  so a device session needs the phone unlocked to prove anything visually.

Next: **M2** - the SAF picker, plus an environment bridge so a device run can set the harness
variables (`MP2_DEBUG_IDS`, `MP2_FRAMES`, `MP2_INPUT`) from `port_paths.txt`; that is what makes
"the game reached its first update" provable on the phone instead of inferred from audio.

**M2 - SAF picker.** Port MP1's picker + copy-to-`<user>/disc.iso` (`main.cpp:513-560` there), with
MP2's game-id check (this port expects `G2ME01`). **Implemented, device verification partial - see
the M2 detail below.** *Verified when*: picking an ISO once makes later launches boot with no
picker.

### M2 detail: what landed (2026-10-17)

Two things, both in `platform/main.cpp`, both `#if defined(__ANDROID__)`:

- **The environment bridge.** `port_paths.txt` gained `env.NAME=value`: the reader calls `setenv`
  while it reads, which is the top of the entry - before Aurora starts and before the game or the
  harness looks at anything. That is the device's only equivalent of the desktop's
  `MP2_FRAMES=1800 MP2_SKIP_FRONTEND=1 ./metroid_prime2_port` prefix, and it is what makes a
  device run drivable at all. Verified on the phone, verbatim:

      mp2port : metroid_prime2_port: port_paths.txt set MP2_DEBUG_IDS=1
      mp2port : metroid_prime2_port: port_paths.txt set MP2_FRAMES=1800
      mp2port : metroid_prime2_port: port_paths.txt set MP2_SKIP_FRONTEND=1
      mp2port : metroid_prime2_port: paths chosen: user=…/files/user cache=…/files/cache disc=…/files/disc.iso
      mp2port : metroid_prime2_port: disc mounted: /data/user/0/org.metroidprime2.port/files/disc.iso

- **The picker.** The trigger is the resolved image not being usable - `ResolveDiscPath` hands back
  `<files>/disc.iso` whether or not it exists, so a first launch would otherwise reach
  `aurora_dvd_open` and die with nothing on screen saying what to do. `EnsureDiscImage` reads the
  first eight bytes and wants `G2ME01`, revision 0, disc 0 (a raw `iso`/`gcm` starts with its boot
  header, which is why those are the offered filters), asks through
  `SDL_ShowFileDialogWithProperties` with a five-minute deadline, then copies the pick to the
  resolved path through `<name>.part` and a rename: the header is checked before a byte is written,
  a short copy is rejected on the size that actually landed, and a multi-minute copy that is killed
  cannot leave a truncated image that the next launch would prefer over asking again. It re-asks
  after a bad pick, and a cancel or a failed copy exits before Aurora comes up rather than starting
  the renderer only to close it.

It runs **before `aurora_initialize`**, deliberately: the system picker covers the app and destroys
its surface, and MP1 only survives that by dropping its swapchain by hand
(`aurora_release_lost_surface`, which this snapshot of Aurora does not export). Before the window
exists there is no swapchain to lose, and the copy of a 1.4 GB image - minutes on a phone - happens
before the renderer is up instead of during a frame.

**Verified on the device (2026-10-17, second session)** - both checks M2 left open, with the
disc image the user keeps at `/sdcard/MetroidPrime/disc.iso` on the phone: the app's copy was
moved aside, the run printed `disc.iso ... is not a Metroid Prime 2: Echoes disc image; asking
for one` / `asking for a disc image`, the system picker was driven by hand (drawer, search,
`mp2-echoes.iso`) and the log then shows `copying the disc image, 100%` and
`disc image picked: content://com.android.providers.downloads.documents/document/raw%3A%2Fstorage%2Femulated%2F0%2FDownload%2Fmp2-echoes.iso,
copied to /data/user/0/org.metroidprime2.port/files/disc.iso`, followed by `disc mounted`; a
relaunch after that boots with **no picker** (`disc mounted` and no "asking for a disc image"),
which is this milestone's acceptance. The strict first-update proof is in the M3 detail below.

**M3 - touch controls.** Port `TouchControlsView.java` + `touch_pad.cpp` + the JNI bridge, and map
them into this port's input layer (`platform/port_events.cpp`'s virtual pad, `platform/port_mouse.cpp`
for the look-stick).
*Verified when*: a run on the phone moves the player and fires with only touch.
MP2-specific work: MP2 has no `port_input_settings.cpp`/`debug_ui` module split yet, so the layout
settings have somewhere to live only after a small refactor - see MP1's `platform/debug_ui/controls.inc`
if the module split is copied. **Implemented, device-verified - see the M3 detail below.**

### M3 detail: what landed (2026-10-17)

Four new files and four small edits, and no MP1-specific subsystem ported that MP2 cannot back:

- **`platform/include/port_touch.h` + `platform/port_touch.cpp`** are the native side: the pad,
  the aim, the overlay's settings and the JNI entry points (MP2 has no input-settings module and
  no JNI bridge, so MP1's `debug_ui/*.inc` + `android_jni.inc` split collapses into one module).
  It feeds the game exactly three ways - an SDL *virtual gamepad* (the overlay is a real pad, so
  the game's own control mapper, prompts and C-stick/beam/visor handling apply to touch with no
  game-side change), the touch aim through `PortMouse` (a third aim source beside the mouse and
  the twin stick), and its own settings. The settings live in `<user>/touch.cfg`
  (`classic=`/`twin=`/`layout=`), beside the memory card, because MP2 has no settings file of its
  own yet.
- **`platform/touch_pad.{h,cpp}`** are MP1's virtual-gamepad module, ported unchanged (it was
  already platform independent on purpose), and **`tests/touch_pad.cpp`** with them. A
  `port_touch_pad_tests` target in the full build runs it on the host: `touch pad: mapping,
  conversion and cleanup all behave`.
- **`android/.../TouchControlsView.java`** is MP1's view (2167 lines against MP1's 3148) over the
  SDL surface, added by `MetroidPrime2Activity.addTouchControls()` to SDL's own `mLayout`, exactly
  as MP1 adds its own. It keeps MP1's three layouts, its layout editor, its hide pill, its
  floating stick and its per-control layout string, and it gains one thing MP1 does not need: a
  small **Edit** button beside the eye in the bottom row. MP1 opens the editor from its F1 window's
  "Edit layout", and MP2's F1 window takes no touch, so without that button the editor the port
  went to the trouble of keeping would be unreachable on the only device this overlay exists for.
  It calls the new `nativeRequestTouchEdit` JNI entry point, which raises the same
  `PortTouch::RequestEdit` flag the F1 button will use.
- **`platform/port_events.cpp`** calls `PortTouch::BeginFrame()` each iteration and reports a
  physical key or mouse button; **`platform/main.cpp`** calls `PortTouch::Initialize(<user dir>)`
  before Aurora; **`platform/port_mouse.{h,cpp}`** gained the touch aim source (`TouchEnabled`,
  `AddTouchAim`, `SetTouchAimDown`, `SpringSwipe`, `TouchAimSpeed`, mutex-guarded pending pixels)
  and **`src/MetroidPrime/Player/CPlayer.cpp`** plus
  **`src/MetroidPrime/Cameras/CFirstPersonCamera.cpp`** ask it alongside the mouse, inside their
  existing `#ifdef TARGET_PC` blocks, so the non-PC path is unchanged.

**MP2-specific decisions**, all of them because Echoes differs from Prime:

- **The default layout is the classic GameCube one, with the C-stick**, and drag-to-aim is off
  (`MP2_TOUCH_AIM=1` turns it on). MP1's default has no C-stick because its beam and visor wheels
  are the only way to switch; MP2 switches beams and visors *with the C-stick* and has no wheel
  control, so leaving it out would strand them.
- **MP1's wheels, minimap tap, map pan/zoom, pointer menus, Turbo, beam shift and update toast are
  not ported.** The Java view has none of that code and no native exists
  for them; the day one of those subsystems lands, its Java declaration and its native are added
  together. That includes the F1 overlay pill: MP2's F1 window takes no touch yet, so a pill that
  opened it would open a window the phone could not close.
- **`PortPrompts::NoteTouchInput` is declared but not built** in this tree
  (`platform/port_prompts.cpp` is not in `CMakeLists.txt`), so `PortTouch::NoteDevice` records the
  layout instead of calling it; the JNI entry point stays for when that module is wired in.

**On the phone** (Xiaomi 25102PCBEG, Android 16/arm64, the M3 debug APK):

- the strict "the game reached its first update" check that M2 left open, verbatim:
  `[harness] MP2_FRAMES=1800: the run ends itself after 1800 game updates`,
  `[harness] the first game update is at 33.9 s`, `[ids] frame 0: 352/1024 ids allocated`.
- the pad attaches and Aurora takes it as a controller:
  `[touch] pad attached id=97 open=1 gamepad=1` then
  `[info] [aurora::input] Added controller 'Metroid Prime touch gamepad' (instance 97, vid 045e,
  pid 02ea, type 3)`.
- touch reaches the game: a held press on the drawn A pill logs `[touch] button 0 down`/`up` and
  the game advances its own boot screen ("WARNING-HEALTH AND SAFETY ... Press any button to
  continue" to the Nintendo logo), i.e. the whole path Java view to JNI to virtual pad to Aurora's
  pad to the game works with nothing but touch. In gameplay, holding the floating left stick up
  (`adb shell input swipe 500 900 500 470 4500`) logs `[touch] axis 1 -1.00` and moves the player -
  the screenshot shows the stick deflected and the corridor closed up around the camera - and a
  400 ms press on A fires the beam, which is M3's acceptance ("moves the player and fires with
  only touch"). The Edit pill opens the editor (every control gets its dashed grab box, the tool
  row reads `- + Reset Reset all Layout Done`), the `Layout` tool switches to the twin-stick
  layout live and writes `classic=0 / twin=1` to `touch.cfg`, and `Done` closes it.
- **two bugs came out of the device runs**, both fixed and rebuilt: `SDL_EVENT_GAMEPAD_ADDED` was
  read as "physical input", which hid the overlay the moment its own pad attached (a physical pad
  is now found by scanning SDL's device list and skipping virtual ones), and a `tap` shorter than
  a frame is invisible to the game - MP1 documents that limitation in `touch_pad.h`, and a device
  test has to hold a button (`adb shell input swipe x y x y 600`) to see it.
- `MP2_DEBUG_PAD=1` (a line per button and stick change) is what made the touch readable in a
  device log; without it a working overlay and a dead one look identical from outside.
- **Since M3 (2026-10-18)**: the overlay carries a MENU pill that toggles the F1 window, and while
  it is open every touch but the pill goes to SDL as a mouse (MP1's approach), so the window is
  usable with a finger; two fingers or a drag on its empty space scroll it. The port also writes
  `metroid_prime2_port.log` (and the previous run as `.old.log`) into
  `Android/data/org.metroidprime2.port/files/`, which `adb pull` can read; `MP2_LOG_FILE=0` turns it
  off, and `MP2_DEBUG_ARAM=1` brings back the ARAM staging lines that are off by default.
- What is still untested on the phone: drag-to-aim (`MP2_TOUCH_AIM=1`; it is off by default and
  the C-stick is the look stick), a physical pad taking the overlay over (the pad scan), and
  moving a control in the editor and re-launching to see the saved `layout=` restored.

### M4 detail: the tech-demo release (2026-10-17)

`versionName "0.4.0"`, `versionCode 4` (MP1 is 0.28.1 / 32; here one step per milestone). Build:
`tools/android_apk.sh :app:assembleRelease -PandroidNodStub=false` ->
`android/app/build/outputs/apk/release/app-release.apk` (12.7 MB; debug is 13.8 MB). Both carry the
same 32 MB `arm64-v8a` `.so` (the native build is RelWithDebInfo in both, see `build.gradle`) with
no `.debug_*` sections; the release is non-debuggable, the debug variant is `debuggable=true`.

**Signing.** `tools/make_android_keystore.sh` writes `android/keystore.jks` and the
`android/keystore.properties` that `build.gradle` reads (both git-ignored; a CI can pass the same four
values as `MP_APK_KEYSTORE`, `MP_APK_KEYSTORE_PASSWORD`, `MP_APK_KEY_ALIAS`, `MP_APK_KEY_PASSWORD`
instead). With no key configured, `android_apk.sh` falls back to the shared debug key only when asked
(`-PmpDebugSigning=true`) and otherwise refuses, and `build.gradle` refuses a half-configured key
rather than signing with the wrong one. Verified: a release built after the script reports
`Signer #1 certificate DN: CN=Metroid Prime 2 Port, O=local, C=US` (`apksigner verify --print-certs`).
**Switching keys is a one-time uninstall on any phone that already has the port**: Android will not
replace a package signed with a different certificate, and `adb uninstall org.metroidprime2.port`
deletes the app's data - `files/disc.iso` (the picked ISO), `files/user/USA` (the memory card),
`files/touch.cfg` and `files/port_paths.txt` - so re-pick the ISO and copy the save back afterwards.
Keep the key: losing it loses in-place updates of an installed copy.
The user's phone (2026-10-20) runs a copy signed with the shared debug key
(`~/.android/debug.keystore`), so an update there is the release APK re-signed with it:
`zipalign -p 4` then `apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android
--ks-key-alias androiddebugkey` (`-PmpDebugSigning=true` does not override a configured key).
The phone is reachable over tailscale (`poco-f8-ultra`); wireless debugging shows up as an mDNS
`adb-*._adb-tls-connect._tcp` device once it is switched on.
The Lenovo tablet (TB710FU, LAN only, not on tailscale; `adb mdns services` gives its
`192.168.68.x:<port>`) has the project's release key: install `app-release.apk` unchanged.

**Reading a crash on the phone.** The port installs a fault handler that prints `[port] caught
SIGSEGV (11)` and a backtrace to logcat (tag `mp2port`) before it re-raises, so `adb logcat -d |
grep -A30 'port\] caught'` is the first thing to read; the backtrace names the port's own frames.
When the process dies without one (an abort, or a JNI/Java crash), `libsigchain` dumps the frames
instead and `adb logcat` has them under `E libsigchain:`; `ndk-stack -sym <dir with
libmetroid_prime2_port.so> -dump crash.txt` turns a full tombstone into the same symbolised frames
(the unstripped `.so` is `android/app/build/intermediates/cxx/RelWithDebInfo/*/obj/arm64-v8a/`).

**Build line.** `platform/main.cpp` prints `metroid_prime2_port: build <git hash>[-dirty]` at startup
(logcat tag `mp2port`; `adb logcat -s mp2port | grep build`), from `generated/port_build_info.h`
(`cmake/BuildRevision.cmake`, target `port_build_info`). The F1 overlay that shows it in MP1 is not
built in this tree.

**Install and run.** `adb install -r app-release.apk` (or copy it to the phone and open it; allow
unknown sources). First launch opens the system file picker: choose a raw `G2ME01` ISO; it is
checked and copied to `files/disc.iso`, and later launches skip the picker. Data lives in the app's
files dir (`/sdcard/Android/data/org.metroidprime2.port/files/`): `user/` (card, `touch.cfg`),
`cache/`, `disc.iso`, `port_paths.txt`. For a no-input run, add to `port_paths.txt`:

    env.MP2_SKIP_FRONTEND=1
    env.MP2_SKIP_CINEMATICS=1
    env.MP2_SKIP_INTRO=1
    env.MP2_TELEPORT=5

(needs a save in `user/USA/Card A`; 2.7 s to the first game update and 7 s to a controllable player
since `MP2_SKIP_INTRO` also skips the level-load text).

### Layout editor additions (MP1 sync package 8, item 5)

`TouchControlsView` now has MP1's editor: per-control opacity and hide, deleting stock controls,
adding your own buttons (up to 64, any digital input action, stored in the `touch_layout` string as
`k<slot>:dx,dy,scale,opacity,hidden,<action>[,page]`, cap 16 KB), and a Morph Ball page (Bomb,
Boost, Power Bomb, Spider, Unmorph) drawn only in the ball and split from the gameplay layout. The
Edit and Menu buttons cannot be hidden, deleted or faded out. Native side: the touch state is 128
bits (`PortInput::kTouchCount`; stock controls 0..35, ball page 40..44, added buttons 64+slot);
`PortInputDevices::SetTouchControl` feeds them, `BuildProfile` binds the added buttons
(`PortInput::BindTouchButtons`) and the ball page rows. JNI: `nativeTouchControl`,
`nativeTouchControlBound`, `nativeTouchBallLabel`, `nativeTouchButtonActions`.

### Custom Vulkan drivers (2026-10-27, from MP1)

On arm64 the port can load a custom Vulkan driver such as Mesa Turnip for Adreno GPUs through
[libadrenotools](https://github.com/bylaws/libadrenotools) (`extern/adrenotools`,
`platform/port_gpu_driver.cpp`), the same way Android emulators do, and takes the same driver zips
(a `meta.json` naming the library, plus that library). None is bundled.

F1 > Video > Compatibility > "Install driver (.zip)..." unpacks a zip into the app's internal
storage (`gpu_drivers/<id>/`; dlopen refuses shared storage) and selects it. The "Vulkan driver"
combo switches between installed drivers and System; `gpu_driver=<id>` (empty = System) takes
effect at the next start, and `MP2_GPU_DRIVER=<id>` overrides it for one run. "Running:" shows what
the GPU reports (e.g. `Mesa Turnip ...`), since adrenotools quietly falls back to the system driver
when its hooks fail. The OpenGL ES backend always uses the system driver.

While the GPU starts, `gpu_driver_starting` stays in the user folder, so a crash there reverts to
System at the next start, like the OpenGL ES toggle. If Vulkan does not start with the chosen
driver, Aurora retries with the system driver in the same run (`aurora_vulkan_library_failed`), and
`PortVideo::GpuStarted` switches the setting back to System and shows the error under the combo. A
driver that starts but draws garbage has to be switched back by hand.

A library named `vulkan.*` is installed as `mportv.*` with its `vulkan.*` SONAME patched to match:
Android's linker would otherwise return the system `vulkan.adreno.so` HWUI already loaded.

How it reaches Dawn: Dawn opens `libvulkan.so` by name from its search paths only. The port copies
`libmp2_vkshim.so` (exports only `vkGetInstanceProcAddr`, forwarding to the custom driver's) to
`<internal>/vkshim/libvulkan.so`, loads it, points it at the driver adrenotools opened, and puts that
folder first in Dawn's `additionalRuntimeSearchPaths` (`AuroraConfig::vulkanLibraryDir`). The shim
and adrenotools' hook libraries (`libhook_impl.so`, `libmain_hook.so`, `libfile_redirect_hook.so`,
`libgsl_alloc_hook.so`) are extra CMake targets in `android/app/build.gradle` and are loaded by path
from the native library dir (`useLegacyPackaging`). Not yet run on a device.

### Memory log and memory warnings (2026-10-27, from MP1 #102/#162/#187)

`platform/port_memory_log.cpp` (Linux and Android) writes to the log, and so to logcat (tag `mp2port`):
at startup `port: device memory: ...` (total/available RAM, low-RAM flag, memory class) and
`port: previous run: <reason> ...` from `ApplicationExitInfo` (Android 11+; the head of the ANR
trace when it was one), both built in `MetroidPrime2Activity.buildExitReport()` before SDL starts;
then `port: memory: process N MiB, system A of T MiB available, ..., dawn N MiB (textures, buffers,
allocated)` every 30 s or when process memory moves 64 MiB or system memory 128 MiB.
`onTrimMemory` (level >= 10) and `onLowMemory` log `port: memory warning` and call
`nativeMemoryWarning`, which evicts pipelines idle for 16 frames and asks Dawn to release GPU memory
(`aurora_reduce_gpu_memory`). Android always builds pipelines lazily, so the 5-s pressure check
(SETTINGS.md, "Pipeline memory") runs there too. Not run on a device (no installs from this lane).

### Data folder move to shared storage (MP1 sync, gpu-misc)

F1 > System > Data folder (`platform/port_data_folder.cpp`, Android only) moves `user/` (settings,
memory card, save backups, mods, save states, texture pack) from app storage to
`/storage/emulated/0/MetroidPrime2/`, and back. The cache folder and `disc.iso` stay in app storage.
Flow: ask for "All files access" (`MANAGE_EXTERNAL_STORAGE`, Android 11+; the legacy storage permission
on 9/10) through `MetroidPrime2Activity.hasStorageAccess/requestStorageAccess`, copy with a progress bar
(each file under a `.mpcopy` name, renamed when complete; `.mpcopy_incomplete` marks a cut-short copy),
offer "Use the data already there" when the target holds data, write the choice as the `user=` line of
`port_paths.txt` (other lines kept; `WriteUserLine`), then `restartApp` (`RestartActivity`, own process).
"Delete the old copy" frees app storage afterwards. If the chosen folder is unwritable at the next start
(access revoked, reinstall), `ResolveAndroidPaths` (`platform/main.cpp`) runs from app storage that once and
sets `MP2_USER_UNAVAILABLE`, which the panel reports with buttons to grant access again or stay. Unlike MP1 there is no
startup dialog for that case and no `data_folder.txt`: `port_paths.txt` is the only marker. Engine test:
`port_data_folder_tests`. Not run on a device (no installs from this lane).

## 5. Risks, worst first

- **REL arena placement** (M0): the thing that could have made the whole plan collapse. **Resolved**
  - see the M0 detail: it was a compile error (`MAP_32BIT`) plus a placement strategy, and the
  phone measurement says a 32 MiB arena lands below 4 GiB with the console's `0x81000000` region
  free. What remains unmeasured is placement in the *real* app's process, which has SDL, Vulkan and
  Dawn mapped by then (see the caveat there).
- **Tombstones instead of backtraces**: without `execinfo`, a crash on device is only a tombstone;
  `adb logcat` + `ndk-stack` is the workflow to write down before debugging on the phone.
- **Touch mapping into MP2's input layer** (M3): MP2's platform layer is smaller than MP1's (no
  input-settings module, no touch pad), so expect a small refactor, and expect the look-stick to be
  the fiddly part.
- **Performance unknown**: MP2 on a phone GPU is untested; the port's software-Vulkan present path
  is a desktop workaround and must not be copied - Android needs the real SDL/Aurora surface.
- **The game itself is incomplete** (see `AGENTS.md` Status): the APK will hit stubs, missing
  modules and the sweep's walls. That is expected for a tech demo, and the phone is actually a
  useful second front for that work - *if* the crash reporting is in place first (logcat + tombstone).
