# PrimedGun Echoes

**Metroid Prime 2: Echoes in native VR, on Windows PC and standalone Meta Quest.**

PrimedGun Echoes carries the VR layer of
[PrimedGun v2](https://github.com/iChris4/MetroidPrimePortVR) (Metroid Prime in VR, by Nobbie and
iChris4) onto [Echoes Unbound](https://github.com/Odrannnn/MetroidPrime2Port), Odrannnn's native
port of Metroid Prime 2: Echoes. It keeps the controls and VR experience of
[the original PrimedGun](https://github.com/Nobbie248/PrimedGun).

**This is an alpha.** Expect rough edges, and keep normal in-game saves as well as save states.

**No game content is included.** You need your own copy of the game: the port reads it from a disc
image and never ships any of its assets. This repository ships no disc image, and neither does any
package or release built from it.

## Features

- Native OpenXR VR on PC (Direct3D 12, or Vulkan for SteamVR headsets such as PSVR2) and standalone Meta Quest (tested on Quest 3).
- Room-scale movement with a modern VR control scheme, snap or smooth turning.
- 6DOF arm cannon tracking for aiming, grapple, lock-on and left-handed play.
- One-click height calibration and adjustable cannon position and rotation.
- Beam wheel and visor gestures configurations.
- 3D map, pause screens and message boxes in the headset.
- In-headset settings for controls, calibration, graphics, and more.

## Getting started

1. Get the latest build from the [releases page](https://github.com/iChris4/MetroidPrime2PortVR/releases).
   **PC:** extract the `Windows-x64.zip` and run `PrimedGunEchoes.exe`. **Quest:** sideload the
   `Quest.apk` (SideQuest or `adb install`) and open PrimedGun Echoes from Unknown Sources.
2. Use **Select Game** to choose your own Metroid Prime 2: Echoes disc image. USA (`G2ME01`) is the
   tested release; PAL (`G2MP01`) and Metroid Prime Trilogy (`R3ME01`, `R3MP01`) are experimental.
   ISO, NKit ISO, GCM, RVZ, WIA, CISO and WBFS images are accepted.
3. On PC, set your headset's OpenXR runtime as active and keep **Enable VR** on in **Port Config**.
   Press **Play**.
4. Check the **Layout** tab for the controller bindings.
5. In game, click the right stick to set your height. Click the left stick to open or close the VR
   menu.
6. Use **Calibration** to place the arm cannon. **Cannon Textures** applies textures you import to
   the cannon.

To bring over your VR settings from PrimedGun v2 (Metroid Prime) or from PrimedGun on Dolphin, use
**Import PrimedGun Settings** in the launcher. Memory cards are not transferred by the launcher; the
game can share or mirror a Dolphin or PrimeHack card itself (in game, F1 > System > Memory card, PC
only).

## Where your data is

- **PC:** the `user` folder next to the executables: settings (`settings.cfg`), memory cards, save
  states, logs and caches. Keep it when you update by hand: copy `user` into the new release's
  folder. The launcher's own updater keeps it in place.
- **Quest:** `/sdcard/Android/data/org.primedgun.echoes/files`. Install a new APK over the existing
  app to keep your saves; uninstalling removes them.

## Updating the PC launcher

**Setup > Check for updates...** in `PrimedGunEchoes.exe` (from v1.0.0-alpha.2) asks this
repository's GitHub releases whether a newer version is out, with no GitHub login, and shows its
release notes. **Include alpha, beta and release candidate versions** is on by default while a
preview is installed.

**Download update** downloads the Windows package and checks its SHA-256 (GitHub's digest of the
file, else the release's `SHA256SUMS.txt`); a package that cannot be verified is not installed.
**Install and restart launcher** then waits for the game and the launcher to close, copies the new
files over the installation, keeps the replaced ones in a backup folder and puts them back if
anything fails, then starts the new launcher. The `user` folder is never touched. Nothing is
installed until you ask. The Quest has no updater yet: install the new APK over the old one.

## Not done yet

- Echoes-only systems keep the original pad behaviour for now: the Seeker missile multi-lock, the
  Screw Attack's camera, turrets.
- Cannon Textures has no preset textures for Echoes' cannon yet, so Remove Shine does not work.
- Local multiplayer is not supported in VR.

## Building

See [docs/PRIMEDGUN.md](docs/PRIMEDGUN.md) for the VR layer's engineering notes,
[launcher/README.md](launcher/README.md) for the launchers, and `licenses/SOURCE.txt` in each release
for the exact build commands of that release. The port itself builds as upstream documents it
([docs/RUNNING.md](docs/RUNNING.md), [docs/ANDROID.md](docs/ANDROID.md)); VR needs
`-DMP_ENABLE_OPENXR=ON` (on by default on Windows), the PC launcher `-DMP_BUILD_LAUNCHER=ON` and Qt
6.5, and the Quest APK `quest/Build-Quest.ps1`.

Branches: `OpenXR` holds the VR work; `master` tracks upstream's `port` branch.

## Credits and licensing

- PrimedGun Echoes: iChris4. PrimedGun's VR experience and controls: Nobbie and iChris4.
- Echoes Unbound, the native port this builds on: Odrannnn and its contributors, from the
  PrimeDecomp Echoes decompilation.
- Aurora (`extern/aurora`), MusyX (`extern/musyx-port`) and the other third-party components keep
  their own licences; release packages carry them in `licenses/`.

`LICENSE` restores the original PrimeDecomp CC0 dedication, unchanged. The native port's MIT licence is preserved in
[`packaging/licenses/native-port-MIT.txt`](packaging/licenses/native-port-MIT.txt)
and covers this project's own work: the platform layer, the VR layer (`platform/vr`,
the stereo renderer files in `extern/aurora`, the launchers), the build and packaging scripts. It
does **not** cover the game code under `src/` and `include/`, which comes from the decompilation and
remains Nintendo's. See `NOTICE` for the exact scope and the third-party terms. `LICENSE` is
upstream PrimeDecomp's dedication, kept as it was.

Metroid Prime 2: Echoes is a trademark of Nintendo. This project is unaffiliated with Nintendo and
Retro Studios, and ships no game data.

## Documentation

| Document | What it covers |
|---|---|
| `docs/PRIMEDGUN.md` | The VR layer: what was ported, how, and what was checked |
| `launcher/README.md` | The PC launcher and the Quest launcher panel |
| `docs/SETTINGS.md` | The port's settings and environment variables |
| `docs/RUNNING.md` | Running the port: harness, console scripts, sweeps |
| `docs/PAL.md`, `docs/TRILOGY.md` | The PAL and Trilogy discs |
| `docs/MODS.md` | Mods, suit colours, texture packs |
| `PORT_NOTES.md` | How the native port works (upstream's notes) |
