// G2ME01 scaffold remaining identity and code-generation questions are recorded in research.
#include "Kyoto/Graphics/CGraphics.hpp"

#ifdef TARGET_PC
// Port: Aurora brackets a presented frame around the game's scene.
#include <aurora/aurora.h>

#include "port_harness.h"
#include "port_overlay.h"
#include "port_video.h"
// PortVr (PrimedGun Echoes): the presentation policy's frame tags, the headset's pacing, the VR menu.
#include "vr/openxr_integration.h"
#include "vr/prime_vr_policy.h"
#include "vr/vr_view.h"
#include "MetroidPrime/PortEfbCopy.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

namespace {
// GXInit (CGraphics::Startup) submits register writes the Aurora worker processes, so a frame
// must be open before it runs; the first EndScene closes it.
bool s_auroraFrameOpen = false;
bool AuroraFrameBegin() {
  // PortVr: tagged with the presentation policy's content tag (vr/prime_vr_policy.h).
  if (!s_auroraFrameOpen && aurora_begin_frame_tagged(PortVr::PrimeVRPolicyContentTag())) {
    s_auroraFrameOpen = true;
  }
  return s_auroraFrameOpen;
}
void AuroraFrameEnd() {
  if (s_auroraFrameOpen) {
    // PortVr: the frame is sealed with the policy's safety tag, so the headset only ever replays a
    // stereo packet built for this content.
    aurora_end_frame_tagged(PortVr::PrimeVRPolicyContentTag());
    s_auroraFrameOpen = false;
  }
}

// Retail's EndScene waits for the GP breakpoint and the next retrace, which holds the game to
// the video rate: each loop iteration ticks a fixed 1/60 s. Without that wait the loop ran ~8x
// real time (the 61 s health warning passed in 7 s), so sleep to a 60/50 Hz deadline instead.
// The size of one texel of a copy destination, for `PortEfbCopy::Fill`: a copy is identified by
// `dest`, its size and this, because the game copies several different formats into the same spare
// buffer in one frame (A8 palette indices for the scan visor, RGB565 for the scan window's pane).
// Formats no copy destination uses return 0, which matches no staging slot.
int CopyTexelBytes(GXTexFmt fmt) {
  switch (fmt) {
  case GX_TF_I8:
    return 1;
  case GX_TF_IA8:
  case GX_TF_RGB565:
    return 2;
  case GX_TF_RGBA8:
    return 4;
  default:
    return 0;
  }
}

// periodUs: the video rate under the 60 FPS cap, else 1/FpsCap (MP1's FPS cap slider, f1b).
void PaceFrame(long periodUs) {
  using clock = std::chrono::steady_clock;
  static clock::time_point sDeadline = clock::now();
  const clock::duration period = std::chrono::microseconds(periodUs);
  const clock::time_point now = clock::now();
  sDeadline += period;
  if (sDeadline > now) {
    std::this_thread::sleep_until(sDeadline);
  } else if (now - sDeadline > period) {
    // Too far behind (a load stall): resync rather than run fast to catch up.
    sDeadline = now;
  }
}

// The cap's pacing after a presented (or held) frame.
void PaceVideoRate() {
  // PortVr: the headset paces the loop (PortPaceFrame waits for its frame request).
  if (!PortHarness::Unpaced() && !PortVr::OpenXRIsRunning()) {
    if (PortOverlay::FrameLimit()) {
      PaceFrame(CGraphics::Is50Hz() ? 20000 : 16667);
    } else if (const int cap = PortOverlay::FpsCap(); cap > 0) {
      PaceFrame(1000000 / cap);
    }
  }
}

bool s_portHoldFrame = false;
} // namespace
#endif

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Alloc/LockedCache.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/Basics/CStopwatch.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphicsSys.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/Graphics/DolphinGPUMemory.hpp"
#include "Kyoto/Math/CRelAngle.hpp"

#include "dolphin/types.h"
#include "rstl/math.hpp"

#include "dolphin/gx.h"
#include "dolphin/vi.h"

#include <string.h>

bool CGraphicsSys::mGraphicsInitialized;
static CStopwatch sFPSTimer;
static bool sGXAborted;
static bool sUseStreamVertexDelay;
static int sSpareAllocationSize;
static void* sSpareAllocation;
bool CGraphics::sIs50Hz;
static float sFrameWaitFraction;
static float sPreviousFrameWaitFraction;
static int sTotalSize;
static int sAlloced;
static uchar* sGraphicsArena;

// clang-format off
CTevCombiners::CTevPass CGraphics::kEnvModulateConstColor(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_RasterColor,
    CTevCombiners::kCS_RegisterC0,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_RasterAlpha,
    CTevCombiners::kAS_RegisterA0,
    CTevCombiners::kAS_Zero
  )
);
CTevCombiners::CTevPass CGraphics::kEnvConstColor(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_RegisterC0
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_RegisterA0
  )
);
CTevCombiners::CTevPass CGraphics::kEnvModulate(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_RasterColor,
    CTevCombiners::kCS_TextureColor,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_RasterAlpha,
    CTevCombiners::kAS_TextureAlpha,
    CTevCombiners::kAS_Zero
  )
);
CTevCombiners::CTevPass CGraphics::kEnvDecal(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_RasterColor,
    CTevCombiners::kCS_TextureColor,
    CTevCombiners::kCS_TextureAlpha,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_RasterAlpha
  )
);
CTevCombiners::CTevPass CGraphics::kEnvBlend(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_RasterColor,
    CTevCombiners::kCS_One,
    CTevCombiners::kCS_TextureColor,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_TextureAlpha,
    CTevCombiners::kAS_RasterAlpha,
    CTevCombiners::kAS_Zero
  )
);
CTevCombiners::CTevPass CGraphics::kEnvReplace(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_TextureColor
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_TextureAlpha
  )
);
static CTevCombiners::CTevPass kEnvBlendCTandCConCF(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_RegisterC0,
    CTevCombiners::kCS_TextureColor,
    CTevCombiners::kCS_RasterColor,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_RasterAlpha
  )  
);
CTevCombiners::CTevPass CGraphics::kEnvModulateAlpha(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_RasterColor
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_TextureAlpha,
    CTevCombiners::kAS_RasterAlpha,
    CTevCombiners::kAS_Zero
  )
);
CTevCombiners::CTevPass CGraphics::kEnvModulateColor(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_TextureColor,
    CTevCombiners::kCS_RasterColor,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Konst,
    CTevCombiners::kAS_RasterAlpha,
    CTevCombiners::kAS_Zero
  )
);
CTevCombiners::CTevPass CGraphics::kEnvModulateColorByAlpha(
  CTevCombiners::ColorPass(
    CTevCombiners::kCS_Zero,
    CTevCombiners::kCS_PreviousColor,
    CTevCombiners::kCS_PreviousAlpha,
    CTevCombiners::kCS_Zero
  ),
  CTevCombiners::AlphaPass(
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_Zero,
    CTevCombiners::kAS_PreviousAlpha
  )
);
// clang-format on

CGraphics::CRenderState CGraphics::mRenderState;
VecPtr CGraphics::mVertexBuffer;
VecPtr CGraphics::mNormalBuffer;
Vec2Ptr CGraphics::mTexCoordBuffer0;
Vec2Ptr CGraphics::mTexCoordBuffer1;
uint* CGraphics::mColorBuffer;
bool CGraphics::mJustReset;
ERglCullMode CGraphics::mCullMode;
int CGraphics::mNumLightsActive;
float CGraphics::mDepthNear;
VecPtr CGraphics::mpVtxBuffer;
VecPtr CGraphics::mpNrmBuffer;
Vec2Ptr CGraphics::mpTxtBuffer0;
Vec2Ptr CGraphics::mpTxtBuffer1;
uint* CGraphics::mpClrBuffer;

// Guessed name inherited from Prime; these unused fields are not yet identified.
struct CGXLightParams {
  int x0_;
  int x4_;
  int x8_;
  int xc_;
  int x10_;

  CGXLightParams() : x0_(4), x4_(0), x8_(0), xc_(2), x10_(2) {}
};
CGXLightParams mLightParams[8];

struct {
  Vec mPosition;
  Vec mNormal;
  Vec2 mTexCoord0;
  Vec2 mTexCoord1;
  uint mColor;
  ushort mTextureUsed;
  uchar mStreamFlags;
} vtxDescr;

CVector3f CGraphics::kDefaultPositionVector(0.f, 0.f, 0.f);
CVector3f CGraphics::kDefaultDirectionVector(0.f, 1.f, 0.f);
CGraphics::CProjectionState CGraphics::mProj(true, -1.f, 1.f, 1.f, -1.f, 1.f, 100.f);
CTransform4f CGraphics::mViewMatrix = CTransform4f::Identity();
CTransform4f CGraphics::mModelMatrix = CTransform4f::Identity();
CColor CGraphics::mClearColor = CColor::Black();
bool CGraphics::mUseNormalMatrix = true;
GXLightObj CGraphics::mLightObj[8];
GXTexRegion CGraphics::mTexRegions[GX_MAX_TEXMAP];
GXTexRegion CGraphics::mTexRegionsCI[GX_MAX_TEXMAP / 2];
GXRenderModeObj CGraphics::mRenderModeObj;
Mtx CGraphics::mGXViewPointMatrix;
Mtx CGraphics::mGxModelViewInvXpose;
Mtx CGraphics::mGxModelView;
Mtx CGraphics::mCameraMtx;

int CGraphics::mNumPrimitives;
int CGraphics::mFrameCounter;
float CGraphics::mFramesPerSecond;
float CGraphics::mLastFramesPerSecond;
int CGraphics::mNumBreakpointsWaiting;
int CGraphics::mFlippingState;
bool CGraphics::mLastFrameUsedAbove;
bool CGraphics::mInterruptLastFrameUsedAbove;
uchar CGraphics::mLightActive;
uchar CGraphics::mLightsWereOn;
void* CGraphics::mpFrameBuf1;
void* CGraphics::mpFrameBuf2;
void* CGraphics::mpCurrenFrameBuf;
int CGraphics::mSpareBufferSize;
void* CGraphics::mpSpareBuffer;
int CGraphics::mSpareBufferTexCacheSize;
GXTexRegionCallback CGraphics::mGXDefaultTexRegionCallback;
void* CGraphics::mpFifo;
GXFifoObj* CGraphics::mpFifoObj;
uint CGraphics::mFifoSize = 0;
uint CGraphics::mRenderTimings;
float CGraphics::mSecondsMod900;
#ifdef TARGET_PC
static double sRenderTimePresentLag = 0.0;
static bool sRenderTimingsByTick = false;
#endif
CTimeProvider* CGraphics::mpExternalTimeProvider;
int CGraphics::mScreenStretch;
int CGraphics::mScreenPositionX;
int CGraphics::mScreenPositionY;

CViewport CGraphics::mViewport = {0, 0, 640, 480, 320.f, 240.f};
ELightType CGraphics::mLightTypes[8] = {
    kLT_Directional, kLT_Directional, kLT_Directional, kLT_Directional,
    kLT_Directional, kLT_Directional, kLT_Directional, kLT_Directional,
};

const CTevCombiners::CTevPass& CGraphics::kEnvPassthru = CTevCombiners::kEnvPassthru;
bool CGraphics::mIsBeginSceneClearFb = true;
ERglEnum CGraphics::mDepthFunc = kE_LEqual;
ERglPrimitive CGraphics::mCurrentPrimitive = kP_Points;
float CGraphics::mDepthFar = 1.f;
u32 CGraphics::mClearDepthValue = GX_MAX_Z24;
float CGraphics::mPixelAspectRatio = 1.f;
bool CGraphics::mIsGXModelMatrixIdentity = true;
bool CGraphics::mFirstFrame = true;
bool CGraphics::mUseVideoFilter = true;
float CGraphics::mBrightness = 1.f;

const GXTexMapID CGraphics::kSpareBufferTexMapID = GX_TEXMAP7;

void CGraphics::InitGraphicsFifo(GXFifoObj* obj, void* fifo, uint fifoSize) {
  GXFifoObj fifoObj;
  GXInitFifoBase(&fifoObj, fifo, fifoSize);
  GXSetCPUFifo(&fifoObj);
  GXSetGPFifo(&fifoObj);
  GXInitFifoLimits(obj, fifoSize - 0x4000, fifoSize - 0x10000);
  GXSetCPUFifo(obj);
  GXSetGPFifo(obj);
}

// Guessed names for the graphics arena helpers.
static void InitializeGraphicsArena(void* base, int size) {
  sGraphicsArena = static_cast< uchar* >(base);
  sTotalSize = size;
  sAlloced = 0;
}

static void* AllocateGraphicsArena(int size) {
  void* result = sGraphicsArena + sAlloced;
  sAlloced += size;
  return result;
}

static void ResetGraphicsArena() { sAlloced = 0; }

bool CGraphics::Startup(const COsContext& osContext, bool progressive) {
#ifdef TARGET_PC
  AuroraFrameBegin();
#endif
  InitializeGraphicsArena(osContext.GetArenaBlock(), osContext.GetArenaBlockSize());
  VIInit();
  VISetBlack(TRUE);
  ConfigureVideo(true, progressive);
  CGX::ResetGXStates();
  InitGraphicsVariables();
  ConfigureFrameBuffer();
  for (int i = 0; i < ARRAY_SIZE(mTexRegions); ++i) {
    GXInitTexCacheRegion(&mTexRegions[i], false, 0x8000 * i, GX_TEXCACHE_32K, 0x80000 + 0x8000 * i,
                         GX_TEXCACHE_32K);
  }
  for (int i = 0; i < ARRAY_SIZE(mTexRegionsCI); ++i) {
    GXInitTexCacheRegion(&mTexRegionsCI[i], false, (8 + 2 * i) << 15, GX_TEXCACHE_32K,
                         (9 + 2 * i) << 15, GX_TEXCACHE_32K);
  }
  mGXDefaultTexRegionCallback = GXSetTexRegionCallback(TexRegionCallback);
  mSpareBufferSize = sSpareAllocationSize;
  mpSpareBuffer = sSpareAllocation;
  mSpareBufferTexCacheSize = 0x10000;
  return true;
}

// Guessed name.
static void SetProgressiveFilter(GXRenderModeObj& mode) {
  const u8 filter[7] = {4, 4, 16, 16, 16, 4, 4};
  memcpy(mode.vfilter, filter, sizeof(filter));
}

void CGraphics::ConfigureVideo(bool initial, uchar progressive) {
  if (!initial) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  GXRenderModeObj* mode = nullptr;
  switch (VIGetTvFormat()) {
  case VI_MPAL:
    if (!progressive) {
      mode = &GXMpal480IntDf;
    } else {
      mode = &GXNtsc480Prog;
    }
    break;
  case VI_NTSC:
    if (!progressive) {
      mode = &GXNtsc480IntDf;
    } else {
      mode = &GXNtsc480Prog;
    }
    break;
  case VI_PAL:
  case VI_EURGB60:
    rs_debugger_printf("PAL TV Format not compatible with NTSC build");
    break;
  }
  GXAdjustForOverscan(mode, &mRenderModeObj, 0, 16);
  mPixelAspectRatio = 1.f;
  sIs50Hz = false;
  mRenderModeObj.viWidth += 20;
  mRenderModeObj.viXOrigin -= 10;
  if (progressive) {
    SetProgressiveFilter(mRenderModeObj);
  }
  mpFrameBuf1 = nullptr;
  mpFrameBuf2 = nullptr;
  mpFifo = nullptr;
  sSpareAllocation = nullptr;
  GPUMemory::SetBuffer(nullptr, 0);
  ResetGraphicsArena();
  const int frameBufferSize =
      ((mRenderModeObj.fbWidth + 15) & 0xfff0) * mRenderModeObj.xfbHeight * 2;
  mpFrameBuf1 = AllocateGraphicsArena(frameBufferSize);
  mpFrameBuf2 = AllocateGraphicsArena(frameBufferSize);
  sSpareAllocationSize = 0x46000;
  mFifoSize = 0x60000;
  mpFifo = AllocateGraphicsArena(mFifoSize);
  sSpareAllocation = AllocateGraphicsArena(sSpareAllocationSize);
#ifdef TARGET_PC
  // The console's heap was zeroed and the host's is not. The spare buffer is where EFB copies land
  // (the scan visor's C8 overlay, the camera filters) and nothing else writes it; those copies are
  // not implemented in the port yet, so on the host it held whatever the allocator left there. The
  // visor binds it as a C8 texture and samples those bytes as palette indices through a 248-entry
  // TLUT, and an index past the end of the palette is a GPUVM fault - the device loss that killed
  // every scan-visor run (VK_ERROR_DEVICE_LOST, SQC (data)). Zeros are both what the console had
  // and a valid palette index.
  memset(sSpareAllocation, 0, sSpareAllocationSize);
#endif
  GPUMemory::SetBuffer(AllocateGraphicsArena(0x40000), 0x40000);
  mSpareBufferSize = sSpareAllocationSize;
  mpSpareBuffer = sSpareAllocation;
  if (!initial) {
    mRenderModeObj.viWidth += mScreenStretch * 2;
    mRenderModeObj.viXOrigin += mScreenPositionX - mScreenStretch;
    mRenderModeObj.viYOrigin += mScreenPositionY;
  }
  if (!initial) {
    VIWaitForRetrace();
    VIWaitForRetrace();
  }
  VIConfigure(&mRenderModeObj);
  VIFlush();
  if (initial) {
    mpFifoObj = GXInit(mpFifo, mFifoSize);
  }
  InitGraphicsFifo(mpFifoObj, mpFifo, mFifoSize);
  GXSetCopyFilter(mRenderModeObj.aa, mRenderModeObj.sample_pattern, GX_TRUE,
                  mRenderModeObj.vfilter);
}

#ifdef TARGET_PC
// Port: the Prime 1 port's PortResizeFrameBuffer. Aurora sizes the EFB it presents from the render
// mode, so a new fbWidth widens the picture; the viewport and scissor follow it.
void CGraphics::PortResizeFrameBuffer(u16 fbWidth) {
  if (fbWidth == 0 || fbWidth == mRenderModeObj.fbWidth) {
    return;
  }
  mRenderModeObj.fbWidth = fbWidth;
  SetViewport(0, 0, fbWidth, mRenderModeObj.efbHeight);
  SetScissor(0, 0, fbWidth, mRenderModeObj.efbHeight);
  VIConfigure(&mRenderModeObj);
}

#endif
GXTexRegion* CGraphics::TexRegionCallback(const GXTexObj* obj, GXTexMapID id) {
  static signed char nextTexRgn = 0;
  static signed char nextTexRgnCI = 0;
#ifdef TARGET_PC
  // Port: Aurora's GXGetTexObjFmt takes a non-const pointer.
  const GXTexFmt fmt = GXGetTexObjFmt(const_cast< GXTexObj* >(obj));
#else
  const GXTexFmt fmt = GXGetTexObjFmt(obj);
#endif
  if (id == GX_TEXMAP7) {
    if (fmt != GX_TF_C4 && fmt != GX_TF_C8 && fmt != GX_TF_C14X2) {
      return &mTexRegions[0];
    }
    return &mTexRegionsCI[0];
  }
  if (fmt != GX_TF_C4 && fmt != GX_TF_C8 && fmt != GX_TF_C14X2) {
    do {
      nextTexRgn = (nextTexRgn + 1) & 7;
    } while (nextTexRgn == 0);
    return &mTexRegions[nextTexRgn];
  }
  do {
    nextTexRgnCI = (nextTexRgnCI + 1) & 3;
  } while (nextTexRgnCI == 0);
  return &mTexRegionsCI[nextTexRgnCI];
}

void CGraphics::InitGraphicsVariables() {
  for (int i = 0; i < ARRAY_SIZE(mLightTypes); ++i) {
    mLightTypes[i] = kLT_Directional;
  }
  mLightActive = 0;
  SetDepthWriteMode(false, mDepthFunc, false);
  SetCullMode(kCM_None);
  SetAmbientColor(CColor(0.2f, 0.2f, 0.2f, 1.f));
  mIsGXModelMatrixIdentity = false;
  SetIdentityViewPointMatrix();
  SetIdentityModelMatrix();
  SetViewport(0, 0, mViewport.mWidth, mViewport.mHeight);
  SetPerspective(60.f,
                 static_cast< float >(mViewport.mWidth) / static_cast< float >(mViewport.mHeight),
                 mProj.GetNear(), mProj.GetFar());
  SetCopyClear(mClearColor, 1.f);
  const GXColor white = {0xFF, 0xFF, 0xFF, 0xFF};
  CGX::SetChanMatColor(CGX::Channel0, white);
  mRenderState.ResetFlushAll();
}

void CGraphics::Shutdown() {
  GXSetTexRegionCallback(mGXDefaultTexRegionCallback);
  CFrameDelayedKiller::StallAndFlushAllAllocations();
  mpFrameBuf1 = nullptr;
  mpFrameBuf2 = nullptr;
  mpFifo = nullptr;
  sSpareAllocation = nullptr;
  GPUMemory::SetBuffer(nullptr, 0);
  ResetGraphicsArena();
}

void CGraphics::InitGraphicsDefaults() {
  SetDepthRange(0.f, 1.f);
  mIsGXModelMatrixIdentity = false;
  SetModelMatrix(mModelMatrix);
  SetViewPointMatrix(mViewMatrix);
  SetDepthWriteMode(false, mDepthFunc, false);
  SetCullMode(mCullMode);
  SetViewport(mViewport.mLeft, mViewport.mTop, mViewport.mWidth, mViewport.mHeight);
  FlushProjection();
  CTevCombiners::Init();
  DisableAllLights();
  SetDefaultVtxAttrFmt();
}

void CGraphics::ConfigureFrameBuffer() {
  VIConfigure(&mRenderModeObj);
  VISetNextFrameBuffer(mpFrameBuf1);
  mpCurrenFrameBuf = mpFrameBuf2;
  GXSetViewport(0.f, 0.f, static_cast< float >(mRenderModeObj.fbWidth),
                static_cast< float >(mRenderModeObj.efbHeight), 0.f, 1.f);
  GXSetScissor(0, 0, mRenderModeObj.fbWidth, mRenderModeObj.efbHeight);
  GXSetDispCopySrc(0, 0, mRenderModeObj.fbWidth, mRenderModeObj.efbHeight);
  GXSetDispCopyDst(mRenderModeObj.fbWidth, mRenderModeObj.efbHeight);
  GXSetDispCopyYScale(static_cast< float >(mRenderModeObj.xfbHeight) / mRenderModeObj.efbHeight);
  GXSetCopyFilter(mRenderModeObj.aa, mRenderModeObj.sample_pattern, GX_ENABLE,
                  mRenderModeObj.vfilter);
  if (mRenderModeObj.aa) {
    GXSetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
  } else {
    GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  }
  GXSetDispCopyGamma(GX_GM_1_0);
  GXCopyDisp(mpCurrenFrameBuf, true);
  VIFlush();
  VIWaitForRetrace();
  VIWaitForRetrace();
  mViewport.mWidth = mRenderModeObj.fbWidth;
  mViewport.mHeight = mRenderModeObj.efbHeight;
  InitGraphicsDefaults();
}

void CGraphics::EnableLight(ERglLight light) {
  CGX::SetNumChans(1);
  int lightsWereOn = mLightActive;
  GXLightID lightId = static_cast< GXLightID >(1 << light);
  if ((lightsWereOn & lightId) == GX_LIGHT_NULL) {
    mLightActive |= lightId;
    CGX::SetChanCtrl(CGX::Channel0, true, GX_SRC_REG, GX_SRC_REG,
                     static_cast< GXLightID >((lightsWereOn | lightId) & (GX_MAX_LIGHT - 1)),
                     GX_DF_CLAMP, GX_AF_SPOT);
    ++mNumLightsActive;
  }
  mLightsWereOn = mLightActive;
}

static inline GXLightID get_hw_light_index(ERglLight light) {
  return static_cast< GXLightID >(1u << light);
}

void CGraphics::LoadLight(ERglLight light, const CLight& info) {
  GXLightID lightId = get_hw_light_index(light);
  CVector3f pos = info.GetPosition();
  CVector3f dir = info.GetDirection();

  switch (info.GetType()) {
  case kLT_Spot: {
    MTXMultVec(mCameraMtx, reinterpret_cast< VecPtr >(&pos), reinterpret_cast< VecPtr >(&pos));
    GXLightObj* obj = &mLightObj[light];
    GXInitLightPos(obj, pos.GetX(), pos.GetY(), pos.GetZ());
    MTXMultVecSR(mCameraMtx, reinterpret_cast< VecPtr >(&dir), reinterpret_cast< VecPtr >(&dir));
    GXInitLightDir(obj, dir.GetX(), dir.GetY(), dir.GetZ());
    GXInitLightAttn(obj, 1.f, 0.f, 0.f, info.GetAttenuationConstant(), info.GetAttenuationLinear(),
                    info.GetAttenuationQuadratic());
    GXInitLightSpot(obj, info.GetSpotCutoff(), GX_SP_COS2);
    break;
  }
  case kLT_Point:
  case kLT_LocalAmbient: {
    MTXMultVec(mCameraMtx, reinterpret_cast< VecPtr >(&pos), reinterpret_cast< VecPtr >(&pos));
    GXInitLightPos(&mLightObj[light], pos.GetX(), pos.GetY(), pos.GetZ());
    GXInitLightAttn(&mLightObj[light], 1.f, 0.f, 0.f, info.GetAttenuationConstant(),
                    info.GetAttenuationLinear(), info.GetAttenuationQuadratic());
    break;
  }
  case kLT_Directional: {
    MTXMultVecSR(mCameraMtx, reinterpret_cast< VecPtr >(&dir), reinterpret_cast< VecPtr >(&dir));
    dir = -dir;
    GXInitLightPos(&mLightObj[light], dir.GetX() * 1048576.f, dir.GetY() * 1048576.f,
                   dir.GetZ() * 1048576.f);
    GXInitLightAttn(&mLightObj[light], 1.f, 0.f, 0.f, 1.f, 0.f, 0.f);
    break;
  }
  case kLT_Custom:
  case kLT_Hard: {
    MTXMultVec(mCameraMtx, reinterpret_cast< VecPtr >(&pos), reinterpret_cast< VecPtr >(&pos));
    GXLightObj* obj = &mLightObj[light];
    GXInitLightPos(obj, pos.GetX(), pos.GetY(), pos.GetZ());
    MTXMultVecSR(mCameraMtx, reinterpret_cast< VecPtr >(&dir), reinterpret_cast< VecPtr >(&dir));
    GXInitLightDir(obj, dir.GetX(), dir.GetY(), dir.GetZ());
    GXInitLightAttn(obj, info.GetAngleAttenuationConstant(), info.GetAngleAttenuationLinear(),
                    info.GetAngleAttenuationQuadratic(), info.GetAttenuationConstant(),
                    info.GetAttenuationLinear(), info.GetAttenuationQuadratic());
    break;
  }
  default:
    break;
  }

  GXInitLightColor(&mLightObj[light], info.GetColor().GetGXColor());
  GXLoadLightObjImm(&mLightObj[light], lightId);
  mLightTypes[light] = info.GetType();
}

void CGraphics::DisableAllLights() {
  mNumLightsActive = 0;
  mLightActive = 0;
  CGX::SetChanCtrl(CGX::Channel0, false, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                   GX_AF_NONE);
  CGX::SetChanCtrl(CGX::Channel1, false, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                   GX_AF_NONE);
}

// https://en.wikipedia.org/wiki/Hamming_weight
static inline uint popcount8(uint b) {
  b = (b & 0x55) + ((b & 0xAA) >> 1);
  b = (b & 0x33) + ((b & 0xCC) >> 2);
  return (static_cast< uchar >(b) & 0xF) + (static_cast< uchar >(b) >> 4);
}

void CGraphics::SetLightState(uchar lights) {
  GXAttnFn attnFn = GX_AF_NONE;
  if (lights != 0) {
    attnFn = GX_AF_SPOT;
  }
  GXDiffuseFn diffFn = GX_DF_NONE;
  if (lights != 0) {
    diffFn = GX_DF_CLAMP;
  }
  CGX::SetChanCtrl(CGX::Channel0, lights != 0 ? GX_ENABLE : GX_DISABLE, GX_SRC_REG,
                   (vtxDescr.mStreamFlags & 2) != 0 ? GX_SRC_VTX : GX_SRC_REG,
                   static_cast< GXLightID >(lights), diffFn, attnFn);
  mLightActive = lights;
  mNumLightsActive = popcount8(lights);
}

void CGraphics::SetViewMatrix() {
  if (mIsGXModelMatrixIdentity) {
    MTXCopy(mCameraMtx, mGxModelView);
  } else {
    MTXConcat(mCameraMtx, mModelMatrix.GetCStyleMatrix(), mGxModelView);
  }
  GXLoadPosMtxImm(mGxModelView, GX_PNMTX0);
  if (mUseNormalMatrix) {
    MTXInvXpose(mGxModelView, mGxModelViewInvXpose);
    GXLoadNrmMtxImm(mGxModelViewInvXpose, GX_PNMTX0);
  }
}

void CGraphics::SetViewPointMatrix(const CTransform4f& xf) {
  mViewMatrix = xf;
  mGXViewPointMatrix[0][0] = xf.Get00();
  mGXViewPointMatrix[0][1] = xf.Get10();
  mGXViewPointMatrix[0][2] = xf.Get20();
  mGXViewPointMatrix[0][3] = 0.f;
  mGXViewPointMatrix[1][0] = xf.Get02();
  mGXViewPointMatrix[1][1] = xf.Get12();
  mGXViewPointMatrix[1][2] = xf.Get22();
  mGXViewPointMatrix[1][3] = 0.f;
  mGXViewPointMatrix[2][0] = -xf.Get01();
  mGXViewPointMatrix[2][1] = -xf.Get11();
  mGXViewPointMatrix[2][2] = -xf.Get21();
  mGXViewPointMatrix[2][3] = 0.f;
  Mtx translation;
  const CVector3f viewPoint = xf.GetTranslation();
  MTXTrans(translation, -viewPoint.GetX(), -viewPoint.GetY(), -viewPoint.GetZ());
  MTXConcat(mGXViewPointMatrix, translation, mCameraMtx);
  SetViewMatrix();
}

void CGraphics::SetIdentityViewPointMatrix() { SetViewPointMatrix(CTransform4f::Identity()); }

void CGraphics::SetModelMatrix(const CTransform4f& xf) {
  if (&xf == &CTransform4f::Identity()) {
    if (mIsGXModelMatrixIdentity) {
      return;
    }
    mIsGXModelMatrixIdentity = true;
  } else {
    mIsGXModelMatrixIdentity = false;
  }
  mModelMatrix = xf;
  SetViewMatrix();
}

void CGraphics::SetIdentityModelMatrix() {
  if (!mIsGXModelMatrixIdentity) {
    mModelMatrix = CTransform4f::Identity();
    mIsGXModelMatrixIdentity = true;
    SetViewMatrix();
  }
}

void CGraphics::SetOrtho(float left, float right, float top, float bottom, float znear,
                         float zfar) {
  mProj = CProjectionState(false, left, right, top, bottom, znear, zfar);
  FlushProjection();
}

void CGraphics::SetPerspective(float fovy, float aspect, float znear, float zfar) {
  float t = tan(CRelAngle::FromDegrees(fovy).AsRadians() / 2.f);
  mProj = CProjectionState(true,                               // Is Projection
                           -(aspect * 2.f * znear * t * 0.5f), // Left
                           (aspect * 2.f * znear * t * 0.5f),  // Right
                           (znear * 2.f * t * 0.5f),           // Top
                           -(znear * 2.f * t * 0.5f),          // Bottom
                           znear, zfar);
  FlushProjection();
}

CMatrix4f CGraphics::GetPerspectiveProjectionMatrix() {
  // construct in place
  return CMatrix4f(
      // clang-format off
    (mProj.GetNear() * 2.f) / (mProj.GetRight() - mProj.GetLeft()),
    -(mProj.GetRight() + mProj.GetLeft()) / (mProj.GetRight() - mProj.GetLeft()),
    0.f,
    0.f,
    0.f,
    -(mProj.GetTop() + mProj.GetBottom()) / (mProj.GetTop() - mProj.GetBottom()),
    (mProj.GetNear() * 2.f) / (mProj.GetTop() - mProj.GetBottom()),
    0.f,
    0.f,
    (mProj.GetFar() + mProj.GetNear()) / (mProj.GetFar() - mProj.GetNear()),
    0.f,
    -(mProj.GetFar() * 2.f * mProj.GetNear()) / (mProj.GetFar() - mProj.GetNear()),
    0.f,
    1.f,
    0.f,
    0.f
      // clang-format on
  );
}

CMatrix4f CGraphics::CalculatePerspectiveMatrix(float fovy, float aspect, float znear, float zfar) {
  float t = tan(CRelAngle::FromDegrees(fovy).AsRadians() / 2.f);
  float depth = zfar - znear;
  float twoNear = 2.f * znear;
  float right = aspect * 2.f * znear * t * 0.5f;
  float top = twoNear * t * 0.5f;
  float left = -right;
  float bottom = -top;
  // construct in place
  return CMatrix4f(
      // clang-format off
    twoNear / (right - left),
    -(right + left) / (right - left),
    0.f,
    0.f,
    0.f,
    -(top + bottom) / (top - bottom),
    twoNear / (top - bottom),
    0.f,
    0.f,
    (zfar + znear) / depth,
    0.f,
    -(2.f * zfar * znear) / depth,
    0.f,
    1.f,
    0.f,
    0.f
      // clang-format on
  );
}

void CGraphics::SetViewport(int left, int bottom, int width, int height) {
  mViewport.mLeft = left;
  mViewport.mTop = mRenderModeObj.efbHeight - (bottom + height);
  mViewport.mWidth = width;
  mViewport.mHeight = height;
  mViewport.mHalfWidth = static_cast< float >(width / 2);
  mViewport.mHalfHeight = static_cast< float >(height / 2);
  GXSetViewport(static_cast< float >(mViewport.mLeft), static_cast< float >(mViewport.mTop),
                static_cast< float >(mViewport.mWidth), static_cast< float >(mViewport.mHeight),
                mDepthNear, mDepthFar);
}

int CGraphics::GetViewportTop(int bottom) {
  return mRenderModeObj.efbHeight - (bottom + mViewport.mHeight);
}

void CGraphics::SetScissor(int left, int bottom, int width, int height) {
  GXSetScissor(left, mRenderModeObj.efbHeight - (bottom + height), width, height);
}

void CGraphics::SetAmbientColor(const CColor& color) {
  CGX::SetChanAmbColor(CGX::Channel0, color.GetGXColor());
  CGX::SetChanAmbColor(CGX::Channel1, color.GetGXColor());
}

void CGraphics::SetCopyClear(const CColor& color, float depth) {
  mClearColor = color;
  mClearDepthValue = static_cast< u32 >(depth * GX_MAX_Z24);
  GXSetCopyClear(color.GetGXColor(), mClearDepthValue);
}

void CGraphics::SetClearColor(const CColor& color) {
  mClearColor = color;
  GXSetCopyClear(mClearColor.GetGXColor(), mClearDepthValue);
}

CColor CGraphics::GetClearColor() { return mClearColor; }

void CGraphics::ClearBackAndDepthBuffers() {
  GXInvalidateTexAll();
  if (mRenderModeObj.field_rendering) {
    GXSetViewportJitter(0.f, 0.f, static_cast< float >(mRenderModeObj.fbWidth),
                        static_cast< float >(mRenderModeObj.xfbHeight), 0.f, 1.f, VIGetNextField());
  } else {
    GXSetViewport(0.f, 0.f, static_cast< float >(mRenderModeObj.fbWidth),
                  static_cast< float >(mRenderModeObj.xfbHeight), 0.f, 1.f);
  }
  GXInvalidateVtxCache();
}

static const GXVtxDescList skPosColorTexDirect[] = {
    {GX_VA_POS, GX_DIRECT},
    {GX_VA_CLR0, GX_DIRECT},
    {GX_VA_TEX0, GX_DIRECT},
    {GX_VA_NULL, GX_NONE},
};

#ifdef TARGET_PC
void CGraphics::BeginScene() {
  // Port: a scene maps to one Aurora frame (covers init-time draws too).
  // A held frame opens none, so the window keeps showing the last one (PortSetHoldFrame).
  if ((s_portHoldFrame && !s_auroraFrameOpen) || !AuroraFrameBegin()) {
    return;
  }
  // F1 > Video > Compatibility's GPU self-test, between frames' GX work (PortVideo, f1b).
  PortVideo::RunGpuSelfTestIfRequested();
  ClearBackAndDepthBuffers();
}
#else
void CGraphics::BeginScene() { ClearBackAndDepthBuffers(); }
#endif

#ifdef TARGET_PC
bool CGraphics::PortSceneOpen() { return s_auroraFrameOpen; }
void CGraphics::PortSetHoldFrame(bool hold) { s_portHoldFrame = hold; }
bool CGraphics::PortFrameHeld() { return s_portHoldFrame; }
// A held frame presents nothing, so no vsync waits on it: uncapped, it would spin. Pace it at the
// video rate.
void CGraphics::PortPaceHeldFrame() {
  if (!PortHarness::Unpaced() && !PortOverlay::FrameLimit() && PortOverlay::FpsCap() <= 0) {
    PaceFrame(CGraphics::Is50Hz() ? 20000 : 16667);
  } else {
    PaceVideoRate();
  }
}
#endif

void CGraphics::SwapBuffers() {
  GXDisableBreakPt();
  mFlippingState = 1;
}

void CGraphics::VideoPreCallback(u32 retraceCount) {
  if (mNumBreakpointsWaiting != 0 && mFlippingState == 1) {
    if (mFirstFrame) {
      VISetBlack(FALSE);
      mFirstFrame = false;
    }
    VISetNextFrameBuffer(mpCurrenFrameBuf);
    VIFlush();
    mpCurrenFrameBuf = mpCurrenFrameBuf == mpFrameBuf1 ? mpFrameBuf2 : mpFrameBuf1;
    mFlippingState = 2;
  }

#ifndef TARGET_PC
  // Port: no GP FIFO to stall on; Aurora consumes commands itself.
  static int stalledRetraces = 0;
  static u32 lastOverflowCount = 0;
  GXBool overHighWaterMark;
  GXBool ignored;
  GXGetGPStatus(&overHighWaterMark, &ignored, &ignored, &ignored, &ignored);
  const u32 overflowCount = GXGetOverflowCount();
  if (overHighWaterMark && lastOverflowCount == overflowCount) {
    if (++stalledRetraces >= 10) {
      GXAbortFrame();
      OSResumeThread(GXGetCurrentGXThread());
      GXFifoObj fifo;
      GXInitFifoBase(&fifo, mpFifo, mFifoSize);
      GXSetCPUFifo(&fifo);
      GXSetGPFifo(&fifo);
      GXSetCPUFifo(mpFifoObj);
      GXSetGPFifo(mpFifoObj);
      sGXAborted = true;
      stalledRetraces = 0;
    }
  } else {
    stalledRetraces = 0;
    lastOverflowCount = overflowCount;
  }
#endif
}

void CGraphics::VideoPostCallback(u32 retraceCount) {
  if (mNumBreakpointsWaiting != 0) {
    if (mFlippingState == 2) {
      --mNumBreakpointsWaiting;
      mFlippingState = 0;
      CStopwatch& timer = sFPSTimer;
      float elapsed = timer.GetElapsedTime();
      mLastFramesPerSecond = mFramesPerSecond;
      mFramesPerSecond = 1.f / elapsed;
      timer.Reset();
      mInterruptLastFrameUsedAbove = VIGetNextField() == 1;
    }
  }
}

void CGraphics::EndScene() {
#ifdef TARGET_PC
  if (!s_auroraFrameOpen) {
    return;
  }
#endif
  volatile int& numBreakPt = const_cast< volatile int& >(mNumBreakpointsWaiting);
  volatile bool& aborted = const_cast< volatile bool& >(sGXAborted);
  const OSTime start = OSGetTime();
#ifdef TARGET_PC
  // Port: there is no GP breakpoint/retrace handshake; the frame is presented below.
  (void)start;
  mNumBreakpointsWaiting = 0;
  sGXAborted = false;
  aborted = false;
  if (false) {
#else
  if (sGXAborted) {
#endif
    GXAbortFrame();
  } else {
    CStopwatch waitTimer;
    while (numBreakPt > 0 && !aborted) {
      OSYieldThread();
      if (OSTicksToMilliseconds(OSGetTime() - start) > 250) {
        const bool interrupts = OSDisableInterrupts();
        GXAbortFrame();
        aborted = true;
        OSRestoreInterrupts(interrupts);
      }
    }
    const float elapsedMs = static_cast< float >(waitTimer.GetElapsedMicros() / 1000);
    const float framePeriod = sIs50Hz ? 20.f : 16.6666667f;
    sPreviousFrameWaitFraction = sFrameWaitFraction;
    sFrameWaitFraction = (framePeriod - elapsedMs) / framePeriod;
  }
  if (aborted) {
    mNumBreakpointsWaiting = 0;
    aborted = false;
    mpFifoObj = GXInit(mpFifo, mFifoSize);
    InitGraphicsFifo(mpFifoObj, mpFifo, mFifoSize);
    SetDefaultVtxAttrFmt();
    CGX::ResetGXStatesFull();
    InitGraphicsVariables();
  }
  ++mNumBreakpointsWaiting;

  Mtx44 projection;
  MTXOrtho(projection, mViewport.mHeight / 2, -mViewport.mHeight / 2, -mViewport.mWidth / 2,
           mViewport.mWidth / 2, -1.f, -10.f);
  GXSetProjection(projection, GX_ORTHOGRAPHIC);
  Mtx model;
  MTXIdentity(model);
  GXLoadPosMtxImm(model, GX_PNMTX0);
  CGX::SetZMode(true, GX_LEQUAL, false);
  CGX::SetVtxDescv(skPosColorTexDirect);
#ifdef TARGET_PC
  // Port: the 96 one-pixel triangles flushed the hardware's pipeline before the copy. They sit at
  // the middle of the screen, in whatever TEV state the frame's last draw left (the minimap's Samus
  // marker colours them), and at the TV's resolution they cover no pixel centre: at any higher one
  // they show as the "stray pixel" in the middle of the picture, one per eye in the headset.
  // Nothing on the PC needs the flush.
#else
  CGX::Begin(GX_TRIANGLES, GX_VTXFMT0, 288);
  for (int i = 0; i < 96; ++i) {
    GXPosition3f32(0.f, 1.f, 1.f);
    GXColor1u32(0);
    GXTexCoord2f32(0.f, 0.f);
    GXPosition3f32(0.f, 0.f, 1.f);
    GXColor1u32(0);
    GXTexCoord2f32(0.f, 0.f);
    GXPosition3f32(1.f, 0.f, 1.f);
    GXColor1u32(0);
    GXTexCoord2f32(0.f, 0.f);
  }
  CGX::End();
#endif
  CGX::SetZMode(true, GX_LEQUAL, true);

  const float brightness = CMath::Clamp(0.f, mBrightness, 2.f);
  static const u8 unfiltered[7] = {0, 0, 21, 22, 21, 0, 0};
  const u8* filter = mUseVideoFilter ? mRenderModeObj.vfilter : unfiltered;
  u8 adjustedFilter[7];
  for (int i = 0; i < 7; ++i) {
    adjustedFilter[i] = static_cast< u8 >(brightness * filter[i]);
  }
  GXSetCopyFilter(mRenderModeObj.aa, mRenderModeObj.sample_pattern, GX_TRUE, adjustedFilter);
  GXCopyDisp(mpCurrenFrameBuf, mIsBeginSceneClearFb);
  GXSetCopyFilter(mRenderModeObj.aa, mRenderModeObj.sample_pattern, mUseVideoFilter,
                  mRenderModeObj.vfilter);
  GXSetBreakPtCallback(SwapBuffers);
  VISetPreRetraceCallback(VideoPreCallback);
  VISetPostRetraceCallback(VideoPostCallback);
  GXFlush();
#ifdef TARGET_PC
  // PortVr: PrimedGun's VR menu, while it is open in the headset.
  PortVr::VrMenuUpdate();
  // Port: the debug overlay draws into this frame before it is presented.
  if (s_auroraFrameOpen) {
    PortOverlay::Draw();
  }
  // Port: present this Aurora frame instead of waiting on the GP breakpoint.
  AuroraFrameEnd();
  // A fast run or a harness boot's load needs no video-rate wait (PortHarness::Unpaced).
  // With the cap off, the FPS cap slider's rate, or none (0 = unlimited).
  PaceVideoRate();
  mNumBreakpointsWaiting = 0;
  mFlippingState = 0;
#else
  void* readPtr;
  void* writePtr;
  GXGetFifoPtrs(GXGetGPFifo(), &readPtr, &writePtr);
  GXEnableBreakPt(writePtr);
#endif
  mLastFrameUsedAbove = mInterruptLastFrameUsedAbove;
  ++mFrameCounter;
  CFrameDelayedKiller::FlushAllocationsForFrame();
}

void CGraphics::SetDepthWriteMode(const bool test, ERglEnum comp, const bool write) {
  mDepthFunc = comp;
  CGX::SetZMode(test, static_cast< GXCompare >(comp), write);
}

void CGraphics::SetCullMode(ERglCullMode cullMode) {
  mCullMode = cullMode;
  GXSetCullMode(static_cast< GXCullMode >(cullMode));
}

void CGraphics::SetBlendMode(ERglBlendMode mode, ERglBlendFactor src, ERglBlendFactor dst,
                             ERglLogicOp op) {
  CGX::SetBlendMode(static_cast< GXBlendMode >(mode), static_cast< GXBlendFactor >(src),
                    static_cast< GXBlendFactor >(dst), static_cast< GXLogicOp >(op));
}

void CGraphics::SetAlphaCompare(ERglAlphaFunc comp0, uchar ref0, ERglAlphaOp op,
                                ERglAlphaFunc comp1, uchar ref1) {
  CGX::SetAlphaCompare(static_cast< GXCompare >(comp0), static_cast< uchar >(ref0),
                       static_cast< GXAlphaOp >(op), static_cast< GXCompare >(comp1),
                       static_cast< uchar >(ref1));
}

;

void CGraphics::Render2D(const CTexture& tex, int x, int y, int w, int h, const CColor& col) {
  Render2D(&tex, x, y, w, h, col);
}

void CGraphics::Render2D(const CTexture* tex, int x, int y, int w, int h, const CColor& col) {
  Mtx44 proj;
  MTXOrtho(proj, mViewport.mHeight / 2, -mViewport.mHeight / 2, -mViewport.mWidth / 2,
           mViewport.mWidth / 2, -1.f, -10.f);
  GXSetProjection(proj, GX_ORTHOGRAPHIC);
#ifdef TARGET_PC
  uint c = col.GetColor_GX();
#else
  uint c = col.GetColor_u32();
#endif

  Mtx mtx;
  MTXIdentity(mtx);
  GXLoadPosMtxImm(mtx, GX_PNMTX0);

  float x2, y2, x1, y1;
  x1 = x - mViewport.mWidth / 2;
  y1 = y - mViewport.mHeight / 2;
  x2 = x1 + w;
  y2 = y1 + h;

  // Save state + setup
  CGX::SetVtxDescv(skPosColorTexDirect);
  mLightsWereOn = mLightActive;
  if (mLightActive != 0) {
    DisableAllLights();
  }
  ERglCullMode cullMode = mCullMode;
  SetCullMode(kCM_None);
  if (tex != nullptr) {
    SetTevStates(6);
    tex->Load(GX_TEXMAP0, CTexture::kCM_Repeat);
  } else {
    SetTevStates(2);
    SetTevOp(kTS_Stage0, kEnvPassthru);
  }

  // Draw
  CGX::Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
  GXPosition3f32(x1, y1, 1.f);
  GXColor1u32(c);
  GXTexCoord2f32(0.f, 0.f);
  GXPosition3f32(x2, y1, 1.f);
  GXColor1u32(c);
  GXTexCoord2f32(1.f, 0.f);
  GXPosition3f32(x1, y2, 1.f);
  GXColor1u32(c);
  GXTexCoord2f32(0.f, 1.f);
  GXPosition3f32(x2, y2, 1.f);
  GXColor1u32(c);
  GXTexCoord2f32(1.f, 1.f);
  CGX::End();

  // Restore state
  if (mLightsWereOn != 0) {
    SetLightState(mLightsWereOn);
  }
  FlushProjection();
  mIsGXModelMatrixIdentity = false;
  SetModelMatrix(mModelMatrix);
  SetCullMode(cullMode);
}

void CGraphics::DrawPrimitive(ERglPrimitive primitive, const float* pos, const CVector3f& normal,
                              const CColor& col, int numVerts) {
  StreamBegin(primitive);
  StreamNormal(reinterpret_cast< const float* >(&normal));
  StreamColor(col);
  for (int i = 0; i < numVerts; ++i) {
    StreamVertex(pos + i * 3);
  }
  StreamEnd();
}

#define STREAM_PRIM_BUFFER_SIZE 240

#define NRM_BUFFER_ADDR(base) ((base) + ((STREAM_PRIM_BUFFER_SIZE + 1) * sizeof(float)))
#define TXT0_BUFFER_ADDR(base)                                                                     \
  (NRM_BUFFER_ADDR(base) + ((STREAM_PRIM_BUFFER_SIZE + 1) * sizeof(float)))
#define TXT1_BUFFER_ADDR(base)                                                                     \
  (TXT0_BUFFER_ADDR(base) + ((STREAM_PRIM_BUFFER_SIZE + 1) * sizeof(Vec2)))
#define CLR_BUFFER_ADDR(base)                                                                      \
  (TXT1_BUFFER_ADDR(base) + ((STREAM_PRIM_BUFFER_SIZE + 1) * sizeof(Vec2)))

static const uchar kHasNormals = 1;
static const uchar kHasColor = 2;
static const uchar kHasTexture = 4;

void CGraphics::StreamBegin(ERglPrimitive primitive) {
#ifdef TARGET_PC
  // Port: there is no locked cache on a host (its base is null), so each stream gets its own
  // full-size array; one extra slot covers ResetVertexDataStream's fan/strip carry-over.
  static Vec sVtx[STREAM_PRIM_BUFFER_SIZE + 2];
  static Vec sNrm[STREAM_PRIM_BUFFER_SIZE + 2];
  static Vec2 sTxt0[STREAM_PRIM_BUFFER_SIZE + 2];
  static Vec2 sTxt1[STREAM_PRIM_BUFFER_SIZE + 2];
  static uint sClr[STREAM_PRIM_BUFFER_SIZE + 2];
  mVertexBuffer = sVtx;
  mNormalBuffer = sNrm;
  mTexCoordBuffer0 = sTxt0;
  mTexCoordBuffer1 = sTxt1;
  mColorBuffer = sClr;
#else
  uchar* base = static_cast< uchar* >(GetLockedCacheAllocationBase());
  mVertexBuffer = reinterpret_cast< VecPtr >(base);
  mNormalBuffer = reinterpret_cast< VecPtr >(NRM_BUFFER_ADDR(base));
  mTexCoordBuffer0 = reinterpret_cast< Vec2Ptr >(TXT0_BUFFER_ADDR(base));
  mTexCoordBuffer1 = reinterpret_cast< Vec2Ptr >(TXT1_BUFFER_ADDR(base));
  mColorBuffer = reinterpret_cast< uint* >(CLR_BUFFER_ADDR(base));
#endif
  ResetVertexDataStream(true);
  mCurrentPrimitive = primitive;
  vtxDescr.mStreamFlags = kHasColor;
}

void CGraphics::StreamVertex(float x, float y, float z) {
  vtxDescr.mPosition.x = x;
  vtxDescr.mPosition.y = y;
  vtxDescr.mPosition.z = z;
  UpdateVertexDataStream();
}

void CGraphics::StreamVertex(const float* vtx) {
  vtxDescr.mPosition.x = vtx[0];
  vtxDescr.mPosition.y = vtx[1];
  vtxDescr.mPosition.z = vtx[2];
  UpdateVertexDataStream();
}

void CGraphics::StreamVertex(const CVector3f& vtx) {
  vtxDescr.mPosition.x = vtx.GetX();
  vtxDescr.mPosition.y = vtx.GetY();
  vtxDescr.mPosition.z = vtx.GetZ();
  UpdateVertexDataStream();
}

void CGraphics::StreamNormal(const float* nrm) {
  vtxDescr.mNormal.x = nrm[0];
  vtxDescr.mNormal.y = nrm[1];
  vtxDescr.mNormal.z = nrm[2];
  vtxDescr.mStreamFlags |= kHasNormals;
}

void CGraphics::StreamColor(uint color) {
  vtxDescr.mColor = color;
  vtxDescr.mStreamFlags |= kHasColor;
}

void CGraphics::StreamColor(const CColor& color) {
#ifdef TARGET_PC
  // mColor is 0xRRGGBBAA (GXColor1u32); GetColor_u32 is the host-order memory word.
  vtxDescr.mColor = (color.GetRedu8() << 24) | (color.GetGreenu8() << 16) |
                    (color.GetBlueu8() << 8) | color.GetAlphau8();
#else
  vtxDescr.mColor = color.GetColor_u32();
#endif
  vtxDescr.mStreamFlags |= kHasColor;
}

void CGraphics::StreamColor(float r, float g, float b, float a) {
  // clang-format off
  vtxDescr.mColor = (static_cast< uchar >(r * 255.f) << 24) |
                   (static_cast< uchar >(g * 255.f) << 16) |
                   (static_cast< uchar >(b * 255.f) << 8) |
                   static_cast< uchar >(a * 255.f);
  // clang-format on
  vtxDescr.mStreamFlags |= kHasColor;
}

void CGraphics::StreamTexcoord(const CVector2f& uv) {
  vtxDescr.mTexCoord0.x = uv.GetX();
  vtxDescr.mTexCoord0.y = uv.GetY();
  vtxDescr.mStreamFlags |= kHasTexture;
  vtxDescr.mTextureUsed |= 1;
}

void CGraphics::StreamTexcoord(float u, float v) {
  vtxDescr.mTexCoord0.x = u;
  vtxDescr.mTexCoord0.y = v;
  vtxDescr.mStreamFlags |= kHasTexture;
  vtxDescr.mTextureUsed |= 1;
}

void CGraphics::StreamEnd() {
  if (mNumPrimitives != 0) {
    FlushStream();
  }
  mVertexBuffer = nullptr;
  vtxDescr.mStreamFlags = 0;
  vtxDescr.mTextureUsed = 0;
  mNormalBuffer = nullptr;
  mTexCoordBuffer0 = nullptr;
  mTexCoordBuffer1 = nullptr;
  mColorBuffer = nullptr;
}

void CGraphics::SetLineWidth(float w, ERglTexOffset offs) {
  CGX::SetLineWidth(static_cast< uchar >(w * 6.f), static_cast< GXTexOffset >(offs));
}

void CGraphics::UpdateVertexDataStream() {
  ++mNumPrimitives;
  mpVtxBuffer->x = vtxDescr.mPosition.x;
  mpVtxBuffer->y = vtxDescr.mPosition.y;
  mpVtxBuffer->z = vtxDescr.mPosition.z;
  ++mpVtxBuffer;
  if ((vtxDescr.mStreamFlags & kHasNormals) != 0) {
    mpNrmBuffer->x = vtxDescr.mNormal.x;
    mpNrmBuffer->y = vtxDescr.mNormal.y;
    mpNrmBuffer->z = vtxDescr.mNormal.z;
    ++mpNrmBuffer;
  }
  if ((vtxDescr.mStreamFlags & kHasTexture) != 0) {
    mpTxtBuffer0->x = vtxDescr.mTexCoord0.x;
    mpTxtBuffer0->y = vtxDescr.mTexCoord0.y;
    ++mpTxtBuffer0;

    mpTxtBuffer1->x = vtxDescr.mTexCoord1.x;
    mpTxtBuffer1->y = vtxDescr.mTexCoord1.y;
    ++mpTxtBuffer1;
  }
  if ((vtxDescr.mStreamFlags & kHasColor) != 0) {
    *mpClrBuffer = vtxDescr.mColor;
    ++mpClrBuffer;
  }
  mJustReset = 0;
  if (mNumPrimitives == STREAM_PRIM_BUFFER_SIZE) {
    FlushStream();
    ResetVertexDataStream(false);
  }
}

void CGraphics::ResetVertexDataStream(bool initial) {
  mpVtxBuffer = mVertexBuffer;
  mpNrmBuffer = mNormalBuffer;
  mpTxtBuffer0 = mTexCoordBuffer0;
  mpTxtBuffer1 = mTexCoordBuffer1;
  mpClrBuffer = mColorBuffer;
  mNumPrimitives = 0;

  if (initial) {
    return;
  }

  switch (mCurrentPrimitive) {
  case kP_TriangleFan:
    mpVtxBuffer = mVertexBuffer + 1;
    memcpy(mpVtxBuffer, &vtxDescr.mPosition, sizeof(Vec));
    ++mpVtxBuffer;

    if ((vtxDescr.mStreamFlags & kHasNormals) != 0) {
      ++mpNrmBuffer;
      memcpy(mpNrmBuffer, &vtxDescr.mNormal, sizeof(Vec));
      ++mpNrmBuffer;
    }

    if ((vtxDescr.mStreamFlags & kHasTexture) != 0) {
      ++mpTxtBuffer0;
      memcpy(mpTxtBuffer0, &vtxDescr.mTexCoord0, sizeof(Vec2));
      ++mpTxtBuffer0;

      ++mpTxtBuffer1;
      memcpy(mpTxtBuffer1, &vtxDescr.mTexCoord1, sizeof(Vec2));
      ++mpTxtBuffer1;
    }

    if ((vtxDescr.mStreamFlags & kHasColor) != 0) {
      ++mpClrBuffer;
      *mpClrBuffer = vtxDescr.mColor;
      ++mpClrBuffer;
    }

    mNumPrimitives += 2;
    break;

  default:
    break;
  }

  mJustReset = 1;
}

void CGraphics::FlushStream() {
#ifdef TARGET_PC
  // Port diagnostic: the stream's primitive is a static, so a bad value means something wrote
  // over it (or a caller forgot StreamBegin). Aurora then aborts on the FIFO thread with no clue
  // about who produced it; report the stream state here, while the game frames are live.
  switch (static_cast< int >(mCurrentPrimitive)) {
  case GX_QUADS:
  case GX_TRIANGLES:
  case GX_TRIANGLESTRIP:
  case GX_TRIANGLEFAN:
  case GX_LINES:
  case GX_LINESTRIP:
  case GX_POINTS:
    break;
  default:
    std::fprintf(stderr,
                 "[gx] FlushStream with primitive %d, %d primitives queued, stream flags %02x\n",
                 static_cast< int >(mCurrentPrimitive), mNumPrimitives, vtxDescr.mStreamFlags);
    std::fflush(stderr);
    std::raise(SIGSEGV);
    break;
  }
#endif
  GXVtxDescList descriptors[10];
  GXVtxDescList* next = descriptors;
  const GXVtxDescList position = {GX_VA_POS, GX_DIRECT};
  *next++ = position;
  if ((vtxDescr.mStreamFlags & kHasNormals) != 0) {
    const GXVtxDescList normal = {GX_VA_NRM, GX_DIRECT};
    *next++ = normal;
  }
  if ((vtxDescr.mStreamFlags & kHasColor) != 0) {
    const GXVtxDescList color = {GX_VA_CLR0, GX_DIRECT};
    *next++ = color;
  }
  if ((vtxDescr.mStreamFlags & kHasTexture) != 0) {
    const GXVtxDescList texture = {GX_VA_TEX0, GX_DIRECT};
    *next++ = texture;
  }
  const GXVtxDescList end = {GX_VA_NULL, GX_NONE};
  *next = end;
  CGX::SetVtxDescv(descriptors);
#ifdef TARGET_PC
  // Port: with GX_DIRECT attributes the FIFO decoder reads each attribute with the *format's*
  // type, and VTXFMT0's types are global state that other code reconfigures - the fluid path
  // sets VTXFMT0 positions to U8 for its own drawing and does not put them back, so the stream's
  // float vertices were then read as bytes and the decoder lost sync (which surfaced as
  // "unsupported primitive type" in Aurora's FIFO thread, and only in areas that had drawn
  // fluid first). Declare what this stream actually writes, every flush.
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  if ((vtxDescr.mStreamFlags & kHasNormals) != 0) {
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  }
  if ((vtxDescr.mStreamFlags & kHasColor) != 0) {
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  }
  if ((vtxDescr.mStreamFlags & kHasTexture) != 0) {
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  }
#endif
  SetTevStates(vtxDescr.mStreamFlags);
  if (sUseStreamVertexDelay) {
    FullRenderWithVertexDelay();
  } else {
    FullRender();
  }
}

void CGraphics::SetTevStates(uchar flags) {
  switch (flags) {
  case 0:
  case kHasNormals:
  case kHasColor:
  case kHasNormals | kHasColor:
    CGX::SetNumChans(1);
    CGX::SetNumTexGens(0);
    CGX::SetNumTevStages(1);
    CGX::SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    CGX::SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    break;
  case kHasTexture:
  case kHasNormals | kHasTexture:
  case kHasColor | kHasTexture:
  case kHasNormals | kHasColor | kHasTexture:
    CGX::SetNumChans(1);
    if ((vtxDescr.mTextureUsed & 3) != 0) {
      CGX::SetNumTexGens(2);
    } else {
      CGX::SetNumTexGens(1);
    }
    CGX::SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    CGX::SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
    break;
  }
  CGX::SetNumIndStages(0);
  CGX::SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, false, GX_PTIDENTITY);
  CGX::SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY, false, GX_PTIDENTITY);

  uint light = mLightActive;
  GXAttnFn attnFn = GX_AF_NONE;
  if (light != 0) {
    attnFn = GX_AF_SPOT;
  }
  GXDiffuseFn diffFn = GX_DF_NONE;
  if (light != 0) {
    diffFn = GX_DF_CLAMP;
  }
  CGX::SetChanCtrl(CGX::Channel0, light ? GX_ENABLE : GX_DISABLE, GX_SRC_REG,
                   (flags & kHasColor) ? GX_SRC_VTX : GX_SRC_REG, static_cast< GXLightID >(light),
                   diffFn, attnFn);
}

// Guessed name. The alternate path deliberately delays every vertex by eight instructions.
static inline void DelayStreamVertex() {
#ifdef TARGET_PC
  // Port: a delay has no meaning against Aurora.
#else
  asm {
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
  }
#endif
}

void CGraphics::FullRenderWithVertexDelay() {
  CGX::Begin(static_cast< GXPrimitive >(mCurrentPrimitive), GX_VTXFMT0, mNumPrimitives);
  switch (vtxDescr.mStreamFlags) {
  case 0:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      DelayStreamVertex();
    }
    break;
  case kHasNormals:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXNormal3f32(mNormalBuffer[i].x, mNormalBuffer[i].y, mNormalBuffer[i].z);
      DelayStreamVertex();
    }
    break;
  case kHasColor:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXColor1u32(mColorBuffer[i]);
      DelayStreamVertex();
    }
    break;
  case kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXTexCoord2f32(mTexCoordBuffer0[i].x, mTexCoordBuffer0[i].y);
      DelayStreamVertex();
    }
    break;
  case kHasNormals | kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXNormal3f32(mNormalBuffer[i].x, mNormalBuffer[i].y, mNormalBuffer[i].z);
      GXTexCoord2f32(mTexCoordBuffer0[i].x, mTexCoordBuffer0[i].y);
      DelayStreamVertex();
    }
    break;
  case kHasNormals | kHasColor:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXNormal3f32(mNormalBuffer[i].x, mNormalBuffer[i].y, mNormalBuffer[i].z);
      GXColor1u32(mColorBuffer[i]);
      DelayStreamVertex();
    }
    break;
  case kHasColor | kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXColor1u32(mColorBuffer[i]);
      GXTexCoord2f32(mTexCoordBuffer0[i].x, mTexCoordBuffer0[i].y);
      DelayStreamVertex();
    }
    break;
  case kHasNormals | kHasColor | kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      GXPosition3f32(mVertexBuffer[i].x, mVertexBuffer[i].y, mVertexBuffer[i].z);
      GXNormal3f32(mNormalBuffer[i].x, mNormalBuffer[i].y, mNormalBuffer[i].z);
      GXColor1u32(mColorBuffer[i]);
      GXTexCoord2f32(mTexCoordBuffer0[i].x, mTexCoordBuffer0[i].y);
      DelayStreamVertex();
    }
    break;
  }
  CGX::End();
}

void CGraphics::FullRender() {
  CGX::Begin(static_cast< GXPrimitive >(mCurrentPrimitive), GX_VTXFMT0, mNumPrimitives);
  switch (vtxDescr.mStreamFlags) {
  case 0:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
    }
    break;
  case kHasNormals:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      const Vec& nrm = mNormalBuffer[i];
      GXNormal3f32(nrm.x, nrm.y, nrm.z);
    }
    break;
  case kHasColor:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      GXColor1u32(mColorBuffer[i]);
    }
    break;
  case kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      const Vec2& uv = mTexCoordBuffer0[i];
      GXTexCoord2f32(uv.x, uv.y);
    }
    break;
  case kHasNormals | kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      const Vec& nrm = mNormalBuffer[i];
      GXNormal3f32(nrm.x, nrm.y, nrm.z);
      const Vec2& uv = mTexCoordBuffer0[i];
      GXTexCoord2f32(uv.x, uv.y);
    }
    break;
  case kHasNormals | kHasColor:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      const Vec& nrm = mNormalBuffer[i];
      GXNormal3f32(nrm.x, nrm.y, nrm.z);
      GXColor1u32(mColorBuffer[i]);
    }
    break;
  case kHasColor | kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      GXColor1u32(mColorBuffer[i]);
      const Vec2& uv = mTexCoordBuffer0[i];
      GXTexCoord2f32(uv.x, uv.y);
    }
    break;
  case kHasNormals | kHasColor | kHasTexture:
    for (int i = 0; i < mNumPrimitives; i++) {
      const Vec& vtx = mVertexBuffer[i];
      GXPosition3f32(vtx.x, vtx.y, vtx.z);
      const Vec& nrm = mNormalBuffer[i];
      GXNormal3f32(nrm.x, nrm.y, nrm.z);
      GXColor1u32(mColorBuffer[i]);
      const Vec2& uv = mTexCoordBuffer0[i];
      GXTexCoord2f32(uv.x, uv.y);
    }
    break;
  }
  CGX::End();
}

void CGraphics::SetDepthRange(float near, float far) {
  mDepthNear = near;
  mDepthFar = far;
  GXSetViewport(static_cast< float >(mViewport.mLeft), static_cast< float >(mViewport.mTop),
                static_cast< float >(mViewport.mWidth), static_cast< float >(mViewport.mHeight),
                mDepthNear, mDepthFar);
}

static inline GXTevStageID get_texture_unit(const ERglTevStage stage) {
  return static_cast< GXTevStageID >(stage);
}

void CGraphics::SetTevOp(ERglTevStage stage, const CTevCombiners::CTevPass& pass) {
  CTevCombiners::SetupPass(get_texture_unit(stage), pass);
}

void CGraphics::SetTevRegisterColor(int index, const CColor& color) {
  CTevCombiners::SetTevRegisterColor(index, color);
}

void CGraphics::SetFog(ERglFogMode mode, float startz, float endz, const CColor& color) {
  CGX::SetFog(static_cast< GXFogType >(mode), startz, endz, mProj.GetNear(), mProj.GetFar(),
              color.GetGXColor());
}

void CGraphics::ResetGfxStates() { mRenderState.Set(0); }

void CGraphics::SetDefaultVtxAttrFmt() {
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_NRM, GX_NRM_XYZ, GX_S16, 14);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_NRM, GX_NRM_XYZ, GX_S16, 14);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_U16, 15);
  for (int i = 1; i <= 7; ++i) {
    GXAttr attr = static_cast< GXAttr >(GX_VA_TEX0 + i);
    GXSetVtxAttrFmt(GX_VTXFMT0, attr, GX_TEX_ST, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, attr, GX_TEX_ST, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT2, attr, GX_TEX_ST, GX_F32, 0);
  }
}

void CGraphics::LoadDolphinSpareTexture(int width, int height, GXTexFmt fmt, void* data,
                                        GXTexMapID texId) {
  GXTexObj texObj;
#ifdef TARGET_PC
  // Aurora resolves a GXCopyTex into a texture, so the game's main-memory copy destination is only
  // filled by the port's read-back (PortEfbCopy); land the latest one before it is uploaded.
  void* texData = data != nullptr ? data : mpSpareBuffer;
  PortEfbCopy::Fill(texData, width, height, CopyTexelBytes(fmt));
  GXInitTexObj(&texObj, texData, width, height, fmt, GX_CLAMP, GX_CLAMP, GX_DISABLE);
#else
  GXInitTexObj(&texObj, data != nullptr ? data : mpSpareBuffer, width, height, fmt, GX_CLAMP,
               GX_CLAMP, GX_DISABLE);
#endif
  GXInitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
  GXLoadTexObj(&texObj, texId);
  CTexture::InvalidateTexmap(texId);
  if (texId == GX_TEXMAP7) {
    GXInvalidateTexRegion(&mTexRegions[0]);
  }
}

void CGraphics::LoadDolphinSpareTexture(int width, int height, GXCITexFmt fmt, GXTlut tlut,
                                        void* data, GXTexMapID texId) {
  GXTexObj texObj;
#ifdef TARGET_PC
  // The scan visor samples these bytes as palette indices, and this is where they are consumed:
  // land the latest completed EFB copy before the texture is uploaded. The copy that fills this
  // texture is an A8 one, so its identity is one byte per texel.
  void* texData = data != nullptr ? data : mpSpareBuffer;
  PortEfbCopy::Fill(texData, width, height, 1);
  GXInitTexObjCI(&texObj, texData, width, height, fmt, GX_CLAMP, GX_CLAMP, GX_DISABLE, tlut);
#else
  GXInitTexObjCI(&texObj, data != nullptr ? data : mpSpareBuffer, width, height, fmt, GX_CLAMP,
                 GX_CLAMP, GX_DISABLE, tlut);
#endif
  GXInitTexObjLOD(&texObj, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
  GXLoadTexObj(&texObj, texId);
  CTexture::InvalidateTexmap(texId);
  if (texId == GX_TEXMAP7) {
    GXInvalidateTexRegion(&mTexRegionsCI[0]);
  }
}

void CGraphics::TickRenderTimings() {
  mRenderTimings = (mRenderTimings + 1) % (900 * 60);
  mSecondsMod900 = static_cast< float >(mRenderTimings) / 60.f;
}

#ifdef TARGET_PC
void CGraphics::PortAdvanceRenderTimings(uint ticks) {
  mRenderTimings = (mRenderTimings + ticks) % (900 * 60);
  mSecondsMod900 = static_cast< float >(mRenderTimings) / 60.f;
  sRenderTimingsByTick = true;
}

bool CGraphics::PortTakeRenderTimingsByTick() {
  const bool byTick = sRenderTimingsByTick;
  sRenderTimingsByTick = false;
  return byTick;
}

void CGraphics::SetRenderTimePresentLag(float seconds) { sRenderTimePresentLag = seconds; }

void CGraphics::PortLoadScreenProjectionTexMtx(uint texMtx) {
  Mtx44 proj;
  if (mProj.IsPerspective()) {
    MTXFrustum(proj, mProj.GetTop(), mProj.GetBottom(), mProj.GetLeft(), mProj.GetRight(),
               mProj.GetNear(), mProj.GetFar());
  } else {
    MTXOrtho(proj, mProj.GetTop(), mProj.GetBottom(), mProj.GetLeft(), mProj.GetRight(),
             mProj.GetNear(), mProj.GetFar());
  }
  // s, t and q as functionals of the view-space point (x, y, z, 1).
  float rows[3][4];
  for (int i = 0; i < 4; ++i) {
    rows[0][i] = 0.5f * (proj[0][i] + proj[3][i]);
    rows[1][i] = 0.5f * (proj[3][i] - proj[1][i]);
    rows[2][i] = proj[3][i];
  }
  // Then of the object-space point, through the modelview (its implicit fourth row (0, 0, 0, 1)).
  Mtx tex;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 4; ++c) {
      tex[r][c] = rows[r][0] * mGxModelView[0][c] + rows[r][1] * mGxModelView[1][c] +
                  rows[r][2] * mGxModelView[2][c];
    }
    tex[r][3] += rows[r][3];
  }
  GXLoadTexMtxImm(tex, texMtx, GX_MTX3x4);
}

#endif
float CGraphics::GetSecondsMod900() {
  if (mpExternalTimeProvider != nullptr) {
    return mpExternalTimeProvider->GetSecondsMod900();
  }
#ifdef TARGET_PC
  if (sRenderTimePresentLag > 0.0) {
    double s = static_cast< double >(mSecondsMod900) - sRenderTimePresentLag;
    if (s < 0.0) {
      s += 900.0;
    }
    return static_cast< float >(s);
  }
#endif
  return mSecondsMod900;
}

void CGraphics::SetExternalTimeProvider(CTimeProvider* timeProvider) {
  mpExternalTimeProvider = timeProvider;
}

void CGraphics::FlushProjection() {
  float right = mProj.GetRight();
  float left = mProj.GetLeft();
  float top = mProj.GetTop();
  float bottom = mProj.GetBottom();
  float near = mProj.GetNear();
  float far = mProj.GetFar();
  if (mProj.IsPerspective()) {
    Mtx44 mtx;
    MTXFrustum(mtx, top, bottom, left, right, near, far);
    GXSetProjection(mtx, GX_PERSPECTIVE);
  } else {
    Mtx44 mtx;
    MTXOrtho(mtx, top, bottom, left, right, near, far);
    GXSetProjection(mtx, GX_ORTHOGRAPHIC);
  }
}

const CGraphics::CProjectionState& CGraphics::GetProjectionState() { return mProj; }

void CGraphics::SetProjectionState(const CProjectionState& proj) {
  mProj = proj;
  FlushProjection();
}

CGraphics::CClippedScreenQuad
CGraphics::ClipScreenQuadFromVS(const CVector3f& p1, const CVector3f& p2, const CVector3f& p3,
                                const CVector3f& p4, ETexelFormat fmt) {
  if (p1.GetY() < GetProjectionState().GetNear() || p2.GetY() < GetProjectionState().GetNear() ||
      p3.GetY() < GetProjectionState().GetNear() || p4.GetY() < GetProjectionState().GetNear()) {
    return CClippedScreenQuad();
  }
  if (p1.GetY() > GetProjectionState().GetFar() || p2.GetY() > GetProjectionState().GetFar() ||
      p3.GetY() > GetProjectionState().GetFar() || p4.GetY() > GetProjectionState().GetFar()) {
    return CClippedScreenQuad();
  }

  rstl::reserved_vector< CVector2f, 4 > texCoords;
  rstl::reserved_vector< CVector2i, 4 > points;
  points.push_back(ProjectPoint(p1));
  points.push_back(ProjectPoint(p2));
  points.push_back(ProjectPoint(p3));
  points.push_back(ProjectPoint(p4));
  int minX = rstl::min_val(rstl::min_val(points[0].GetX(), points[1].GetX()),
                           rstl::min_val(points[2].GetX(), points[3].GetX()));
  int minY = rstl::min_val(rstl::min_val(points[0].GetY(), points[1].GetY()),
                           rstl::min_val(points[2].GetY(), points[3].GetY()));
  int maxX = rstl::max_val(rstl::max_val(points[0].GetX(), points[1].GetX()),
                           rstl::max_val(points[2].GetX(), points[3].GetX()));
  int maxY = rstl::max_val(rstl::max_val(points[0].GetY(), points[1].GetY()),
                           rstl::max_val(points[2].GetY(), points[3].GetY()));
  const CViewport& vp = mViewport;
  const int width = maxX - minX;
  const int height2 = maxY - minY;
  const int vpLeft = vp.mLeft;
  const int vpWidth = vp.mWidth;
  const int vpTop = vp.mTop;
  const int vpHeight = vp.mHeight;

  int left = minX & ~1;
  if (left >= vpLeft + vpWidth) {
    return CClippedScreenQuad();
  }
  int right = (minX + width + 2) & ~1;
  if (right <= vpLeft) {
    return CClippedScreenQuad();
  }
  left = rstl::max_val(left, vpLeft) & ~1;
  right = (rstl::min_val(right, vpLeft + vpWidth) + 1) & ~1;

  int top = minY & ~1;
  if (top >= vpTop + vpHeight) {
    return CClippedScreenQuad();
  }
  int bottom = (minY + height2 + 2) & ~1;
  if (bottom <= vpTop) {
    return CClippedScreenQuad();
  }
  top = rstl::max_val(top, vpTop) & ~1;
  bottom = (rstl::min_val(bottom, vpTop + vpHeight) + 1) & ~1;

  int texAlign = 4;
  switch (fmt) {
  case kTF_I8:
    texAlign = 8;
    break;
  case kTF_IA8:
  case kTF_RGB565:
  case kTF_RGB5A3:
    texAlign = 4;
    break;
  case kTF_RGBA8:
    texAlign = 2;
    break;
  }
  const int texWidth = (right - left + texAlign - 1) & ~(texAlign - 1);
  if (left + texWidth > mViewport.mWidth) {
    left = mViewport.mWidth - texWidth;
  }
  for (int i = 0; i < 4; ++i) {
    texCoords.push_back(CVector2f(float(points[i].GetX() - left) / float(texWidth - 1),
                                  float(points[i].GetY() - top) / float(bottom - top - 1)));
  }
  return CClippedScreenQuad(left, top, right - left, texWidth, bottom - top, texCoords);
}

CGraphics::CClippedScreenQuad
CGraphics::ClipScreenQuadFromMS(const CVector3f& p1, const CVector3f& p2, const CVector3f& p3,
                                const CVector3f& p4, ETexelFormat fmt) {
  return ClipScreenQuadFromVS(mViewMatrix.TransposeMultiply(mModelMatrix * p1),
                              mViewMatrix.TransposeMultiply(mModelMatrix * p2),
                              mViewMatrix.TransposeMultiply(mModelMatrix * p3),
                              mViewMatrix.TransposeMultiply(mModelMatrix * p4), fmt);
}

float CGraphics::GetFPS() {
  BOOL level = OSDisableInterrupts();
  float value = rstl::min_val(mFramesPerSecond, mLastFramesPerSecond);
  OSRestoreInterrupts(level);
  return value;
}

void CGraphics::SetUseVideoFilter(const bool b) {
  mUseVideoFilter = b;
  GXSetCopyFilter(mRenderModeObj.aa, mRenderModeObj.sample_pattern, b ? GX_ENABLE : GX_DISABLE,
                  mRenderModeObj.vfilter);
}

bool CGraphics::GetUseVideoFilter() { return mUseVideoFilter; }

int CGraphics::GetFrameCounter() { return mFrameCounter; }

CVector2i CGraphics::ProjectPoint(const CVector3f& point) {
  CVector3f vec = GetPerspectiveProjectionMatrix().MultiplyOneOverW(point);
  vec.SetX(vec.GetX() * GetViewport().mHalfWidth + GetViewport().mHalfWidth);
  vec.SetY(-vec.GetY() * GetViewport().mHalfHeight + GetViewport().mHalfHeight);
  return CVector2i(CCast::FtoL(vec.GetX()), CCast::FtoL(vec.GetY()));
}

void CGraphics::SetProgressiveMode(bool b) {
  bool isProgressive = GetProgressiveMode();
  OSSetProgressiveMode(b ? OS_PROGRESSIVE_MODE_ON : OS_PROGRESSIVE_MODE_OFF);
  if (b == isProgressive) {
    return;
  }

  VISetBlack(TRUE);
  VIFlush();
  VIWaitForRetrace();
  for (int i = 0; i < 10; ++i) {
    VIWaitForRetrace();
  }

  if (b) {
    mRenderModeObj.viTVmode = VI_TVMODE_NTSC_PROG;
    mRenderModeObj.xFBmode = VI_XFBMODE_SF;
    SetProgressiveFilter(mRenderModeObj);
  } else {
    mRenderModeObj.viTVmode = VI_TVMODE_NTSC_INT;
    mRenderModeObj.xFBmode = VI_XFBMODE_DF;
    memcpy(mRenderModeObj.vfilter, GXNtsc480IntDf.vfilter, 7);
  }
  GXSetCopyFilter(mRenderModeObj.aa, mRenderModeObj.sample_pattern, GX_ENABLE,
                  mRenderModeObj.vfilter);

  VIConfigure(&mRenderModeObj);
  VISetBlack(TRUE);
  VIFlush();
  for (int i = 0; i < 100; ++i) {
    VIWaitForRetrace();
  }
  VISetBlack(FALSE);
  VIFlush();
  for (int i = 0; i < 2; ++i) {
    VIWaitForRetrace();
  }
}

bool CGraphics::GetProgressiveMode() { return mRenderModeObj.viTVmode == VI_TVMODE_NTSC_PROG; }

bool CGraphics::CanSetProgressiveMode() { return VIGetDTVStatus() != 0; }

bool CGraphics::GetProgressiveDefault() { return OSGetProgressiveMode() == OS_PROGRESSIVE_MODE_ON; }

void CGraphics::GetScreenPosition(int* stretch, int* xOffset, int* yOffset) {
  if (stretch != nullptr) {
    *stretch = mScreenStretch;
  }
  if (xOffset != nullptr) {
    *xOffset = mScreenPositionX;
  }
  if (yOffset != nullptr) {
    *yOffset = mScreenPositionY;
  }
}

void CGraphics::SetScreenPosition(int stretch, int xOffset, int yOffset) {
  int stretchChange = stretch - mScreenStretch;
  int xChange = xOffset - mScreenPositionX;
  int yChange = yOffset - mScreenPositionY;
  if (stretchChange == 0 && xChange == 0 && yChange == 0) {
    return;
  }
  mRenderModeObj.viWidth += stretchChange * 2;
  mRenderModeObj.viXOrigin += xChange - stretchChange;
  mRenderModeObj.viYOrigin += yChange;
  VIConfigure(&mRenderModeObj);
  VIFlush();
  mScreenStretch = stretch;
  mScreenPositionX = xOffset;
  mScreenPositionY = yOffset;
}

void CGraphics::SetIsBeginSceneClearFb(bool b) { mIsBeginSceneClearFb = b; }

void CGraphics::SetUseNormalMatrix(bool enabled) {
  if (mUseNormalMatrix != enabled) {
    mUseNormalMatrix = enabled;
    if (enabled) {
      SetViewMatrix();
    }
  }
}

void CGraphics::SetUseStreamVertexDelay(bool enabled) { sUseStreamVertexDelay = enabled; }

CGraphicsSys::CGraphicsSys(const COsContext& osContext, const CMemorySys& memorySys,
                           bool progressive) {
  if (mGraphicsInitialized != true) {
    mGraphicsInitialized = CGraphics::Startup(osContext, progressive);
  }
}

CGraphicsSys::~CGraphicsSys() {
  if (mGraphicsInitialized == true) {
    CGraphics::Shutdown();
    mGraphicsInitialized = false;
  }
}

CGraphics::CRenderState::CRenderState() {
  x0_ = 0;
  x4_ = 0;
}

void CGraphics::CRenderState::Flush() {}

int CGraphics::CRenderState::SetVtxState(const float* pos, const float* nrm, const uint* clr) {
  CGX::SetArray(GX_VA_POS, pos, 12);
  CGX::SetArray(GX_VA_NRM, nrm, 12);
  CGX::SetArray(GX_VA_CLR0, clr, 4);
  int result = 1;
  if (nrm != nullptr) {
    result |= 2;
  }
  if (clr != nullptr) {
    result |= 16;
  }
  return result;
}

void CGraphics::CRenderState::ResetFlushAll() {
  x0_ = 0;
  SetVtxState(nullptr, nullptr, nullptr);
  for (int i = 0; i < 8; i++) {
    CGX::SetArray(static_cast< GXAttr >(GX_VA_TEX0 + i), nullptr, 8);
  }
  Flush();
}
