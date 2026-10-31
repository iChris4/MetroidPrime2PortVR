# Android

The Android build is shipped: `metroid-prime2-port-X-android-arm64.apk` on the release page (current
`versionName "0.6.2"`, `versionCode 8`, `applicationId org.metroidprime2.port`, arm64-v8a only,
minSdk 28, targetSdk 34, NDK 29.0.14206865; `android/app/build.gradle`). The original plan, the M0-M4
milestone logs and the risk list (written 2026-10-17, when nothing was built) are in
[history/ANDROID-to-2026-10-29.md](history/ANDROID-to-2026-10-29.md).

## Build

`tools/android_apk.sh :app:assembleRelease` (resolves SDK/NDK/Gradle, writes `android/local.properties`)
-> `android/app/build/outputs/apk/release/app-release.apk`. `tools/android_probe.sh` cross-builds only
the native `.so`. CI: `.github/workflows/android.yml` (started by hand; see [RELEASING.md](RELEASING.md)).
Native build: AGP `externalNativeBuild` over the top-level `CMakeLists.txt`; Dawn comes from the pinned
prebuilt `android-aarch64` package (`build/android-deps/dawn_prebuilt` when present, else downloaded)
and SDL3 from vendored sources.

## Signing

`tools/make_android_keystore.sh` writes `android/keystore.jks` and `android/keystore.properties` (both
git-ignored); CI passes the same four values as `MP_APK_KEYSTORE`, `MP_APK_KEYSTORE_PASSWORD`,
`MP_APK_KEY_ALIAS`, `MP_APK_KEY_PASSWORD`. With no key, `android_apk.sh` signs with the shared debug key
only when asked (`-PmpDebugSigning=true`) and otherwise refuses; `build.gradle` refuses a
half-configured key. Android will not replace a package signed with another certificate, and
uninstalling deletes the app's data (`files/disc.iso`, `files/user/`, `touch.cfg`, `port_paths.txt`), so
re-pick the ISO and copy saves back. Keep the key: losing it loses in-place updates.

## Install and run

`adb install -r app-release.apk`, or copy it to the phone (allow unknown sources). First launch opens the
system file picker: choose a supported disc image; it is checked and copied to `files/disc.iso`, and
later launches skip the picker. Data lives in `/sdcard/Android/data/org.metroidprime2.port/files/`:
`user/` (card, `touch.cfg`), `cache/`, `disc.iso`, `port_paths.txt` (`env.NAME=value` lines set
environment variables, e.g. `env.MP2_SKIP_FRONTEND=1`; `user=` is the data folder, see below).
The log is `metroid_prime2_port.log` (previous run `.old.log`) in the same folder; `MP2_LOG_FILE=0`
turns it off. Touch controls and the F1 MENU pill: [SETTINGS.md](SETTINGS.md).

## Reading a crash

The port prints `[port] caught signal N at <addr>` to logcat (tag `mp2port`) before it re-raises, so
`adb logcat -d | grep -A30 'port\] caught'` is the first thing to read. When the process dies without
one (an abort, a JNI/Java crash), `libsigchain` dumps frames under `E libsigchain:`; `ndk-stack -sym
<dir with libmetroid_prime2_port.so> -dump crash.txt` symbolises a tombstone (unstripped `.so`:
`android/app/build/intermediates/cxx/RelWithDebInfo/*/obj/arm64-v8a/`). `platform/main.cpp` also prints
`metroid_prime2_port: build <git hash>[-dirty]` at startup (`adb logcat -s mp2port | grep build`).

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

### Memory card export (issue #23)

SDL3 has no folder picker on Android, so "Export to folder..." did nothing there. F1 > System >
Memory card shows "Export..." instead (`platform/debug_ui/card_data.cpp`): the card's files are
queued (`PortSave::ExportBytes`) and `SDL_ShowSaveFileDialog` opens once per file with its name
suggested through `SDLActivity.setNextSaveDialogSuggestedFilename` (EXTRA_TITLE of
ACTION_CREATE_DOCUMENT). The Trilogy choice picks a source save.bin with the open dialog, then saves
the updated copy through the save dialog. The content:// URI is written with `SDL_IOFromFile`.
"Export to Dolphin/PrimeHack", "Open card folder" and the emulator section are hidden on Android.
On every platform a dialog that fails now says why in the status line. The APK builds
(`tools/android_apk.sh :app:assembleRelease -PmpDebugSigning=true`, 2026-10-30); not yet run on a device.

### Map screen on wide screens (issue #23)

The map screen's window is spread with the screen (`SetSpreadPanel` in `CAutoMapper`), but the map
is depth-tested against the HUD's automapper mask, which stayed at its authored size, so on a wide
screen the map was cut to a central box. `CInGameGuiManager` now widens the mask in view-space X by
`CAutoMapper::PortMapScreenWindowWiden()` (the window's spread over its authored half-width, 0.83),
blended in as the minimap grows into the map screen. Checked at 2340x1080 under Xvfb: a zoomed,
panned map reaches the frame's inner edges on both sides.

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
