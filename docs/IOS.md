# iPhone and iPad

## Status

**Experimental, and untested on real hardware.** The build is made in CI on GitHub's macOS
runners; nobody working on the port owns an iPhone or iPad. It is the same build as the Prime 1
port's (`../MetroidPrimePort/docs/IOS.md`), which starts in the iOS Simulator. Please open an
issue and attach the log (see [Where things are](#where-things-are)).

- iOS / iPadOS 16.0 or newer, 64-bit (arm64). Metal through Dawn, like desktop.
- **Touch controls** are drawn in C++ (`platform/debug_ui/touch_overlay.cpp`, the Prime 1 iOS
  overlay adapted to MP2: Visor/Beam wheels, Default/Classic/Twin stick layouts, radial menus, minimap
  tap) and feed the same virtual gamepad as Android (`touch_pad.cpp`). Not yet checked on a device;
  the in-overlay Edit button and Menu-only pause mode of Android's `TouchControlsView` are not ported
  (F1 > Controls > Edit layout works). A paired controller works too.
- Mobile UI gates use `PORT_MOBILE` (`platform/include/port_mobile.h`: Android or iOS); Android-only
  APIs (JNI, storage picker, gallery export, texture-pack picker) stay on `__ANDROID__`.
- All REL modules are linked into the executable, so nothing maps executable code at run time.
- A phone with 4 GB of RAM or less may run out of memory.

## Getting the app

The `.ipa` is **unsigned**; sign it yourself with a free Apple ID.

1. Download `MetroidPrime2Port.ipa` from a release, or the `metroid_prime2_port-ios` artifact of a
   green [ios workflow](../.github/workflows/ios.yml) run.
2. Sign and install it with [AltStore](https://altstore.io), [SideStore](https://sidestore.io) or
   [Sideloadly](https://sideloadly.io).
3. On iOS 16 or later, turn on **Settings > Privacy & Security > Developer Mode**, then trust your
   Apple ID under **Settings > General > VPN & Device Management**.

A free Apple ID signature lasts **7 days** (refresh with the same tool; data is kept) and allows at
most 3 sideloaded apps.

## The disc

You need your own Metroid Prime 2: Echoes disc image (GameCube USA, or the other supported discs
in the README). On first start a document picker opens: pick the image (`.iso`, `.gcm`, `.rvz`,
`.wbfs`, `.ciso`, `.nkit`) from Files, iCloud Drive or a USB drive. It is moved into the app's
folder. You can instead copy it into **Files > On My iPhone > Metroid Prime 2 Port** first; the
game finds it there without asking.

## Where things are

Everything the game writes is in **Files > On My iPhone (or iPad) > Metroid Prime 2 Port**: the
log (`metroid_prime2_port.log`, when the `log_file` setting is on), `settings.cfg`, `controls.toml`, the memory card (`USA/Card A/`) and your disc image. Shader
caches stay in the hidden Library/Caches folder; iOS may clear them. Deleting the app deletes the
folder: copy your saves out first.

## Building

`.github/workflows/ios.yml` (manual) configures CMake for `iphoneos`, builds `metroid_prime2_port`
and runs `tools/ios_package.sh`, which makes `dist/MetroidPrime2Port.ipa` (icons, licences, ad hoc
signature, a zipped `Payload/`). It needs `llvm` from Homebrew (`LLVM_BIN`). On a Mac,
`tools/ios_sim_smoke.sh` starts an `iphonesimulator` build up to the disc prompt; CI does not run it.
