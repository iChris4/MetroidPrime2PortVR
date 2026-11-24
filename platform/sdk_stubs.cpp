// Temporary SDK stubs for functions the Aurora compatibility layer does not
// implement. These let the Metroid Prime port link; several will need real
// implementations for correct audio/VI/OS behaviour. Signatures are taken from
// the same headers the game compiled against (Aurora's dolphin/*).

#include <cmath>
#include <cstdio>
#include <thread>

#include <dolphin/ai.h>
#include <dolphin/ar.h>
#include <dolphin/dtk.h>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/os.h>
#include <dolphin/vi.h>

// CElementGen.cpp declares this SDK entry point directly.
extern "C" float frsqrte(float x);

// AI (audio DMA) is implemented in platform/ai_dma.cpp.

// --- MTX44 (paired-single 4x4 helpers Aurora's mtx library lacks) -----------
extern "C" void PSMTX44Identity(Mtx44 m) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] = i == j ? 1.f : 0.f;
        }
    }
}
extern "C" void PSMTX44Concat(const Mtx44 a, const Mtx44 b, Mtx44 ab) {
    Mtx44 tmp;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            tmp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            ab[i][j] = tmp[i][j];
        }
    }
}
extern "C" void PSMTX44Trans(Mtx44 m, f32 xT, f32 yT, f32 zT) {
    PSMTX44Identity(m);
    m[0][3] = xT;
    m[1][3] = yT;
    m[2][3] = zT;
}

// --- AR (ARAM) --------------------------------------------------------------
extern "C" u32 ARGetDMAStatus(void) {
    return 0;
}

// --- DTK (streamed audio tracks) --------------------------------------------
extern "C" void DTKInit(void) {}
extern "C" u32 DTKGetState(void) {
    return 0;
}
extern "C" int DTKSetState(u32 state) {
    (void)state;
    return 0;
}
extern "C" int DTKNextTrack(void) {
    return 0;
}
extern "C" int DTKFlushTracks(DTKFlushCallback callback) {
    (void)callback;
    return 0;
}
extern "C" u32 DTKQueueTrack(char* fileName, DTKTrack* track, u32 eventMask, DTKCallback callback) {
    (void)fileName;
    (void)track;
    (void)eventMask;
    (void)callback;
    return 0;
}
extern "C" void DTKSetRepeatMode(u32 repeat) {
    (void)repeat;
}
extern "C" void DTKSetSampleRate(u32 samplerate) {
    (void)samplerate;
}
extern "C" void DTKSetVolume(u8 left, u8 right) {
    (void)left;
    (void)right;
}

// --- GX gaps ----------------------------------------------------------------
extern "C" void GXAbortFrame(void) {}
extern "C" void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWaterMark, u32 loWaterMark) {
    (void)fifo;
    (void)hiWaterMark;
    (void)loWaterMark;
}
extern "C" void GXInvalidateTexRegion(const GXTexRegion* region) {
    (void)region;
}
extern "C" void GXSetMisc(GXMiscToken token, u32 val) {
    (void)token;
    (void)val;
}

// --- OS gaps ----------------------------------------------------------------
extern "C" void OSCancelAlarm(OSAlarm* alarm) {
    (void)alarm;
}
extern "C" BOOL OSDisableInterrupts(void) {
    return TRUE;
}
extern "C" BOOL OSEnableInterrupts(void) {
    return TRUE;
}
extern "C" u32 OSGetConsoleType(void) {
    return 0;
}
extern "C" OSThread* OSGetCurrentThread(void) {
    // Non-null dummy so callers that register/unregister do not dereference
    // null. Give it a plausible guest stack range inside MEM1.
    static OSThread s_dummyThread{};
    static bool init = false;
    if (!init) {
        init = true;
        s_dummyThread.stackBase = reinterpret_cast< u8* >(0x81800000u);
        s_dummyThread.stackEnd = reinterpret_cast< u8* >(0x81700000u);
        s_dummyThread.state = OS_THREAD_STATE_RUNNING;
    }
    return &s_dummyThread;
}
extern "C" u32 OSGetProgressiveMode(void) {
    return 0;
}
extern "C" BOOL OSGetResetButtonState(void) {
    return FALSE;
}
extern "C" void OSGetSavedRegion(void** start, void** end) {
    if (start != nullptr) {
        *start = nullptr;
    }
    if (end != nullptr) {
        *end = nullptr;
    }
}
// The IPL's sound setting: stereo, as MP1's port reports. Mono here forced the game's options to
// Mono at every start (CGameOptions::InitSoundMode).
extern "C" u32 OSGetSoundMode(void) {
    return OS_SOUND_MODE_STEREO;
}
// OSLink, OSLinkFixed and OSUnlink are not defined, on purpose: a no-op that
// returned TRUE would silently fake module loading. Retail's module manager
// (src/MetroidPrime/CRelFile.cpp) is adapted instead: where the cube
// links the disc image and calls its prolog, the host runs the compiled module's
// init (port::modules::Prolog), and the epilog/unlink pair becomes
// port::modules::Epilog. It does not use platform/rel.cpp's port::rel::LinkModule,
// because the image's code is PowerPC and relocating it gives the host nothing it
// can run. So a call to any of the three is still a link error.
// Port: Aurora has no console language/PAD recalibration state. English, and "recalibration was
// not enabled" respectively.
extern "C" u8 OSGetLanguage() { return 0; }
extern "C" BOOL __PADDisableRecalibration(BOOL enable) {
    (void)enable;
    return FALSE;
}
extern "C" void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
    (void)chan;
    (void)addr;
    (void)nBytes;
    (void)control;
}
extern "C" void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    (void)reset;
    (void)resetCode;
    (void)forceMenu;
}
extern "C" BOOL OSRestoreInterrupts(BOOL level) {
    (void)level;
    return TRUE;
}
extern "C" u32 OSSaveContext(OSContext* context) {
    (void)context;
    return 0;
}
extern "C" OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    (void)error;
    (void)handler;
    return nullptr;
}
extern "C" void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    (void)alarm;
    (void)start;
    (void)period;
    (void)handler;
}
extern "C" void OSSetProgressiveMode(u32 on) {
    (void)on;
}
extern "C" void OSSetSaveRegion(void* start, void* end) {
    (void)start;
    (void)end;
}
extern "C" void OSSetSoundMode(u32 mode) {
    (void)mode;
}
// OSUnlink: see the note above OSProtectRange - the module manager's unlink runs
// port::modules::Epilog instead.
extern "C" void OSYieldThread(void) { std::this_thread::yield(); }

// --- VI gaps ----------------------------------------------------------------
extern "C" u32 VIGetDTVStatus(void) {
    return 0;
}
extern "C" u32 VIGetNextField(void) {
    return 0;
}
extern "C" void VISetBlack(BOOL black) {
    (void)black;
}
extern "C" void VISetNextFrameBuffer(void* fb) {
    (void)fb;
}
// Port: the game registers VI retrace callbacks that flip framebuffers and
// advance its frame pacing. With no real retrace, GXEnableBreakPt (see
// platform/shims.cpp) pulses them to complete each frame.
namespace {
VIRetraceCallback s_preRetraceCallback = nullptr;
VIRetraceCallback s_postRetraceCallback = nullptr;
} // namespace

extern "C" void AuroraRetracePulse(void) {
    if (s_preRetraceCallback != nullptr) {
        s_preRetraceCallback(0);
    }
    if (s_postRetraceCallback != nullptr) {
        s_postRetraceCallback(0);
    }
}

extern "C" VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) {
    VIRetraceCallback previous = s_postRetraceCallback;
    s_postRetraceCallback = cb;
    return previous;
}
extern "C" VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) {
    VIRetraceCallback previous = s_preRetraceCallback;
    s_preRetraceCallback = cb;
    return previous;
}
extern "C" void VIWaitForRetrace(void) {}

// --- Aurora-declared but not linked -----------------------------------------
// Aurora lists these in headers but leaves the implementations as TODOs (and
// OSFatal's definition does not link), so provide them here.
extern "C" void GXInitTexCacheRegion(GXTexRegion* region, GXBool is_32b_mipmap, u32 tmem_even,
                                     GXTexCacheSize size_even, u32 tmem_odd,
                                     GXTexCacheSize size_odd) {
    (void)region;
    (void)is_32b_mipmap;
    (void)tmem_even;
    (void)size_even;
    (void)tmem_odd;
    (void)size_odd;
}
extern "C" GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback callback) {
    (void)callback;
    return nullptr;
}
extern "C" void OSFatal(GXColor fg, GXColor bg, const char* msg) {
    (void)fg;
    (void)bg;
    if (msg != nullptr) {
        std::fprintf(stderr, "OSFatal: %s\n", msg);
    }
    // Declared noreturn: returning would run off the end of the caller.
    std::abort();
}

// --- OS context -------------------------------------------------------------
// Referenced by RAssertDolphin's register dump and by REL_Setup's unlinked-import
// walk. Aurora declares these but does not implement them; on the port the host
// runtime owns the context, the PC OSContext is opaque, and there is no guest
// stack to walk, so they carry no console meaning.
extern "C" void OSClearContext(OSContext* context) {
    (void)context;
}
extern "C" void OSSetCurrentContext(OSContext* context) {
    (void)context;
}
extern "C" u32 OSGetStackPointer(void) {
    // Zero ends REL_Setup::_unresolved's stack walk immediately, which is what a
    // host-side build wants: that walk reads a PowerPC back chain.
    return 0;
}

// --- PowerPC math -----------------------------------------------------------
extern "C" float frsqrte(float x) {
    return 1.0f / std::sqrt(x);
}
