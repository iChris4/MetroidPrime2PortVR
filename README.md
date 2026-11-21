# PrimedGun Echoes

**Metroid Prime 2: Echoes in native VR, on Windows PC, standalone Meta Quest and Apple Vision Pro.**

PrimedGun Echoes carries the VR layer of
[PrimedGun v2](https://github.com/iChris4/MetroidPrimePortVR) (Metroid Prime in VR, by Nobbie and
iChris4) onto [Echoes Unbound](https://github.com/Odrannnn/MetroidPrime2Port), Odrannnn's native
port of Metroid Prime 2: Echoes. It keeps the controls and VR experience of
[the original PrimedGun](https://github.com/Nobbie248/PrimedGun).

**This is a beta.** Expect rough edges, and keep normal in-game saves as well as save states.

**No game content is included.** You need your own copy of the game: the port reads it from a disc
image and never ships any of its assets. This repository ships no disc image, and neither does any
package or release built from it.

## Features

- Native OpenXR VR on PC (Direct3D 12, or Vulkan for SteamVR headsets such as PSVR2) and standalone Meta Quest (tested on Quest 3).
- Apple Vision Pro (visionOS 26+, Metal) with PlayStation VR2 Sense controllers or a gamepad; see [docs/VISIONOS.md](docs/VISIONOS.md).
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
   ISO, NKit ISO, GCM, RVZ, WIA, GCZ, CISO and WBFS images are accepted. See the
   [disc compatibility table](#supported-discs-and-saves) below.
3. On PC, set your headset's OpenXR runtime as active and keep **Enable VR** on in **Port Config**.
   Press **Play**.
4. Check the **Layout** tab for the controller bindings.
5. In game, click the right stick to set your height. Click the left stick to open or close the VR
   menu.
6. Use **Calibration** to place the arm cannon. **Cannon Textures** applies textures you import to
   the cannon.
7. Use **Save Settings** after changing launcher options. **Play** also saves pending changes.

To bring over your VR settings from PrimedGun v2 (Metroid Prime) or from PrimedGun on Dolphin, use
**Import PrimedGun Settings** in the launcher. Memory cards are not transferred by the launcher; the
game can share or mirror a Dolphin or PrimeHack card itself (in game, F1 > System > Memory card, PC
only).

## Supported discs and saves

### Disc images

Use your own **Metroid Prime 2: Echoes** disc image. These are the releases accepted by the
launcher and game:

| Game release | Disc ID | Revision | Status |
|---|---|---|---|
| GameCube USA 1.00 | `G2ME01` | 0 | Tested; recommended |
| GameCube PAL | `G2MP01` | 0 | Experimental |
| Wii Metroid Prime Trilogy USA | `R3ME01` | 0 | Experimental; plays the Echoes portion |
| Wii Metroid Prime Trilogy PAL | `R3MP01` | 0 | Experimental; plays the Echoes portion |

Other game IDs and disc revisions are not accepted. Prime 1's compatibility list does not apply
to Echoes: Metroid Prime and Metroid Prime Remastered are not replacement discs for this game.
See [PAL notes](docs/PAL.md) and [Trilogy notes](docs/TRILOGY.md) for their current limitations.

| Image format | File types |
|---|---|
| Standard disc image | `.iso`, `.gcm` |
| NKit ISO | `.nkit.iso` |
| Dolphin compressed images | `.rvz`, `.wia`, `.gcz` |
| CISO | `.ciso` |
| WBFS | `.wbfs` |

Extract ZIP/7z archives before selecting an image. The desktop launcher accepts NKit images
ending in `.iso`; a standalone `.nkit` extension is not in its file filter.

### Saves and memory cards

| Save type | How to use it |
|---|---|
| Individual GameCube save (`.gci`) | Import an Echoes save with the full port options' **System > Memory card > Import file...** |
| Dolphin / PrimeHack GCI folder | Import its Echoes `.gci`, or use **Share memory card with emulator** on PC |
| Complete memory-card image (`.raw`, `.gcp`) | Export the Echoes save as `.gci` with Dolphin's memory-card manager first; the Echoes F1 file importer expects a `.gci` |
| Wii Trilogy `save.bin` | PC memory-card tools can convert/mirror Echoes progress through **Mirror with the Trilogy save.bin**; see below |
| Native save state / Quick Resume (`.mp2ss`) | Stored separately in `user/savestates/`; may depend on the disc and port version |
| Dolphin / PrimeHack emulator save state | Not compatible; load it in the emulator and make a normal in-game save first |

**On Windows:** select your game, start it and stay at the title/file-select screen. Open
**F1 > System > Memory card > Import file...** and choose the Echoes `.gci`. Imports back up the
existing save in `user/save-backups/`. Choose the normal save slot from file select afterwards;
Quick Resume or an older save state restores its own earlier session instead of the imported save.

**On Quest:** close the game before copying files. The normal USA card folder is
`/sdcard/Android/data/org.primedgun.echoes/files/USA/Card A/`; PAL uses `EUR/Card A/`.
Launch the game once to create its data folders, then copy a matching Echoes `.gci` into the card
folder through USB file access. Back up the existing card files before replacing them. If Quick
Resume restores your old session, turn it off temporarily and load the imported slot normally.

On Windows the equivalent folders are `user/USA/Card A/` and `user/EUR/Card A/` beside
`PrimedGunEchoes.exe`. USA/PAL GameCube and Trilogy card saves can be converted by the native
importer; use that importer rather than just renaming a different release's save. Only Echoes
progress is compatible: Prime 1 saves cannot be used here.

For PC sharing, return to the title screen and choose your Dolphin/PrimeHack user folder in
**System > Memory card**, then enable **Share memory card with emulator**. For Trilogy, use an
emulator profile with an existing Trilogy save that has been started once, then enable
**Mirror with the Trilogy save.bin**. This keeps its Echoes progress and the port's card in step.
A Wii-exported `data.bin` is not
the same file; use Dolphin's extracted `save.bin`. See [save compatibility notes](docs/SAVE_RELEASES.md).

**Import PrimedGun Settings** in the launcher imports VR settings, not memory cards. When moving
between native Echoes installations, copying the whole `user` folder preserves saves, settings
and save states. On Quest, install compatible updates over the existing app; do not uninstall.

## Installing HD texture packs

Use an **Echoes** replacement texture pack with `.png` or `.dds` files and their original
filenames (usually `tex1_...`). Extract the ZIP/7z first; copying the archive itself does not
install the textures. No ISO patch is needed. Use the pack version matching your disc: Dolphin
packs commonly put GameCube Echoes textures in `G2M` and Trilogy textures in `R3M`.

### Windows

1. Open **Port Config > Open Texture Pack Folder** in the launcher. This opens the folder your
   installation uses, normally `user/user_textures/` beside `PrimedGunEchoes.exe`.
2. Copy the extracted textures there, keeping their filenames. Named subfolders are supported,
   for example `user/user_textures/My Echoes HD Pack/tex1_...png`.
3. Restart the game, or use **F1 > Mods > Textures > Reload texture pack** while playing.
   Ensure **Use the texture pack** is enabled there.

### Meta Quest

1. Launch the app once, then open **Port Config > Show Texture Pack Folder** to create the folder
   and see its current path.
2. Connect the Quest to your computer, allow file access, and copy the extracted PNG/DDS files to
   `/sdcard/Android/data/org.primedgun.echoes/files/user_textures/`.
   In a USB file browser this is **Internal shared storage > Android > data > org.primedgun.echoes
   > files > user_textures**. Create `user_textures` inside `files` if it is missing.
3. A named pack subfolder is fine. Keep the original filenames and restart the game after copying.

Use one pack/version at a time to avoid conflicting replacements. Higher-resolution packs use
more memory, especially on Quest; try a smaller version if performance suffers. **Texture Pack
Help** in Port Config repeats the installation steps and shows your folder. **Cannon Textures**
is a separate appearance control, not the installation location for a full HD pack.
See [the texture-pack reference](docs/MODS.md) for advanced options.

## Full port options on Quest

In **Port Config**, tap **Open Full Port Options (F1)...** to open the native
settings in a touch-enabled Quest 2D panel. It needs no disc and shares settings
with the game. Close the panel to return to the launcher; graphics backend and
other startup-only changes take effect after restarting the game.

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
installed until you ask.

## Updating on Quest

From alpha.4, open **Setup > Check for updates...** in the Quest launcher. It checks
this Echoes repository, shows release notes and downloads the Quest APK. Alpha/beta
releases are included by default while running a preview; draft releases are hidden.
The download must pass its SHA-256 check and match this app's package and signing key.
Close the game and Port Options, then choose **Install update** and approve the Quest
installer. Quest may first ask you to allow this launcher to install apps. Updates
preserve saves and settings; never uninstall to update. Older versions can be updated
by sideloading the APK over the existing installation.

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

Apple Vision Pro: on a Mac with Apple silicon, Xcode with the visionOS platform, CMake 3.28+, Ninja,
Homebrew's `llvm` and Rust nightly with `rust-src`, run `visionos/Build-VisionOS.sh --team <TEAMID>
--install` (or `--simulator`). See [docs/VISIONOS.md](docs/VISIONOS.md).

Branches: `OpenXR` holds the VR work; `VisionPro` adds Apple Vision Pro; `master` tracks upstream's
`port` branch.

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
| `docs/VISIONOS.md` | Apple Vision Pro: building, installing, playing, how it works |
| `launcher/README.md` | The PC launcher and the Quest launcher panel |
| `docs/SETTINGS.md` | The port's settings and environment variables |
| `docs/RUNNING.md` | Running the port: harness, console scripts, sweeps |
| `docs/PAL.md`, `docs/TRILOGY.md` | The PAL and Trilogy discs |
| `docs/MODS.md` | Mods, suit colours, texture packs |
| `PORT_NOTES.md` | How the native port works (upstream's notes) |
