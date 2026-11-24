// PortOverlay's plain accessors, one row each; included once, inside namespace PortOverlay, by
// port_overlay.cpp. Declarations and their comments stay in include/port_overlay.h, the variables
// and their defaults in port_overlay_internal.h / port_overlay.cpp. A getter reads its MP2_*
// environment override first (ReadEnv, EnvBool/EnvIntIn/EnvFloatIn); a setter stores and saves.
// Anything with more logic than these shapes is written out in port_overlay.cpp.
//
//   PORT_GET_BOOL(Name, env, var)                 bool Name(): env override, else var
//   PORT_GET_BOOL_RETAIL(Name, retail, env, var)  the same, but `retail` in Original Experience
//   PORT_GET_INT_IN(Name, env, var, lo, hi)       int Name(): an env value outside lo..hi is ignored
//   PORT_GET_FLOAT_IN(Name, env, var, lo, hi)     the same for a float (non-finite is ignored too)
//   PORT_SET(Name, type, var)                     void Name(type): var = value, saved
//   PORT_SET_CLAMP(Name, type, var, lo, hi)       the same, clamped to lo..hi

#define PORT_GET_BOOL(Name, env, var)                                                              \
  bool Name() {                                                                                    \
    ReadEnv();                                                                                     \
    return EnvBool(env, var);                                                                      \
  }
#define PORT_GET_BOOL_RETAIL(Name, retail, env, var)                                               \
  bool Name() {                                                                                    \
    ReadEnv();                                                                                     \
    return OriginalExperience() ? retail : EnvBool(env, var);                                      \
  }
#define PORT_GET_INT_IN(Name, env, var, lo, hi)                                                    \
  int Name() {                                                                                     \
    ReadEnv();                                                                                     \
    return EnvIntIn(env, var, lo, hi);                                                             \
  }
#define PORT_GET_FLOAT_IN(Name, env, var, lo, hi)                                                  \
  float Name() {                                                                                   \
    ReadEnv();                                                                                     \
    return EnvFloatIn(env, var, lo, hi);                                                           \
  }
#define PORT_SET(Name, type, var)                                                                  \
  void Name(type value) {                                                                          \
    ReadEnv();                                                                                     \
    var = value;                                                                                   \
    Persist();                                                                                     \
  }
#define PORT_SET_CLAMP(Name, type, var, lo, hi)                                                    \
  void Name(type value) {                                                                          \
    ReadEnv();                                                                                     \
    var = std::clamp(value, lo, hi);                                                               \
    Persist();                                                                                     \
  }

PORT_GET_BOOL_RETAIL(SkipCinematics, false, "MP2_SKIP_CINEMATICS", sSkipCinematics)
PORT_SET(SetSkipCinematics, bool, sSkipCinematics)
PORT_GET_BOOL(SkipPromptBox, "MP2_SKIP_PROMPT_BOX", sSkipPromptBox)
PORT_GET_BOOL(HideHelmet, "MP2_NO_HELMET", sHideHelmet)
PORT_GET_BOOL(HideVisorEffects, "MP2_NO_VISOR_FX", sHideVisorEffects)
PORT_SET(SetHideHelmet, bool, sHideHelmet)
PORT_SET(SetHideVisorEffects, bool, sHideVisorEffects)
PORT_GET_BOOL_RETAIL(CinemaBars, true, "MP2_CINEMA_BARS", sCinemaBars)
PORT_GET_BOOL_RETAIL(SecondaryCrosshairs, true, "MP2_SECONDARY_CROSSHAIRS", sSecondaryCrosshairs)
PORT_GET_BOOL_RETAIL(BallBars, true, "MP2_BALL_BARS", sBallBars)
PORT_GET_BOOL_RETAIL(DarkVisorFullScreen, false, "MP2_DARK_VISOR_FULL", sDarkVisorFull)
PORT_GET_BOOL(AimCrosshair, "MP2_AIM_CROSSHAIR", sAimCrosshair)
PORT_SET(SetAimCrosshair, bool, sAimCrosshair)
PORT_SET_CLAMP(SetCrosshairStyle, int, sCrosshairStyle, kCrosshairStyle_Classic, kCrosshairStyle_GameCube)
PORT_GET_INT_IN(CrosshairSize, "MP2_CROSSHAIR_SIZE", sCrosshairSize, kCrosshairSizeMin, kCrosshairSizeMax)
PORT_SET_CLAMP(SetCrosshairSize, int, sCrosshairSize, kCrosshairSizeMin, kCrosshairSizeMax)
PORT_GET_INT_IN(CrosshairOpacity, "MP2_CROSSHAIR_OPACITY", sCrosshairOpacity, kCrosshairOpacityMin, kCrosshairOpacityMax)
PORT_SET_CLAMP(SetCrosshairOpacity, int, sCrosshairOpacity, kCrosshairOpacityMin, kCrosshairOpacityMax)
PORT_SET(SetGunFollowsFov, bool, sGunFollowsFov)
PORT_GET_BOOL_RETAIL(BallFollowsFov, false, "MP2_BALL_FOLLOWS_FOV", sBallFollowsFov)
PORT_SET(SetBallFollowsFov, bool, sBallFollowsFov)
PORT_GET_INT_IN(Anisotropy, "MP2_ANISO", sAnisotropy, 1, 16)
PORT_SET_CLAMP(SetAnisotropy, int, sAnisotropy, 1, 16)
PORT_SET_CLAMP(SetHudScale, int, sHudScale, kHudScaleMin, kHudScaleMax)
PORT_GET_BOOL_RETAIL(InfiniteBeamAmmo, false, "MP2_INFINITE_BEAM_AMMO", sInfiniteBeamAmmo)
PORT_GET_BOOL_RETAIL(ChargeOnlyBeamAmmo, false, "MP2_CHARGE_ONLY_BEAM_AMMO", sChargeOnlyBeamAmmo)
PORT_GET_BOOL_RETAIL(DarkWorldLightMusic, false, "MP2_DARK_WORLD_LIGHT_MUSIC", sDarkWorldLightMusic)
PORT_GET_BOOL_RETAIL(FastMorph, false, "MP2_FAST_MORPH", sFastMorph)
PORT_SET(SetFastMorph, bool, sFastMorph)
PORT_GET_BOOL(MinimapTap, "MP2_MINIMAP_TAP", sMinimapTap)
PORT_SET(SetMinimapTap, bool, sMinimapTap)
PORT_GET_BOOL(MapPickupKinds, "MP2_MAP_PICKUP_KINDS", sMapPickupKinds)
PORT_SET(SetMapPickupKinds, bool, sMapPickupKinds)
PORT_SET(SetRevealMap, bool, sRevealMap)
PORT_GET_BOOL(PointerMenus, "MP2_POINTER_MENUS", sPointerMenus)
PORT_SET(SetPointerMenus, bool, sPointerMenus)
PORT_GET_BOOL(MenuExit, "MP2_MENU_EXIT", sMenuExit)
PORT_GET_BOOL(MapSwapFingers, "MP2_MAP_SWAP_FINGERS", sMapSwapFingers)
PORT_GET_BOOL(MapInvertX, "MP2_MAP_INVERT_X", sMapInvertX)
PORT_GET_BOOL(MapInvertY, "MP2_MAP_INVERT_Y", sMapInvertY)
PORT_GET_BOOL_RETAIL(BallCamControl, false, "MP2_BALL_CAM", sBallCam)
PORT_GET_INT_IN(BallCamSpeed, "MP2_BALL_CAM_SPEED", sBallCamSpeed, kBallCamSpeedMin, kBallCamSpeedMax)
PORT_GET_BOOL(WalkSpeedEnabled, "MP2_WALK_SPEED_ENABLED", sWalkSpeedEnabled)
PORT_SET(SetWalkSpeedEnabled, bool, sWalkSpeedEnabled)
PORT_SET_CLAMP(SetWalkSpeed, int, sWalkSpeed, kWalkSpeedMin, kWalkSpeedMax)
PORT_GET_BOOL_RETAIL(Prime1AirPhysics, false, "MP2_PRIME1_AIR_PHYSICS", sPrime1AirPhysics)
PORT_SET(SetPrime1AirPhysics, bool, sPrime1AirPhysics)
PORT_GET_BOOL_RETAIL(Prime1BoostBall, false, "MP2_PRIME1_BOOST_BALL", sPrime1BoostBall)
PORT_SET(SetPrime1BoostBall, bool, sPrime1BoostBall)
PORT_GET_INT_IN(Prime1BoostFalloff, "MP2_PRIME1_BOOST_FALLOFF", sPrime1BoostFalloff, kPrime1BoostFalloffMin,
                kPrime1BoostFalloffMax)
PORT_SET_CLAMP(SetPrime1BoostFalloff, int, sPrime1BoostFalloff, kPrime1BoostFalloffMin,
               kPrime1BoostFalloffMax)
PORT_GET_BOOL_RETAIL(SpringBall, false, "MP2_SPRING_BALL", sSpringBall)
PORT_GET_BOOL(QuickResume, "MP2_QUICK_RESUME", sQuickResume)
PORT_SET(SetQuickResume, bool, sQuickResume)
PORT_SET(SetSpringBall, bool, sSpringBall)
PORT_GET_BOOL(SpringBallFlick, "MP2_SPRING_BALL_FLICK", sSpringFlick)
PORT_SET(SetSpringBallFlick, bool, sSpringFlick)
PORT_SET(SetLockOnToggle, bool, sLockOnToggle)
PORT_GET_BOOL_RETAIL(SpiderToggle, false, "MP2_SPIDER_TOGGLE", sSpiderToggle)
PORT_SET(SetSpiderToggle, bool, sSpiderToggle)
PORT_GET_BOOL_RETAIL(AimLockOn, false, "MP2_AIM_LOCK_ON", sAimLockOn)
PORT_SET(SetAimLockOn, bool, sAimLockOn)
PORT_GET_BOOL_RETAIL(ScanShoot, false, "MP2_SCAN_SHOOT", sScanShoot)
PORT_SET(SetScanShoot, bool, sScanShoot)
PORT_GET_BOOL_RETAIL(StickyCharge, false, "MP2_STICKY_CHARGE", sStickyCharge)
PORT_SET(SetStickyCharge, bool, sStickyCharge)
PORT_GET_BOOL_RETAIL(AutoAim, true, "MP2_AUTO_AIM", sAutoAim)
PORT_SET(SetAutoAim, bool, sAutoAim)
PORT_GET_BOOL(PointerAim, "MP2_POINTER_AIM", sPointerAim)
PORT_GET_BOOL(TrilogyRadialSelectors, "MP2_TRILOGY_RADIAL_SELECTORS", sTrilogyRadialSelectors)
PORT_SET(SetPointerAim, bool, sPointerAim)
PORT_GET_INT_IN(PointerAimBox, "MP2_POINTER_AIM_BOX", sPointerAimBox, kPointerAimBoxMin, kPointerAimBoxMax)
PORT_GET_INT_IN(PointerAimTurn, "MP2_POINTER_AIM_TURN", sPointerAimTurn, kPointerAimTurnMin, kPointerAimTurnMax)
PORT_GET_BOOL(GyroAim, "MP2_GYRO_AIM", sGyroAim)
PORT_GET_INT_IN(GyroSource, "MP2_GYRO_SOURCE", sGyroSource, 0, 2)
PORT_GET_BOOL(SteamInputBackButtons, "MP2_STEAM_INPUT_BACK_BUTTONS", sSteamInputBackButtons)
PORT_GET_BOOL(PreciseStickDeadZones, "MP2_PRECISE_STICK_DEADZONES", sPreciseStickDeadZones)
PORT_GET_INT_IN(GyroAutoCalibrate, "MP2_GYRO_AUTO_CALIBRATE", sGyroAutoCalibrate, 0, 2)
PORT_GET_FLOAT_IN(GyroSmoothing, "MP2_GYRO_SMOOTHING", sGyroSmoothing, 0.f, kGyroThresholdMax)
PORT_GET_FLOAT_IN(GyroTightening, "MP2_GYRO_TIGHTENING", sGyroTightening, 0.f, kGyroThresholdMax)
PORT_GET_FLOAT_IN(GyroCursorAngle, "MP2_GYRO_CURSOR_ANGLE", sGyroCursorAngle, kGyroCursorAngleMin, kGyroCursorAngleMax)
PORT_GET_INT_IN(GyroMode, "MP2_GYRO_MODE", sGyroMode, kGyroModeMin, kGyroModeMax)
PORT_GET_FLOAT_IN(GyroSensitivity, "MP2_GYRO_SENSITIVITY", sGyroSensitivity, kGyroSensitivityMin, kGyroSensitivityMax)
PORT_GET_BOOL(GyroCursorCamera, "MP2_GYRO_CURSOR_CAMERA", sGyroCursorCamera)
PORT_GET_BOOL(GyroSplit, "MP2_GYRO_SPLIT", sGyroSplit)
PORT_GET_FLOAT_IN(GyroSensitivityY, "MP2_GYRO_SENSITIVITY_Y", sGyroSensitivityY, kGyroSensitivityMin, kGyroSensitivityMax)
PORT_GET_BOOL(HandOnCannon, "MP2_HAND_ON_CANNON", sHandOnCannon)
PORT_GET_BOOL(LockOnFreeAim, "MP2_LOCK_ON_FREE_AIM", sLockOnFreeAim)
PORT_GET_BOOL(ResetAimBoth, "MP2_RESET_AIM_BOTH", sResetAimBoth)
PORT_GET_BOOL(MenuStandardButtons, "MP2_MENU_STANDARD_BUTTONS", sMenuStandardButtons)
PORT_SET(SetMenuStandardButtons, bool, sMenuStandardButtons)
PORT_GET_BOOL_RETAIL(RapidCharge, false, "MP2_RAPID_CHARGE", sRapidCharge)
PORT_SET(SetRapidCharge, bool, sRapidCharge)
PORT_GET_BOOL(CardBackup, "MP2_CARD_BACKUP", sCardBackup)
PORT_SET(SetCardBackup, bool, sCardBackup)
PORT_GET_BOOL(LogFile, "MP2_LOG_FILE", sLogFile)
PORT_SET(SetLogFile, bool, sLogFile)
PORT_GET_INT_IN(UiScale, "MP2_UI_SCALE", sUiScale, kUiScaleMin, kUiScaleMax)
PORT_SET_CLAMP(SetUiScale, int, sUiScale, kUiScaleMin, kUiScaleMax)
PORT_GET_BOOL(Vsync, "MP2_VSYNC", sVsync)
PORT_SET(SetVsync, bool, sVsync)
PORT_GET_BOOL(Fullscreen, "MP2_FULLSCREEN", sFullscreen)
PORT_GET_BOOL(FrameLimit, "MP2_FRAME_LIMIT", sFrameLimit)
PORT_GET_BOOL(SmoothFrames, "MP2_SMOOTH_FRAMES", sSmoothFrames)
PORT_GET_BOOL(PoseInterpolation, "MP2_POSE_INTERPOLATION", sPoseInterpolation)
PORT_GET_BOOL(ParticleInterpolation, "MP2_PARTICLE_INTERPOLATION", sParticleInterpolation)
// HudWide: written out in port_overlay.cpp (PrimedGun Echoes: off while the headset runs)
PORT_GET_BOOL(LookInterpolation, "MP2_LOOK_INTERPOLATION", sLookInterpolation)
PORT_SET_CLAMP(SetAspectMode, int, sAspectMode, kAspect_4_3, kAspect_Window)
PORT_GET_BOOL(DynamicRes, "MP2_DYNAMIC_RES", sDynamicRes)
PORT_GET_INT_IN(DynamicResTarget, "MP2_DYNAMIC_RES_TARGET", sDynamicResTarget, 0, kFpsCapMax)
PORT_GET_FLOAT_IN(DynamicResMin, "MP2_DYNAMIC_RES_MIN", sDynamicResMin, kDynamicResMinLowest, 4.f)
PORT_GET_INT_IN(UpscaleSharpenAmount, "MP2_UPSCALE_SHARPEN_AMOUNT", sSharpenAmount, 0, 100)
PORT_GET_BOOL(ShowShaderCompilation, "MP2_SHOW_SHADER_COMPILATION", sShowShaderCompilation)
PORT_GET_BOOL(ShowMenuHint, "MP2_SHOW_MENU_HINT", sShowMenuHint)
PORT_GET_BOOL(PerPixelLighting, "MP2_PER_PIXEL_LIGHTING", sPerPixelLighting)
PORT_GET_BOOL(OpenGles, "MP2_OPENGLES", sOpenGles)
PORT_SET_CLAMP(SetScanSpeed, int, sScanSpeed, kScanSpeedMin, kScanSpeedMax)
PORT_SET_CLAMP(SetBeamSwitchSpeed, int, sBeamSwitchSpeed, kBeamSwitchSpeedMin, kBeamSwitchSpeedMax)
PORT_GET_INT_IN(MusicBoost, "MP2_MUSIC_BOOST", sMusicBoost, kVolumeBoostMin, kVolumeBoostMax)
PORT_GET_INT_IN(SfxBoost, "MP2_SFX_BOOST", sSfxBoost, kVolumeBoostMin, kVolumeBoostMax)
PORT_GET_INT_IN(Language, "MP2_LANGUAGE", sLanguage, 0, kLanguageMax)
PORT_SET_CLAMP(SetLanguage, int, sLanguage, 0, kLanguageMax)
PORT_SET_CLAMP(SetComboBeamCost, int, sComboBeamCost, 0, kComboBeamCostMax)
PORT_SET_CLAMP(SetComboMissileCost, int, sComboMissileCost, 0, kComboMissileCostMax)
PORT_SET_CLAMP(SetBeamAmmoPerExpansion, int, sBeamAmmoPerExpansion, kBeamAmmoPerExpansionMin, kBeamAmmoPerExpansionMax)
PORT_GET_BOOL(IgtTimer, "MP2_IGT_TIMER", sIgtTimer)
PORT_SET(SetIgtTimer, bool, sIgtTimer)
PORT_GET_BOOL(OriginalExperience, "MP2_ORIGINAL_EXPERIENCE", sOriginalExperience)
PORT_SET(SetOriginalExperience, bool, sOriginalExperience)
PORT_SET(SetUnlockHardMode, bool, sUnlockHardMode)
PORT_SET(SetUnlockGalleries, bool, sUnlockGalleries)
PORT_SET(SetUnlockBeamDoors, bool, sUnlockBeamDoors)
PORT_SET(SetBeamDoorsStayUnlocked, bool, sBeamDoorsStayUnlocked)
PORT_SET(SetStackBeams, bool, sStackBeams)
PORT_GET_BOOL(StackBeamsSkipDark, "MP2_STACK_BEAMS_SKIP_DARK", sStackBeamsSkipDark)
PORT_SET(SetStackBeamsSkipDark, bool, sStackBeamsSkipDark)

#undef PORT_GET_BOOL
#undef PORT_GET_BOOL_RETAIL
#undef PORT_GET_INT_IN
#undef PORT_GET_FLOAT_IN
#undef PORT_SET
#undef PORT_SET_CLAMP
