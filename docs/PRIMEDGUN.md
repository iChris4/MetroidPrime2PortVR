# PrimedGun Echoes: engineering notes

PrimedGun Echoes is the VR layer of PrimedGun v2 (iChris4/MetroidPrimePortVR, the Metroid Prime
VR fork) carried onto this port of Metroid Prime 2: Echoes. This file is the fork's own log,
newest first, kept apart from `PORT_NOTES.md` (upstream's) so upstream merges never conflict on it.

- Branches: `master` tracks `upstream/port` (github.com/Odrannnn/MetroidPrime2Port); VR work is
  on `OpenXR`.
- The Metroid Prime VR fork (`../MetroidPrimePortVR`, branch `OpenXR`) is the reference for every
  VR file; keep the shared files byte-identical where possible and list the deltas here.

## Parity, part 7: custom layout gate, wrist visor pick (2026-10-29)

- `CTweakPlayerControls::GetMapping`: the port's Scan/Echo visor swap (F1 Controls > Options) is
  off while the VR custom layout drives the pad (`VrCustomLayoutActive`): its visor bindings name the
  visors by the game's own map, as the Metroid Prime VR port's gate does. Echoes' second control
  scheme (`gpTweakPlayerControlsB`) belongs to multiplayer, and its command overrides are never set,
  so nothing else needs the gate. Echoes' touch wheels switch visors and beams outside the control
  mapper (Android only), so the Metroid Prime port's touch-wheel gates have no counterpart here.
- `CPlayer::UpdateVisorState`: the wrist visor display's pick (`VrTakeWristVisorRequest`) selects
  that visor for player 0 like a visor button, unless the scan virus holds the scan visor. It was
  queued by the VR pad but never taken until now.

A/B note: again the first VR-off run after the build differed in the birds only; the repeat was
identical. The first launch of a freshly built executable is slower, which looks like the cause.

## Parity, part 6: camera hints interpolate in morph ball (2026-10-29)

Echoes already interpolates most hint changes the Metroid Prime port cut (`CCameraHintManager`
`SetHint` and `ClearHint` call `SetupInterpolation`), and `CInterpolationCamera::SetInterpolation`
already turns a zero-length handoff into a short eased one in the headset. What still cut was
`SetHint`'s other branch (hint flag 0x20, or 0x08000000 when forced): a camera switch with a
screen flash, or `SetPlayerCamera` ending a running interpolation. In the headset, in or entering
morph ball, outside cinematics and without flag 0x2000, that branch now interpolates from the
current camera to the hint's over the hint's own time (`CameraTransition::Duration`), leaves out
`SetPlayerCamera` (it would end the interpolation at once) and the flash. The clamp-velocity path
of `ClearHint` (flag 0x1000) is a smooth chase already and stays.

Not checked in play: no hint with those flags is reachable from the desk runs. A/B note: the first
VR-off run after a build differed in the birds only, and both repeats were identical.

## Parity, part 5: the wrist visor display (2026-10-29)

`vr_wrist_visor_enabled` (off by default) shows the four visors on the off-hand wrist, drawn with the
HUD visor menu's own icon and lozenge models.

- `CHudVisorBeamMenu::Update` (visor menu) hands its items' model widgets, by visor, to
  `VrWristCaptureModels`; `CGuiModel`'s destructor forgets them (`VrWristForgetModel`).
- `CGuiModel::PortDrawWrist`: the model's material and colour path (Draw's switch, without the depth
  test or write, outside the HUD's batch) at a given transform, at its last HUD draw alpha
  (`mPortHudDrawAlpha`, noted in `Draw`) times the wrist's.
- `vr_wrist_visor.cpp` differs from the Metroid Prime VR port's here: Echoes' GUI models draw from
  their frame's model database, with no model token to keep, so only the widgets are kept and each
  draws itself, its bounds from `PortGetBounds`. The pose maths are the same; the slots map to
  Echoes' visors by index (Combat 0, Echo 1, Scan 2, Dark 3).

Checked on the desk only with a temporary pose ahead of the camera (the controllers report as not
connected on the desk, so the VR pad and the wrist stay off): the icons and lozenges draw in both
eyes with their HUD materials. Visors not yet owned keep the menu's own zero alpha, so they do not
show at all.

A/B note: a VR-off run occasionally differs from the baseline in the birds only, run to run, with
an unchanged binary; repeat the run before blaming a change.

## Parity, part 4: pause screen models in 3D (2026-10-29)

Echoes has no dolls: its pause screen (Data Network) is one 3D scene, the scan tree and its nodes
plus the selected scan's or item's models, under the logbook camera. `CPauseScreen::DrawModels`
wraps `DrawModelView` in `PortVr::VrPauseModel3D` (the Metroid Prime VR port's dolls scope): on the
held screen the models stand in 3D a little in front of it, at the size of their flat picture. The
models stand at the world origin's depth (`GetDefaultModelPosition` has no depth offset), which is
where the scope puts the screen. The scan tree stays flat on the screen. The scan-in sweep is an
orthographic draw: under the 3D route it covers each whole eye, masked by that eye's model alpha.

Checked on the desk: the Missile Launcher entry shows its model in both eyes, 9 px (of 3400) more
crossed than the screen's frame and text, which all shift alike. Harness: a console `press` of the
same button twice while paused needs a `stick 0 0 1` between them (a new hold replaces the old one,
and holds only count down on game updates, which do not run while paused).

## Parity, part 3: the held 3D map, vsync off in the headset (2026-10-29)

- `CAutoMapper::PortVrSetHologram` (statics, set and cleared by `CInGameGuiManager::Draw`): `Draw`
  scales the map camera's space alike in every axis (the view's half-height to the hologram's) and
  sets it ahead of the held anchor, instead of squashing it onto the HUD plane. Only the depth is
  compressed, and only when the near side of the visible areas' volume (`CMapWorld::PortWorldVolume`,
  measured by `RecalculateWorldSphere`) would come closer than the clearance; the compression eases
  out and back in across the world map (hex view) transition. While the map opens or closes it grows
  from the HUD minimap's place and size (`mapGrow` from the mapper's interpolation).
- The frame and compass are drawn first on the held screen (`PortVrDrawFrame`, the end of `Draw`
  alone); `Draw` skips them in the hologram. The player marker draws without the depth test (no
  minimap mask to test against).
- `GetMiniMapCameraOrientation` and the minimap's `camXf`: the HUD minimap turns with the head's yaw
  (level right axis), not the body, for player 0 while immersive.
- `CInGameGuiManager::Draw`, map in view: the Metroid Prime VR port's sequence (frame on
  `ANCHORED_SCREEN` / `FLOATING_HUD`, then `ANCHORED_3D` with `AuroraSetStereoPlacement3DFollowHead`
  from the minimap's head-locked place, a 55° mono perspective for the desktop mirror, identity view,
  depth test off), projection and view restored after.
- `PortOverlay::ApplyVsync`: no vsync while the OpenXR session runs (the headset paces the loop).

Checked on the desk (eye captures): the map grows out of the minimap corner, sits in the held frame
with its own disparity, its size against the frame matches the desktop map screen, the world map
(hex view) and the exit back to gameplay work, and it shrinks back into the minimap on close.
Harness note: a console `press` holds for game updates, and none run on the map screen, so a second
press of the same button there is no new edge: close with B (`press b 6`) after opening with Z.

## Parity, part 2: beam wheel highlight, left-handed weapons (2026-10-29)

- Beam wheel: Echoes keeps the Metroid Prime command slots for its beams (`kC_PowerBeam` Power,
  `kC_IceBeam` Dark, `kC_WaveBeam` Light, `kC_PlasmaBeam` Annihilator) on the same C-stick directions
  (the port's keyboard fallbacks say so), so the wheel's up/right/down/left pulses and its
  `GameBeamId` index pick and light the right Echoes beams. `CHudVisorBeamMenu::PortVrSetHighlight`
  (a member with a default initializer, no constructor change) turns the hovered box and icon white;
  `CSamusHud::UpdateVisorAndBeamMenus` feeds it `VrBeamWheelHoverBeam` in first person.
- Left-handed (`vr_use_right_hand` off): `CPlayerGun::Render` draws the gun motion model and the
  beam/suit-arm models through `VrWeaponModelTransform` under `ModelMirrorScope`; the effects, the
  pose and the shots keep the tracked transform. `CGrappleArm::Render` and the grapple beam's start
  likewise. Not checkable on the desk (both controllers lie side by side).

## Parity, part 1: message boxes and menus in the headset view (2026-10-29)

The Metroid Prime VR port's immersive paused screens, on Echoes (defaults: `vr_message_box_immersive`
on, `vr_game_menu_screen_enabled` off):

- `CPauseScreenBlur`: `PortVrSetLiveWorld` keeps the world drawn under the blur and retakes the
  blur's copy every frame (per eye), drops the quarters' vignette and draws the scan lines in the
  world (`AURORA_STEREO_ROUTE_SKY`); leaving the live view mid-box takes one copy and stops the world
  as the TV does. The two flags sit in the retail bitfield.
- `CInGameGuiManager::Update` notes the message box / save prompt (`VrNoteMessageBox`) and the
  pause menu, logbook and map (`VrNoteGameMenu`), reloads dumped area textures when one comes into
  view, and runs `PortVrRefreshPausedScreens` (live world, held screen anchor: a box, the ball HUD,
  the map). `EnsureStates` skips the area texture dump while the world stays live.
- `CInGameGuiManager::Draw`: the visor and HUD stay drawn under a live blur and fade out as it comes
  up; the message box, the pause menu (with `VrResetEyeDepth`, `VrSetPauseScreenRoute`) and the quit
  prompt go on the held screen (`ANCHORED_SCREEN`, `FLOATING_HUD` with `vr_floating_hud`); the map
  screen's map is laid flat on the held screen (replaced by the held 3D map in part 3).
- `CMFGame::DrawGui`: the save prompt on the held screen.
- `PortVrHost::PublishGameMode`: the Metroid Prime port's classification (Message / Paused /
  Cinematic, FrontEnd while a menu waits for its area textures, held a few frames as a box closes);
  `ForgetStateManager` clears the notes and the anchor.

Checked on the desk: the pause menu and a paused HUD memo show on the held screen in both eyes over
each eye's live blur. The pause screen's 3D view is drawn flat on the screen (no 3D dolls yet).

Harness: `MP2_SHOT_AT` and `MP2_CONSOLE_AT` take `t<seconds>` entries and `MP2_EXIT_AT=<seconds>`
ends a run, for paused states (no game update runs there). Timed entries read the harness clock
only when present: the skip-intro flow times its steps on that clock, and reading it earlier changed
a plain run's world (the birds) though nothing else differed.

## Game hooks, part 2: cannon, controls, look targeting, visors (2026-10-29)

Same rules as part 1 (TARGET_PC, strip-diff CLEAN, `MP2_VR=0` pixel-identical, player 0 only).

- Cannon: `CPlayer::UpdateGunTransform` places the gun at `VrCannonTransform` in every holster
  state (`mGun->SetTransform`, Echoes' holster lift lives in `CPlayerGunBase::UpdateTransform` and
  is skipped), then the aim prediction and the assist. `CPlayerGun::Render` adds
  `VrCannonModelOffsetWorld`; its gun view (`PortGunView`) and relative pose use the room-scale
  view; no idle fidget (`VrNoArmCannonFidget`). Left-hand mirroring is not done yet.
- Recoil and rumble: `CRumbleManager` (TARGET_PC) has two more generators, ports 1 and 2, for the
  cannon's and the grapple's effects while the VR controllers own port 0 (handle bits 4-5 name
  the generator); `PortVrRecoil` replaces the recoil camera shakers of `CGunWeapon::Fire`,
  `CAnnihilatorBeam`, `CAuxWeapon::FireProjectile` and `FireLightCombo` with a gun-hand pulse.
- Controls: `VrPlayerSteers` and `VrApplySnapTurn` live in `vr_view.cpp` (Echoes delta; the
  Metroid Prime port has them as CPlayer members). `CPlayer::Think` runs the snap turn;
  `ComputeMovement` takes directional movement (`VrDirectionalMove`, default on: the pad then
  leaves the sideways stick to it) and the smooth turn's rate.
- Look targeting: `CPlayer::UpdateOrbitInput` runs `VrLookTargetUpdate` before Echoes'
  `UpdateOrbitableObjects`, which takes `VrLookToScanTarget` in the scan visor instead of the scan
  peek (`FindScanTargetId`) and `VrLookToLockTarget` in `FindOrbitTargetId`; a head-picked scan
  lock holds the body (`ValidateCurrentOrbitTargetId`, `UpdateOrbitOrientation`); the body points
  at a lock at once in the headset; `SetOrbitState` releases a lost target (`ReleaseLostTarget`).
- Reticles: they face the head (`VrHeadViewRotation`); the crosshair shows all the time in the
  headset and sits on the cannon ray's hit (`VrCannonTargetPoint`), with Echoes' Remastered
  cursor or the GameCube model; `CalculateClampedScale` measures from the head.
- Scan window (`CPlayerVisor::DrawScanEffect`): copy and pane `HEAD_LOCKED_2D`, dim `FULLSCREEN`,
  back to `HEAD_LOCKED`; `vr_scan_zoom` off copies the pane's own size.
- The wide HUD is off while the headset runs (`PortOverlay::HudWide`): its slices tore the helmet.
- Space warp: in the headset `CStateManager::DrawSpaceWarp` hands the warp to
  `CCubeRenderer::PortDrawSpaceWarpWorld` (a quad around the point facing the head, sampling a
  whole-view copy through `CGraphics::PortLoadScreenProjectionTexMtx` and
  `AuroraSetStereoScreenTexMtx`, half strength), or skips it with `vr_space_warp` off.
- Indirect-texture particles (`CElementGen::RenderParticlesIndirectTexture`): Echoes' PC path
  already copies the view once; in the headset the copy's coordinates come from the position
  through the same per-eye screen matrix, and a particle the body camera clips is still drawn when
  it is in front of the camera.
- Morph ball on the desk (console `press x`, `vr_controller_mode=gamepad` so the console's pad
  gets through): the ball view is level, its HUD on the held screen, the stand-up glide and the
  return to first person work. One run showed no cannon after the unmorph; two repeats with a
  diagnostic showed it drawn (holster state Drawn, alpha 1), so it was not reproduced.

Checked on the desk (SteamVR, PSVR2 lying on the desk, so the eyes look elsewhere than the body
camera): Dark Visor, Echo Visor, the scan visor's palette highlight and window, and the dark world
filter (Dark Agon, `MP2_WORLD="42b935e4,01_sand_entrance_dark"`) render in each eye from that eye's
view. Not checked: anything worn (scale, cannon offsets for Echoes' model, the helmet's look: its
top and bottom bands end short of the eye's field, comfort).

Tools for desk checks:

- `AURORA_SCREENSHOT_EYES=1` (aurora.cpp): a screenshot of a replayed immersive frame also saves
  both eye images (`screenshots/metroid-prime-<stamp>-eye<0|1>.bmp`, full size).
- `MP2_CONSOLE_AT=<update>:<command>[;...]` (port_harness.cpp): console commands at given game
  updates (`give darkVisor`, `visor dark`, `warp ...`), since the console socket is off on Windows.

## Game hooks, part 1: cameras, culling, HUD routes, sky, fog (2026-10-29)

Every hook sits in `#ifdef TARGET_PC` with the retail text under `#else`; `strip_diff.py master`
is CLEAN and `MP2_VR=0` runs are pixel-identical to the untouched port (three screenshots of the
new-game run, `MP2_SHOT_AT=300,700,1100`). Player 0 only throughout.

Cameras and culling:

- `CFirstPersonCamera::UpdateTransform`: a head-picked scan lock holds the body's facing
  (`VrLookHoldsFacing`), the headset owns the pitch (`VrFlattenLookPitch`), the action view turns
  the view during a lock or a grapple and hands the body the view's yaw on release
  (`vr_action_view.h`, member `mPortVrActionView`), no camera bob (`VrNoCameraBob`). Echoes' own
  mouse-aim and empty-lock pitch blocks stand down because direct aim does (below).
- `CCameraManager`: `GetCurrentCameraTransform` levels the morph ball camera, takes the head's
  entry yaw out once, keeps the unmorph base and adds the room-scale offset (`PortVrLevelBallCamera`);
  the audio listener follows the head; `PortCutNextUpdate` (for the snap turn).
- `CStateManager`: `VrCullingFrustum` in `PreRender`, `SetupViewForDraw`, `ResetViewAfterDraw`; the
  arm cannon's retail-FOV draw is off in the headset.
- Morph ball: `CPlayer::BeginMorphTransition` captures the head's heading (`VrBeginBallCamera`),
  turns the body and the first-person camera to it (Echoes' to-ball spline and `SetState` read the
  first-person camera); `BeginUnmorphTransition` starts the unmorph camera, `PrepareToLeaveMorphBallState`
  faces the stand-up model the head's way, `LeaveMorphBallState` gives the body the unmorph base;
  `CBallCamera::UpdateTransitionFromBallCamera` rises straight to the eye in that base;
  `CalculatePlayerControlDirection` rolls the ball along the ball camera's flat forward.
- `CInterpolationCamera`: in the headset a morph ball handoff blends straight to the target camera's
  transform with a bounded, eased duration (`vr_camera_transition.h`). `CCameraHintManager::ClearHint`
  starts from the simulation camera. Echoes' cut-to-smooth conversion of instant hints (MP1's
  `UseCameraHint` change) is left for the comfort pass: Echoes' hint code differs too much to copy.
- `CPlayer::Think`: `VrRoomScaleUpdate`.

Draw routes:

- `CGuiCamera::Draw` keeps the drawn tangents (`GetDrawTanHalfWidth/Height`).
- `CInGameGuiManager::Draw`: helmet `HEAD_LOCKED`; reticle `WORLD`; faceplate, visor, reflection,
  HUD and memo `HEAD_LOCKED` with the head-locked plane from the loaded HUD frame's camera at depth
  20; minimap `HEAD_LOCKED`, or on the morph ball HUD's screen; the wrist visor after (draws nothing
  yet). `Update`: `VrNoteInGameMenu` (paused states and the quit screen), cinematic bars off
  (`VrRemoveCinematicBars`), the morph ball HUD's screen anchor held while it is up.
- `CSamusHud::Draw`: the reticle `WORLD`, the ball HUD frame `ANCHORED_SCREEN` (`FLOATING_HUD`
  with `vr_floating_hud`).
- `CWorld::DrawSky` routed `SKY` (Echoes draws the sky dome at the camera's position, like MP1);
  `CWorldShadow::BuildLightShadowTexture` routed `SKIP` up to its copy.
- `CCubeRenderer`: plane objects are not discarded behind the body camera; the fog volume cap uses
  the head's facing and position; the fog composite covers the whole view.

Platform: the desktop's direct aim (mouse, twin stick, touch: `PortMouse::Enabled`, `StickEnabled`,
`TouchEnabled`, `Aiming`), the Scan/Echo visor button swap and the prompt glyph pick stand down while
the VR controllers own port 0 (`VrPadOwnsPort0`), as in the Metroid Prime fork.

Not done yet: particle billboards (`VrParticleBillboard` in `CElementGen`, `CParticleSwoosh`,
`CEnvFxManager`, `CHUDBillboardEffect`, `CVisorFlare`), the cannon, the controls' game-side gates,
the EFB effects per eye (dark world, Dark/Echo/Scan visors), immersive message boxes and menus
(until then they show on the virtual screen once the world stops being drawn).

## The VR host layer on Echoes (2026-10-29)

`platform/vr` and `platform/include/vr` come from the Metroid Prime VR fork, with its 18 GPU-free
tests (`tests/port_vr_*.cpp`, `cmake/PortTests.cmake`). They build as the object library
`mp_port_vr` (CMakeLists.txt), which is linked into `metroid_prime2_port` and passed to the REL
scoping step like the other platform objects. `MP_ENABLE_OPENXR` (ON on Windows) adds the OpenXR
loader (OpenXR-SDK 1.1.61, static, fetched) and the OpenXR-only files; off, the always-built set
compiles to stubs and the game's `PortVr` calls still link.

What differs from the Metroid Prime fork's files (line counts against it):

| File | Why |
|---|---|
| `vr/vr_host.h`, `vr/vr_host_echoes.cpp` (new) | What the VR layer needs from the port (settings file, the F1 switches, cheats, save states, user texture pack, the live game, the game mode), so `platform/vr` no longer calls one port's `PortDebug`. Worth taking back to the Metroid Prime fork. |
| `vr_menu.cpp` (85), `vr_settings.cpp` (12), `vr_settings.h` (8) | Through `PortVrHost`; `MP2_VR`, `MP2_VR_MIRROR`, `MP2_VR_LOG`, `MP2_FOVEATION`. |
| `openxr_integration.cpp` (2) | `MP2_FDM_DEVICE`. |
| `vr_pad.cpp` (28) | Player 0 of Echoes' per-player accessors; Echoes' visors take the Metroid Prime bits by number (echo where X-ray was, dark where thermal was). |
| `vr_view.cpp` (39), `vr_view.h` (2) | Player 0; the controls-frozen test is Echoes' control hints (`kDF_All`, `kDF_Movement`) plus the ice freeze and an attached actor; the ground mover is always `MoveGroundCollider_New`; the surface-culling volume only where `port_surface_culling_math.h` exists. |
| `vr_look_scan.cpp` (60) | Echoes' orbit code: `GetUpdateFrameIdx`, `ValidateOrbitTargetId` returns 0 for OK, grapple points need the player's area and `GetTweakPlayer()->GetGrappleDistance()`, `GetConstrainToAxis` for a swing-locked point, no default `TUniqueId`. |
| `vr_wrist_visor.cpp` (97) | Draws nothing yet: the wrist icons are the HUD visor menu's models, reached in the Metroid Prime port through `CGuiModel` additions. The selection logic (vr_pad) is there; the setting is off by default. |

Platform hooks:

- `platform/main.cpp`: after the settings load, `PortVrHost::SetUserFolder`,
  `ApplyVrEnvironmentOverrides`, `PushVrSettingsToAurora`; after `PortVideo::BeginGpuStart`,
  `OpenXRPrepareAurora(config)` (a renderer chosen in F1 other than Auto or D3D12 leaves the headset
  off for the run); `aurora_initialize`'s `AuroraInfo` kept; after the first `aurora_update`,
  `OpenXRStartAfterAurora`; `OpenXRShutdownBeforeAurora` before every `aurora_shutdown`.
- `platform/port_events.cpp`: `PortPaceFrame` waits for the headset's frame request instead of the
  60 FPS cap while the session runs; `PortPumpEvents` applies the controller state first and runs
  `VrPadUpdate` last, after the binding engine's `PADSetPortOverride` (Aurora merges the virtual pad
  after the override, `PADSetVirtualExclusive`).
- `platform/input/port_input_devices.cpp`: the headset's input focus counts as focus.
- F1: a VR page (`platform/debug_ui/search.cpp`, `PortVr::DrawVrDebugTab`).
- `settings.cfg`: `vr_<name>` keys, read through `PortVr::ApplyVrSetting` and written after the
  table (`platform/debug_ui/settings.cpp`). Not in the settings self-test yet.
- `platform/debug_ui/cheats.cpp`: Full health and Grant everything are functions
  (`PortDebug::CheatFullHealth`, `CheatGrantEverything`) the VR menu calls too.

Game hooks (inside `#ifdef TARGET_PC`; `tools/primedgun/strip_diff.py master` is CLEAN):

- `src/MetroidPrime/main.cpp`: the fixed 60 Hz tick clock runs whenever the headset paces the loop
  (Echoes uses it only with the cap off; capped, the retail loop would tick once per headset frame);
  interpolation always on under the headset; `PortVrHost::PublishGameMode` before `BeginScene`;
  `OpenXRServiceProducerFrameBoundary` once the scene is open; the last frame's world draw kept.
- `src/Kyoto/Graphics/DolphinCGraphics.cpp`: frames tagged with the policy's content tag
  (`aurora_begin/end_frame_tagged`), no video-rate pacing under the headset, `VrMenuUpdate` before
  the F1 overlay.
- `src/MetroidPrime/CStateManager.cpp`: `PortVrHost::SetStateManager(this)` each update,
  `ForgetStateManager` in the destructor. The front end's own state manager (game mode `FRND`)
  reads as no game.

Game mode (`PortVrHost::PublishGameMode`): in game when the last presented frame drew the world,
cinematic while a cinematic camera runs, paused in the map or a pause; anything else (front end,
transitions) on the virtual screen. Message boxes, the save prompt and the immersive pause menus
come with the game hooks.

Checked on this PC: `build/vr` 50/50 port tests (Echoes' 32 and the 18 VR tests). With
`MP2_VR=0`, and with VR on but no runtime reachable (`XR_RUNTIME_JSON` pointing nowhere: the
loader fails and the game falls back to the desktop), the new-game run's screenshots are
pixel-identical to the untouched port's. A desk session on the user's PSVR2 through SteamVR 2.18.4
(the active runtime): 120 Hz, eyes 3400x3468, the boot on the virtual screen and gameplay replayed
per eye at a steady 120 Hz with ~6.7 ms GPU render a frame (short skip bursts during area
streaming), the game ticking at 60 Hz underneath, clean session shutdown. Nobody wore the headset:
the picture, the pad and the controllers are unverified, and without the game hooks the HUD and
helmet are still drawn as world geometry.

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
