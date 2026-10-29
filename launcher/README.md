# PrimedGun Echoes launcher

`PrimedGunEchoes.exe` is PrimedGun's launcher window for the native Metroid
Prime 2: Echoes port. It is the PrimedGun v2 (Metroid Prime) launcher, adapted
to this port. It is a separate Qt 6 program that sits next to
`metroid_prime2_port.exe`, edits the game's `settings.cfg`, and starts the
game. The tabs are Setup, Controller, Calibration, Cannon Textures, Layout,
Port Config and About.

## Building

The launcher is off by default. To build it, turn it on and point CMake at a
Qt 6.5+ install, for example the prebuilt Qt that PrimedGun's Dolphin tree
carries:

```
cmake -S . -B build/vr -DMP_BUILD_LAUNCHER=ON ^
  -DCMAKE_PREFIX_PATH=C:/path/to/PrimedGun/Externals/Qt/Qt6.5.1/x64
cmake --build build/vr
```

`PrimedGunEchoes.exe` is built next to the game. The build then runs
`windeployqt`, which copies Qt's DLLs and `platforms/`. `cmake --install`
installs the same files. Its version is `MP_PRIMEDGUN_VERSION` in the root
`CMakeLists.txt`, which the game's `--version` prints too.

Qt's prebuilt `Qt6EntryPoint.lib` is compiled with `/GL`, which lld-link cannot
read. The target therefore sets `qt_no_entrypoint` and links with
`/ENTRY:mainCRTStartup`.

The Qt-free half, `core/`, is always built, and `port_launcher_tests` checks
it: settings-file editing, the key table against `PortVrSettings` and the
game's clamps, the disc check and the settings imports. With the launcher on,
`port_launcher_settings_sync_tests` and `port_controller_layout_ui_tests` check
the window (label `port`).

## How it behaves

- **Settings.** The launcher finds the game's user folder the same way the
  game does (`platform/main.cpp` `DesktopUserFolder`, mirrored in
  `ui/tab_context.cpp`): `MP2_USER_PATH`, else `user/` beside the executable,
  else the per-user folder. A save re-reads `settings.cfg` and rewrites only
  the keys the launcher changed. Every other line stays as the game wrote it.
  - The file's rules are the game's: a line starting with `#` is a comment,
    and anywhere else `#` is part of the value. The game writes `\n` line ends.
  - While the game runs, every edit is written as it is made (at most every
    150 ms). The game looks at the file a few times a second and applies the
    keys that changed (`platform/debug_ui/settings.cpp` `PollSettingsFile`):
    the `vr_*` keys, fullscreen, vsync, MSAA, anisotropy, render scale and
    quick resume. Enable VR waits for the next start.
  - What the game saves while it runs (F1, the headset's menu) shows in the
    launcher, keeping any edit not written yet.
  - The launcher's own state, such as the selected disc and the window position,
    is kept in `primedgun_launcher.ini` in the user folder.
- **Play** saves pending edits, then starts the game with the disc as its first
  argument. The game's output goes to `primedgun_last_run.log` in the user
  folder.
  - **Stop** closes the game's window, so the game exits normally. **Force
    Stop** is offered after 10 seconds.
- **Discs.** Metroid Prime 2: Echoes USA (`G2ME01`); PAL (`G2MP01`) and Metroid
  Prime Trilogy (`R3ME01`, `R3MP01`) are experimental, as the game says. ISO
  (NKit too), GCM, RVZ, WIA, CISO and WBFS headers are read; GCZ is taken on
  trust and checked by the game.
- **Import PrimedGun Settings** takes a folder you pick: a PrimedGun v2
  (Metroid Prime) install, whose `user/port_settings.ini` `vr_*` keys come over
  as they are, or the old PrimedGun on Dolphin, whose `PrimedGun.ini` is
  translated. They load as unsaved changes. The cannon texture slot stays
  behind.
- **Memory cards.** The launcher does not transfer cards. The game shares or
  mirrors a Dolphin or PrimeHack card itself (in game, F1 > System > Memory
  card).
- **Open Full Port Options (F1)** writes `open_options.request` in the user
  folder. A running game opens its F1 window and comes forward; otherwise the
  game starts with the window open (the Echoes port has no options-only start).
- **Cannon Textures** imports your own textures for Echoes' arm cannon (23
  textures: the parts every beam shares, then each beam's own) into five slots
  and applies one. No preset ships yet, so Remove Shine reports a missing
  texture (`docs/PRIMEDGUN.md`, "Cannon texture slots").

## The Quest launcher

The Quest APK (`quest/`) opens on the same launcher as a 2D Horizon OS panel
(`quest/app/src/main/java/org/primedgun/echoes/launcher/`). Its native half,
`jni/` (`libprimedgun_launcher.so`, built with `-DMP_BUILD_QUEST_LAUNCHER=ON`),
puts `core/` behind JNI, so both launchers edit `settings.cfg` with the same key
table and line-preserving editor. Where the Quest panel differs:

- The game runs in its own process (`PrimedGunVrActivity`, `:game`); the tabs
  lock while it lives. Select Game copies the picked image to `disc.iso` in the
  user folder, `/sdcard/Android/data/org.primedgun.echoes/files`.
- Import PrimedGun Settings takes a picked file: PrimedGun v2's
  `port_settings.ini`, a `PrimedGun.ini`, or a Dolphin user data zip (settings
  only; memory cards are refused).
- Open Full Port Options says the F1 options are PC-only; the VR menu holds
  the PrimedGun settings in the headset.
