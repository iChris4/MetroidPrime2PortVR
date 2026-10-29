#include <aurora/aurora.h>
#include <aurora/phase.hpp>
#include <aurora/time.hpp>

#ifdef AURORA_ENABLE_GX
#include "gfx/resources.hpp"
#include "gfx/depth_peek.hpp"
#include "gfx/overdraw.hpp"
#include "gfx/frame.hpp"
#include "gfx/pipeline_cache.hpp"
#include "gfx/recording.hpp"
#include "gfx/texture.hpp"
#include "gfx/render_worker.hpp"
#include "gx/command_processor.hpp"
#include "gx/fifo.hpp"
#include "gx/gx.hpp"
#include "gx/texture.hpp"
#include "imgui.hpp"
#include "io.hpp"
#include "stereo_host.hpp"
#include "webgpu/gpu.hpp"
#include "webgpu/gpu_prof.hpp"
#include "gfx/perf_counters.hpp"
#include <webgpu/webgpu_cpp.h>
#endif

#ifdef AURORA_ENABLE_RMLUI
#include "rmlui.hpp"
#endif

#include "input.hpp"
#include "internal.hpp"
#include "screenshot.hpp"
#include "thread.hpp"
#include "window.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_surface.h>
#include <magic_enum.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <string>
#include <chrono>
#include <cstdlib>
#include <filesystem>

#include "system_info.hpp"
#include "tracy/Tracy.hpp"

namespace aurora {
AuroraConfig g_config;
// aurora_set_graphics_quality request: msaa << 16 | anisotropy, 0 = none.
std::atomic<uint32_t> g_pendingQuality{0};
uint32_t g_sdlCustomEventsStart;
char g_gameName[4];

namespace {
constexpr Module Log{"aurora"};

#ifdef AURORA_ENABLE_GX
std::atomic_bool g_screenshotRequested;
// Port: AURORA_RENDER_EVERY=<n> renders one frame in n (1, the default, renders every frame).
// The frames in between are dropped before they are recorded, so the FIFO processor records no
// draw and the render worker encodes and submits nothing for them: a test run that only shows
// one frame in n does not pay for the frames it never shows.
uint32_t render_every() {
  static const uint32_t n = [] {
    const char* env = std::getenv("AURORA_RENDER_EVERY");
    const long v = env != nullptr ? std::strtol(env, nullptr, 10) : 1;
    return v > 1 ? static_cast<uint32_t>(v) : 1u;
  }();
  return n;
}

// Port: AURORA_HEADLESS=1: a test run that needs no picture. No window surface is made (SDL's
// offscreen video driver needs no X server), the WebGPU device is Dawn's Null backend, and no
// frame is rendered: the FIFO is still processed (the register state the game reads back), but
// nothing is drawn, encoded, submitted or presented, and the EFB read-backs (scan peek, EFB CPU
// copy, depth snapshots) never land, so the game keeps its own "no read-back yet" values.
bool headless_flag() noexcept {
  static const bool on = [] {
    const char* env = std::getenv("AURORA_HEADLESS");
    return env != nullptr && env[0] != '\0' && std::string_view(env) != "0";
  }();
  return on;
}

// The frame counter that picks the rendered frames. It advances once per frame, so with the same
// n as AURORA_PRESENT_EVERY the rendered frames are the presented ones.
uint32_t g_renderFrameCounter = 0;
// Set while a screenshot is pending: the frame the shot is taken in and the frame after it are
// both rendered, so the shot is never taken from a frame that was not drawn.
std::atomic_bool g_renderNextFrame{false};
// A PNG request names its file and is answered through g_pngState (0 pending, 1 saved, -1 failed).
std::mutex g_pngMutex;
std::string g_pngPath;
std::atomic_int g_pngState{0};
std::atomic_uint32_t g_pngWidth{0};
std::atomic_uint32_t g_pngHeight{0};


// GPU
using webgpu::g_device;
using webgpu::g_queue;
using webgpu::g_surface;

void record_screenshot(wgpu::CommandEncoder& encoder, std::vector<gfx::AfterSubmitCallback>& callbacks) {
  std::string pngPath;
  {
    std::lock_guard lock{g_pngMutex};
    pngPath = std::move(g_pngPath);
    g_pngPath.clear();
  }
  const bool png = !pngPath.empty();
  const auto& source = webgpu::present_source();
  const bool bgra = source.format == wgpu::TextureFormat::BGRA8Unorm ||
                    source.format == wgpu::TextureFormat::BGRA8UnormSrgb;
  const bool rgba = source.format == wgpu::TextureFormat::RGBA8Unorm ||
                    source.format == wgpu::TextureFormat::RGBA8UnormSrgb;
  if (!bgra && !rgba) {
    Log.warn("Screenshot unsupported for texture format {}", magic_enum::enum_name(source.format));
    if (png) {
      g_pngState.store(-1, std::memory_order_release);
    }
    return;
  }

  const uint32_t width = source.size.width;
  const uint32_t height = source.size.height;
  const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
  const uint64_t byteSize = static_cast<uint64_t>(bytesPerRow) * height;
  const wgpu::BufferDescriptor descriptor{
      .label = "Screenshot readback",
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = byteSize,
  };
  auto readback = g_device.CreateBuffer(&descriptor);
  const wgpu::TexelCopyTextureInfo src{.texture = source.texture};
  const wgpu::TexelCopyBufferInfo dst{
      .layout =
          wgpu::TexelCopyBufferLayout{
              .bytesPerRow = bytesPerRow,
              .rowsPerImage = height,
          },
      .buffer = readback,
  };
  encoder.CopyTextureToBuffer(&src, &dst, &source.size);

  callbacks.emplace_back([readback, width, height, bytesPerRow, byteSize, bgra, png, pngPath]() {
    readback.MapAsync(
        wgpu::MapMode::Read, 0, byteSize, wgpu::CallbackMode::AllowSpontaneous,
        [readback, width, height, bytesPerRow, byteSize, bgra, png, pngPath](wgpu::MapAsyncStatus status,
                                                                          wgpu::StringView message) {
          if (status != wgpu::MapAsyncStatus::Success) {
            if (status != wgpu::MapAsyncStatus::CallbackCancelled && status != wgpu::MapAsyncStatus::Aborted) {
              Log.warn("Screenshot readback failed {}: {}", magic_enum::enum_name(status), message);
            }
            if (png) {
              g_pngState.store(-1, std::memory_order_release);
            }
            return;
          }

          const void* pixels = readback.GetConstMappedRange(0, byteSize);
          // The present source's alpha is not meaningful and is usually 0, so a surface with an
          // alpha channel saves an image every viewer shows as fully transparent (a 32-bit BMP
          // with an alpha mask, or a PNG whose alpha is 0). The X formats drop it: SDL writes a
          // 24-bit BMP and converts to opaque RGBA for PNG.
          SDL_Surface* surface = SDL_CreateSurfaceFrom(
              static_cast<int>(width), static_cast<int>(height),
              bgra ? SDL_PIXELFORMAT_BGRX32 : SDL_PIXELFORMAT_RGBX32, const_cast<void*>(pixels),
              static_cast<int>(bytesPerRow));
          if (surface == nullptr) {
            Log.warn("Unable to create screenshot surface: {}", SDL_GetError());
            if (png) {
              g_pngState.store(-1, std::memory_order_release);
            }
          } else if (png) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(pngPath).parent_path(), ec);
            if (!SDL_SavePNG(surface, pngPath.c_str())) {
              Log.warn("Unable to save screenshot {}: {}", pngPath, SDL_GetError());
              g_pngState.store(-1, std::memory_order_release);
            } else {
              Log.info("Screenshot saved to {}", pngPath);
              g_pngWidth.store(width, std::memory_order_relaxed);
              g_pngHeight.store(height, std::memory_order_relaxed);
              g_pngState.store(1, std::memory_order_release);
            }
            SDL_DestroySurface(surface);
          } else {
            std::error_code ec;
            std::filesystem::create_directories("screenshots", ec);
            const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
            const std::string path = "screenshots/metroid-prime-" + std::to_string(stamp) + ".bmp";
            if (!SDL_SaveBMP(surface, path.c_str())) {
              Log.warn("Unable to save screenshot {}: {}", path, SDL_GetError());
            } else {
              Log.info("Screenshot saved to {}", path);
            }
            SDL_DestroySurface(surface);
          }
          readback.Unmap();
        });
  });
}

// PrimedGun: AURORA_SCREENSHOT_EYES=1 saves the eyes of an immersive frame next to its screenshot
// (screenshots/metroid-prime-<stamp>-eye<0|1>.bmp), so a desk run can check what each eye shows
// without a headset on. Debug only: the eye images are full size.
bool screenshot_eyes_wanted() noexcept {
  static const bool wanted = [] {
    const char* env = std::getenv("AURORA_SCREENSHOT_EYES");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
  }();
  return wanted;
}

void record_eye_screenshot(wgpu::CommandEncoder& encoder, std::vector<gfx::AfterSubmitCallback>& callbacks,
                           const gfx::StereoEyeOutput& output, uint32_t eye) {
  const bool bgra = output.format == wgpu::TextureFormat::BGRA8Unorm ||
                    output.format == wgpu::TextureFormat::BGRA8UnormSrgb;
  const bool rgba = output.format == wgpu::TextureFormat::RGBA8Unorm ||
                    output.format == wgpu::TextureFormat::RGBA8UnormSrgb;
  if (!output.texture || (!bgra && !rgba)) {
    Log.warn("Eye screenshot unsupported for texture format {}", magic_enum::enum_name(output.format));
    return;
  }
  const uint32_t width = output.size.width;
  const uint32_t height = output.size.height;
  const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
  const uint64_t byteSize = static_cast<uint64_t>(bytesPerRow) * height;
  const wgpu::BufferDescriptor descriptor{
      .label = "Eye screenshot readback",
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = byteSize,
  };
  auto readback = g_device.CreateBuffer(&descriptor);
  const wgpu::TexelCopyTextureInfo src{.texture = output.texture, .origin = {0, 0, output.layer}};
  const wgpu::TexelCopyBufferInfo dst{
      .layout = wgpu::TexelCopyBufferLayout{.bytesPerRow = bytesPerRow, .rowsPerImage = height},
      .buffer = readback,
  };
  const wgpu::Extent3D extent{width, height, 1};
  encoder.CopyTextureToBuffer(&src, &dst, &extent);
  callbacks.emplace_back([readback, width, height, bytesPerRow, byteSize, bgra, eye]() {
    readback.MapAsync(
        wgpu::MapMode::Read, 0, byteSize, wgpu::CallbackMode::AllowSpontaneous,
        [readback, width, height, bytesPerRow, byteSize, bgra, eye](wgpu::MapAsyncStatus status, wgpu::StringView) {
          if (status != wgpu::MapAsyncStatus::Success) {
            return;
          }
          const void* pixels = readback.GetConstMappedRange(0, byteSize);
          SDL_Surface* surface = SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height),
                                                       bgra ? SDL_PIXELFORMAT_BGRX32 : SDL_PIXELFORMAT_RGBX32,
                                                       const_cast<void*>(pixels), static_cast<int>(bytesPerRow));
          if (surface != nullptr) {
            std::error_code ec;
            std::filesystem::create_directories("screenshots", ec);
            const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
            const std::string path =
                "screenshots/metroid-prime-" + std::to_string(stamp) + "-eye" + std::to_string(eye) + ".bmp";
            if (SDL_SaveBMP(surface, path.c_str())) {
              Log.info("Eye screenshot saved to {}", path);
            }
            SDL_DestroySurface(surface);
          }
          readback.Unmap();
        });
  });
}

uint32_t clamp_scissor_coord(double value, uint32_t maximum) noexcept {
  if (!std::isfinite(value)) {
    return 0;
  }
  return static_cast<uint32_t>(std::clamp(value, 0.0, static_cast<double>(maximum)));
}

void set_present_viewport(const wgpu::RenderPassEncoder& pass, const gfx::Viewport& viewport, uint32_t surfaceWidth,
                          uint32_t surfaceHeight) noexcept {
  pass.SetViewport(viewport.left, viewport.top, viewport.width, viewport.height, viewport.znear, viewport.zfar);
  const auto scissorX = clamp_scissor_coord(std::floor(viewport.left), surfaceWidth);
  const auto scissorY = clamp_scissor_coord(std::floor(viewport.top), surfaceHeight);
  const auto scissorRight = clamp_scissor_coord(std::ceil(viewport.left + viewport.width), surfaceWidth);
  const auto scissorBottom = clamp_scissor_coord(std::ceil(viewport.top + viewport.height), surfaceHeight);
  pass.SetScissorRect(scissorX, scissorY, scissorRight - scissorX, scissorBottom - scissorY);
}
#endif

#ifdef AURORA_ENABLE_GX
constexpr std::array PreferredBackendOrder{
#ifdef ENABLE_BACKEND_WEBGPU
    BACKEND_WEBGPU,
#endif
#ifdef DAWN_ENABLE_BACKEND_D3D12
    BACKEND_D3D12,
#endif
#ifdef DAWN_ENABLE_BACKEND_METAL
    BACKEND_METAL,
#endif
#ifdef DAWN_ENABLE_BACKEND_VULKAN
    BACKEND_VULKAN,
#endif
#ifdef DAWN_ENABLE_BACKEND_D3D11
    BACKEND_D3D11,
#endif
// Desktop GL last among the GPU backends: for GPUs whose D3D and Vulkan drivers all fail (OpenGL 3.3
// hardware), before the software adapters of the second pass.
#ifdef DAWN_ENABLE_BACKEND_DESKTOP_GL
    BACKEND_OPENGL,
#endif
#ifdef DAWN_ENABLE_BACKEND_OPENGLES
    BACKEND_OPENGLES,
#endif
#ifdef DAWN_ENABLE_BACKEND_NULL
    BACKEND_NULL,
#endif
};
#else
constexpr std::array<AuroraBackend, 0> PreferredBackendOrder{};
#endif

bool g_initialFrame = false;

AuroraInfo initialize(int argc, char* argv[], const AuroraConfig& config) noexcept {
  g_config = config;
  Log.info("Aurora initializing");
  log_system_information();
  if (g_config.appName == nullptr) {
    g_config.appName = "Aurora";
  } else {
    g_config.appName = strdup(g_config.appName);
  }
  if (g_config.userPath == nullptr) {
    g_config.userPath = SDL_GetPrefPath(nullptr, g_config.appName);
    if (g_config.userPath == nullptr) { // MP2 port, see cachePath below
      g_config.userPath = strdup("");
    }
  } else {
    g_config.userPath = strdup(g_config.userPath);
  }
  if (g_config.cachePath == nullptr) {
    g_config.cachePath = SDL_GetPrefPath(nullptr, g_config.appName);
    if (g_config.cachePath == nullptr) { // MP2 port: fails for names Windows rejects (':'); never leave it null
      g_config.cachePath = strdup("");
    }
  } else {
    g_config.cachePath = strdup(g_config.cachePath);
  }
  if (g_config.resourcesPath == nullptr) {
    g_config.resourcesPath = SDL_GetBasePath();
  } else {
    g_config.resourcesPath = strdup(g_config.resourcesPath);
  }
  if (g_config.msaa == 0) {
    g_config.msaa = 1;
  }
  if (g_config.maxTextureAnisotropy == 0) {
    g_config.maxTextureAnisotropy = 16;
  }
  AURORA_ASSERT(window::initialize(), "Error initializing window");

  g_sdlCustomEventsStart = SDL_RegisterEvents(2);
  AURORA_ASSERT(g_sdlCustomEventsStart, "Failed to allocate user events: {}", SDL_GetError());
  AURORA_ASSERT(window::initialize_event_watch(), "Error initializing SDL event watch");

#ifdef AURORA_ENABLE_GX
  /* Attempt to create a window using the calling application's desired backend */
  AuroraBackend selectedBackend = config.desiredBackend;
  bool windowCreated = false;
  if (headless()) {
    // Port: AURORA_HEADLESS: a window (no surface is made from it) and the Null device.
    Log.info("AURORA_HEADLESS: Null WebGPU device, no surface, no frame is rendered");
    selectedBackend = BACKEND_NULL;
    windowCreated = window::create_window(BACKEND_NULL) && webgpu::initialize(BACKEND_NULL, true);
  } else if (selectedBackend != BACKEND_AUTO && window::create_window(selectedBackend)) {
    if (webgpu::initialize(selectedBackend, config.allowCpuAdapter)) {
      windowCreated = true;
    } else {
      window::destroy_window();
    }
  }

  // Without a usable GPU (a virtual machine without 3D acceleration, a broken driver), a software
  // adapter such as llvmpipe or WARP still draws a picture, slowly; the Null backend draws nothing.
  // So unless CPU adapters are allowed from the start, a second pass takes them before Null.
  const int passes = config.allowCpuAdapter ? 1 : 2;
  for (int pass = 0; pass < passes && !windowCreated; ++pass) {
    const bool allowCpu = config.allowCpuAdapter || pass == 1;
    for (const auto backendType : PreferredBackendOrder) {
      if (pass + 1 < passes && backendType == BACKEND_NULL) {
        continue;
      }
      selectedBackend = backendType;
      if (!window::create_window(selectedBackend)) {
        continue;
      }
      if (webgpu::initialize(selectedBackend, allowCpu)) {
        windowCreated = true;
        break;
      } else {
        window::destroy_window();
      }
    }
  }

  AURORA_ASSERT(windowCreated, "Error creating window: {}", SDL_GetError());

  if (webgpu::g_backendType == wgpu::BackendType::Null && config.noGraphicsMessage != nullptr) {
    Log.error("No graphics device could be started; exiting");
    // No parent: the window isn't shown yet, and a box owned by a hidden window may not show either.
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, config.appName, config.noGraphicsMessage, nullptr);
    window::destroy_window();
    SDL_Quit();
    std::exit(1);
  }

  // Initialize SDL_Renderer for ImGui when we can't use a Dawn backend
  if (webgpu::g_backendType == wgpu::BackendType::Null && !headless()) {
    AURORA_ASSERT(window::create_renderer(), "Failed to initialize SDL renderer: {}", SDL_GetError());
  }
#else
  AuroraBackend selectedBackend = BACKEND_NULL;
  AURORA_ASSERT(window::create_window(BACKEND_NULL), "Error creating window: {}", SDL_GetError());
  AURORA_ASSERT(window::create_renderer(), "Failed to initialize SDL renderer: {}", SDL_GetError());
#endif

  window::show_window();
  thread::set_current({
      .name = "Main thread",
      .affinity = thread::Affinity::SharedCache,
  });

#ifdef AURORA_ENABLE_GX
  gfx::initialize();
  gx::fifo::init();
  gx::fifo::set_headless(headless());
  imgui::create_context();
#endif
  const auto size = window::get_window_size();
  Log.info("Using framebuffer size {}x{} scale {}", size.fb_width, size.fb_height, size.scale);
#ifdef AURORA_ENABLE_GX
  if (g_config.imGuiInitCallback != nullptr) {
    g_config.imGuiInitCallback(&size);
  }
  imgui::initialize();
#endif

#ifdef AURORA_ENABLE_RMLUI
  rmlui::initialize(size);
#endif

  g_initialFrame = true;
  g_config.desiredBackend = selectedBackend;
  return {
      .backend = selectedBackend,
      .userPath = g_config.userPath,
      .cachePath = g_config.cachePath,
      .window = window::get_sdl_window(),
      .windowSize = size,
  };
}

void shutdown() noexcept {
#ifdef AURORA_ENABLE_GX
  gx::fifo::shutdown();
  gfx::render_worker::synchronize();
#ifdef AURORA_ENABLE_RMLUI
  rmlui::shutdown();
#endif
  imgui::shutdown();
  gfx::shutdown();
  webgpu::shutdown();
#endif
  input::shutdown();
  window::shutdown();
}

const AuroraEvent* update() noexcept {
  ZoneScoped;
  phase::set(phase::Main, "window event pump");
  if (g_initialFrame) {
    g_initialFrame = false;
    input::initialize();
  }
#ifdef AURORA_ENABLE_GX
  gx::update();
#endif
  const AuroraEvent* events = window::poll_events();
  phase::set(phase::Main, "game update");
  return events;
}

#ifdef AURORA_ENABLE_GX
// Returns false (after dropping the surface where needed) when no frame can be drawn
// because the surface is changing or gone.
static bool service_surface(const char* caller) noexcept {
  // Android keeps the native window across surfaceChanged. Releasing the surface here
  // would rebuild it on that same window while the old swapchain is still connected,
  // which fails with VK_ERROR_NATIVE_WINDOW_IN_USE_KHR (a lost device). Skip the frame.
  if (window::is_surface_changing()) {
    return false;
  }
  const bool invalidated = window::consume_surface_invalidated();
  if (!window::is_presentable() || (invalidated && webgpu::surface_window_changed())) {
    static int sNotPresentable = 0;
    if (sNotPresentable++ < 3) {
      Log.warn("{}: not presentable - surfaceReady={} backgrounded={} invalidated={}", caller,
               window::is_surface_ready(), window::is_backgrounded(), invalidated);
    }
    webgpu::release_surface();
    return false;
  }
  return true;
}
#endif

void release_lost_surface() noexcept {
#ifdef AURORA_ENABLE_GX
  if (g_surface) {
    service_surface("release_lost_surface");
  }
#endif
}

#ifdef AURORA_ENABLE_GX
// Port: AURORA_RENDER_EVERY: pick the frame that is about to be recorded. A frame that is not
// rendered records no draw: the FIFO processor walks its command stream (the register state has
// to survive) but builds no draw, and the render worker encodes and submits nothing for it. A
// frame that holds an EFB copy is raised to "rendered" while it is recorded (see
// gx::fifo::force_frame_rendered). A screenshot is always taken from a rendered frame, and so is
// the frame after it.
bool render_this_frame() {
  if (headless()) {
    return false;
  }
  const uint32_t every = render_every();
  const uint32_t frameIndex = g_renderFrameCounter++;
  const bool screenshotPending = g_screenshotRequested.load(std::memory_order_acquire);
  const bool forced = g_renderNextFrame.exchange(false, std::memory_order_acq_rel);
  if (screenshotPending) {
    // The shot is taken at the end of this frame, so this frame has to be drawn; the frame after
    // it too, so the read-back of the EFB lands on a frame the game drew.
    g_renderNextFrame.store(true, std::memory_order_release);
    return true;
  }
  if (forced) {
    return true;
  }
  return every == 1 || (frameIndex % every) == 0;
}
#endif

bool begin_frame(uint64_t contentTag) noexcept {
  ZoneScoped;
#ifdef AURORA_ENABLE_GX
  // Port: AURORA_HEADLESS has no surface to check or rebuild.
  if (!headless()) {
    if (!service_surface("begin_frame")) {
      return false;
    }
    if (window::is_paused()) {
      static int sPaused = 0;
      if (sPaused++ < 3) {
        Log.warn("begin_frame: paused - surfaceReady={} backgrounded={}", window::is_surface_ready(),
                 window::is_backgrounded());
      }
      return false;
    }
    if (!g_surface) {
      webgpu::refresh_surface(true);
      if (!g_surface) {
        return false;
      }
    }
    if (const uint32_t quality = g_pendingQuality.exchange(0, std::memory_order_acq_rel); quality != 0) {
      webgpu::set_quality(quality >> 16, static_cast<uint16_t>(quality & 0xFFFF));
    }
  }

  phase::set(phase::Main, "begin_frame (waiting for a frame slot)");
  if (!gfx::begin_frame()) {
    return false;
  }
  // The stereo packet for this frame, before the first uniform is staged.
  const gfx::StereoFrameState frameStereo = stereo_host::begin_frame(contentTag);
  gfx::set_frame_stereo(frameStereo);
  // A failed staging-buffer acquisition must not leave an ImGui frame open.
  imgui::new_frame(window::get_window_size());
  gx::fifo::begin_frame();
  // Port: AURORA_RENDER_EVERY: the frame is recorded as usual, but a frame that is not rendered
  // records no draw (gx::fifo::set_frame_rendered tells the FIFO processor and the recorder).
  // A stereo frame is always rendered: the headset's frame loop waits for every frame it hands out.
  gx::fifo::set_frame_rendered(frameStereo.active || render_this_frame());
  // Port: AURORA_RENDERSKIP_COPYDEST: the render interval is the default limit on how stale a
  // copy's texture may be before its frame is rendered in full.
  gx::fifo::set_render_interval(render_every());
#endif
  return true;
}

void end_frame(uint64_t contentTag) noexcept {
  ZoneScoped;
  // The frames just after an EFB resize, timed for its log line (webgpu::resize_swapchain).
  struct ResizeFrameTimer {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    bool timed = webgpu::g_resizeFramesToTime > 0;
    ~ResizeFrameTimer() {
      if (timed) {
        const int index = 3 - webgpu::g_resizeFramesToTime--;
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        Log.info("EFB resize: frame {} after it took {:.1f} ms to record, submit and present", index, ms);
      }
    }
  } resizeFrameTimer;
#ifdef AURORA_ENABLE_GX
  phase::set(phase::Main, "end_frame (recording and queueing the frame)");
  gx::fifo::drain();
  gx::fifo::end_frame();
  gx::texture::end_frame();
  // On a standalone headset nothing shows the mono image of an immersive frame's
  // final pass (no EFB copy follows it, the window is not presented), so only its
  // eyes are rendered. Anything that would read it keeps it: a screenshot, a depth
  // snapshot, or a frame the sink would show as mono (stereo_host::encode).
  const bool headsetOwnsDisplay = stereo_host::headset_owns_display();
  // The desktop window showing an eye instead of the flat image needs that image as little
  // (stereo_host::mirror_skips_mono): the window is then drawn from the eye targets below.
  const int mirrorView = stereo_host::mirror_view();
  const bool mirrorSkipsMono = stereo_host::mirror_skips_mono();
  bool monoUnneeded = false;
  {
    const auto stereo = gfx::recorded_stereo_state();
    monoUnneeded = (headsetOwnsDisplay || mirrorSkipsMono) && stereo.active && stereo.immersive &&
                   stereo.contentTag == contentTag && !g_screenshotRequested.load(std::memory_order_acquire) &&
                   !gfx::depth_peek::snapshot_wanted();
    gfx::set_final_pass_mono_unneeded(monoUnneeded);
  }
  gfx::finish();
  auto imguiDrawData = imgui::freeze();
  const uint32_t logicalFrame = gfx::current_frame();

  const auto& presentSource = webgpu::present_source();
  const auto viewport = webgpu::calculate_present_viewport(webgpu::g_graphicsConfig.surfaceConfiguration.width,
                                                           webgpu::g_graphicsConfig.surfaceConfiguration.height,
                                                           presentSource.size.width, presentSource.size.height);

  wgpu::BindGroup rmlBindGroup;
  bool rmlOverlay = false;
#if AURORA_ENABLE_RMLUI
  if (rmlui::is_initialized()) {
    auto rmlFrame = rmlui::record_frame(viewport);
    rmlBindGroup = std::move(rmlFrame.bindGroup);
    rmlOverlay = rmlFrame.overlay;
  }
#endif

  // Port: AURORA_RENDER_EVERY: a frame that was not rendered has nothing to encode, submit or
  // present. Its bookkeeping (the frame slot, the staging buffer) is done by gfx::end_frame, so
  // the game never waits for it.
  const bool renderFrame = gx::fifo::frame_rendered();
  const auto stereoState = gfx::recorded_stereo_state();
  gfx::end_frame([rmlBindGroup = std::move(rmlBindGroup), rmlOverlay, viewport, renderFrame, contentTag, logicalFrame,
                  stereoState,
                  headsetOwnsDisplay, mirrorView, mirrorSkipsMono, monoUnneeded,
                  imguiDrawData = std::move(imguiDrawData)](
                     wgpu::CommandEncoder& encoder, std::vector<gfx::AfterSubmitCallback> afterSubmitCallbacks) {
    wgpu::Texture currentTexture;
    wgpu::TextureView currentView;
    auto surfaceStatus = wgpu::SurfaceGetCurrentTextureStatus::Error;
    bool acquireAttempted = false;
    // Port: AURORA_PRESENT_EVERY=<n> presents one frame in n (test runs on Xvfb, where each present
    // is a CPU copy under MESA_VK_WSI_DEBUG=sw). The frame is still rendered; screenshots read the EFB.
    static const uint32_t presentEvery = [] {
      const char* env = std::getenv("AURORA_PRESENT_EVERY");
      const long n = env != nullptr ? std::strtol(env, nullptr, 10) : 1;
      return n > 1 ? static_cast<uint32_t>(n) : 1u;
    }();
    static uint32_t presentCounter = 0;
    // The counter advances once per frame, rendered or not, so the presented frames stay the ones
    // AURORA_RENDER_EVERY renders when both use the same n.
    const bool presentSlot = (presentCounter++ % presentEvery) == 0;
    // Port: a frame that was not rendered holds the last rendered frame's EFB, so it is not
    // presented either.
    const bool skipPresent = !renderFrame || !presentSlot;
    if (!renderFrame) {
      // Nothing was recorded for this frame: no draw, no copy, no submit. The screenshot request
      // (if any) stays pending, so the next frame is rendered and takes the shot.
      gfx::after_present();
      return;
    }
    if (g_screenshotRequested.exchange(false, std::memory_order_acq_rel)) {
      record_screenshot(encoder, afterSubmitCallbacks);
      if (screenshot_eyes_wanted() && stereoState.replayed) {
        for (uint32_t eye = 0; eye < AURORA_STEREO_EYE_COUNT; ++eye) {
          record_eye_screenshot(encoder, afterSubmitCallbacks, stereoState.outputs[eye], eye);
        }
      }
    }
    if (skipPresent) {
      gfx::after_present();
    }
    {
      window::SurfaceLock surfaceLock;
      if (!skipPresent && !headsetOwnsDisplay && window::is_presentable() && g_surface) {
        ZoneScopedN("Acquire texture");
        phase::set(phase::Render, "surface GetCurrentTexture");
        wgpu::SurfaceTexture surfaceTexture;
        g_surface.GetCurrentTexture(&surfaceTexture);
        phase::set(phase::Render, "present blit recording");
        acquireAttempted = true;
        surfaceStatus = surfaceTexture.status;
        if (surfaceStatus == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
          currentTexture = std::move(surfaceTexture.texture);
          currentView = currentTexture.CreateView();
        }
      }
    }

    const bool canPresent = currentTexture && currentView;
    if (canPresent) {
      const uint32_t surfaceWidth = webgpu::g_graphicsConfig.surfaceConfiguration.width;
      const uint32_t surfaceHeight = webgpu::g_graphicsConfig.surfaceConfiguration.height;
      // The window drawn from the eyes: their mono image was not rendered (monoUnneeded). A
      // frame whose eyes were not replayed after all keeps the flat image.
      const bool eyesForWindow = mirrorSkipsMono && monoUnneeded && stereoState.replayed;
      struct WindowEye {
        wgpu::BindGroup bindGroup;
        gfx::Viewport viewport;
      };
      std::array<WindowEye, AURORA_STEREO_EYE_COUNT> windowEyes{};
      uint32_t windowEyeCount = 0;
      wgpu::BindGroup presentBindGroup;
      if (eyesForWindow) {
        const auto eyeSource = [&](uint32_t eye) {
          const auto& output = stereoState.outputs[eye];
          return webgpu::TextureWithSampler{
              .texture = output.texture,
              .view = output.view,
              .size = output.size,
              .format = output.format,
              .sampler = webgpu::present_source().sampler,
          };
        };
        if (mirrorView == AURORA_STEREO_MIRROR_BOTH) {
          // Side by side, each eye letterboxed into its half.
          for (uint32_t eye = 0; eye < AURORA_STEREO_EYE_COUNT; ++eye) {
            const auto source = eyeSource(eye);
            if (!source.view) {
              continue;
            }
            auto half = webgpu::calculate_present_viewport(surfaceWidth / 2, surfaceHeight, source.size.width,
                                                           source.size.height);
            half.left += static_cast<float>(eye * (surfaceWidth / 2));
            windowEyes[windowEyeCount++] = {webgpu::create_copy_bind_group(source), half};
          }
        } else if (mirrorView == AURORA_STEREO_MIRROR_LEFT || mirrorView == AURORA_STEREO_MIRROR_RIGHT) {
          const auto source = eyeSource(mirrorView == AURORA_STEREO_MIRROR_RIGHT ? 1 : 0);
          if (source.view) {
            const auto eyeViewport =
                webgpu::calculate_present_viewport(surfaceWidth, surfaceHeight, source.size.width, source.size.height);
            windowEyes[windowEyeCount++] = {
                webgpu::create_copy_bind_group(webgpu::resample_source(encoder, eyeViewport, source)), eyeViewport};
          }
        }
        // AURORA_STEREO_MIRROR_NONE: the window stays clear under the overlay.
      } else if (rmlBindGroup && !rmlOverlay) {
        presentBindGroup = rmlBindGroup;
      } else {
        const auto& resampledSource = webgpu::resample_present_source(encoder, viewport);
        presentBindGroup = webgpu::create_copy_bind_group(resampledSource);
      }
      {
        const std::array attachments{
            wgpu::RenderPassColorAttachment{
                .view = currentView,
                .loadOp = wgpu::LoadOp::Clear,
                .storeOp = wgpu::StoreOp::Store,
            },
        };
        const wgpu::RenderPassDescriptor renderPassDescriptor{
            .label = "EFB copy render pass",
            .colorAttachmentCount = attachments.size(),
            .colorAttachments = attachments.data(),
            .timestampWrites = webgpu::gpu_prof::pass_writes("Present blit"),
        };
        const auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.SetPipeline(webgpu::g_CopyPipeline);
        if (eyesForWindow) {
          for (uint32_t i = 0; i < windowEyeCount; ++i) {
            pass.SetBindGroup(0, windowEyes[i].bindGroup, 0, nullptr);
            set_present_viewport(pass, windowEyes[i].viewport, surfaceWidth, surfaceHeight);
            pass.Draw(3);
          }
        } else {
          // Copy EFB -> XFB (swapchain)
          pass.SetBindGroup(0, presentBindGroup, 0, nullptr);
          set_present_viewport(pass, viewport, surfaceWidth, surfaceHeight);
          pass.Draw(3);
        }
        if (rmlBindGroup && rmlOverlay) {
          pass.SetPipeline(webgpu::g_CopyPremultipliedAlphaPipeline);
          pass.SetBindGroup(0, rmlBindGroup, 0, nullptr);
          pass.Draw(3);
        }
        // ImGui draws in the same pass: a second pass would write the whole swapchain image out
        // and read it back in, which costs a tile-based (mobile) GPU a full-screen round trip.
        pass.SetViewport(0.f, 0.f, static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.width),
                         static_cast<float>(webgpu::g_graphicsConfig.surfaceConfiguration.height), 0.f, 1.f);
        imgui::render(pass, imguiDrawData);
        pass.End();
      }
    } else if (!skipPresent && !headsetOwnsDisplay) {
      Log.info("Skipping present; window not presentable");
    }
    // The headset's eyes, whether or not the window could be presented.
    const auto stereoSink = stereo_host::encode(encoder, stereoState, logicalFrame, contentTag);
    webgpu::gpu_prof::frame_end(encoder);
    gfx::overdraw::frame_end(encoder);
    const wgpu::CommandBufferDescriptor cmdBufDescriptor{.label = "Redraw command buffer"};
    const auto buffer = encoder.Finish(&cmdBufDescriptor);
    {
      ZoneScopedN("Queue Submit");
      phase::set(phase::Render, "queue submit");
      const gfx::perf::Timer timer{gfx::perf::g_submitNs};
      g_queue.Submit(1, &buffer);
    }
    gfx::sample_dawn_memory();
    phase::set(phase::Render, "after submit");
    webgpu::gpu_prof::after_submit();
    // The bridge's native follow-up on the same queue, then its completion callback.
    stereo_host::submitted(stereoSink);
    gfx::overdraw::after_submit();
    if (canPresent && g_surface) {
      ZoneScopedN("Present");
      wgpu::ConvertibleStatus status = wgpu::Status::Error;
      {
        window::SurfaceLock surfaceLock;
        if (window::is_presentable()) {
          phase::set(phase::Render, "surface Present");
          status = g_surface.Present();
        }
      }
      phase::set(phase::Render, "after present");
      if (status) {
        gfx::after_present();
      } else {
        Log.warn("Surface present failed");
        webgpu::release_surface();
      }
    } else if (g_surface && acquireAttempted) {
      // Without an attempt, surfaceStatus is only the Error placeholder: the window was briefly
      // not presentable (an Android surfaceChanged, say). Dropping the surface then made
      // begin_frame build a second VkSurface on the same ANativeWindow while the old swapchain
      // was still connected, which fails with VK_ERROR_NATIVE_WINDOW_IN_USE_KHR (device lost).
      switch (surfaceStatus) {
      case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
        Log.warn("Surface texture acquisition timed out");
        break;
      case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
      case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
        Log.info("Surface texture is {}, reconfiguring swapchain", magic_enum::enum_name(surfaceStatus));
        window::push_custom_event(window::CustomEvent::RefreshSurface);
        break;
      case wgpu::SurfaceGetCurrentTextureStatus::Lost:
        Log.warn("Surface texture is {}, releasing surface", magic_enum::enum_name(surfaceStatus));
        webgpu::release_surface();
        break;
      case wgpu::SurfaceGetCurrentTextureStatus::Error:
        Log.warn("Surface texture is {}, dropping surface", magic_enum::enum_name(surfaceStatus));
        g_surface = {};
        window::set_surface_held(false);
        break;
      default:
        if (!window::is_presentable()) {
          webgpu::release_surface();
        } else {
          Log.error("Failed to get surface texture: {}", magic_enum::enum_name(surfaceStatus));
        }
        break;
      }
    }
    for (auto& callback : afterSubmitCallbacks) {
      if (callback) {
        callback();
      }
    }
    gfx::after_submit();

    TracyPlotConfig("aurora: lastVertSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastUniformSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastIndexSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastStorageSize", tracy::PlotFormatType::Memory, false, true, 0);
    TracyPlotConfig("aurora: lastTextureUploadSize", tracy::PlotFormatType::Memory, false, true, 0);

    const auto& stats = gfx::detail::resources().stats;
    TracyPlot("aurora: queuedPipelines", static_cast<int64_t>(stats.queuedPipelines));
    TracyPlot("aurora: createdPipelines", static_cast<int64_t>(stats.createdPipelines));
    TracyPlot("aurora: drawCallCount", static_cast<int64_t>(stats.drawCallCount));
    TracyPlot("aurora: mergedDrawCallCount", static_cast<int64_t>(stats.mergedDrawCallCount));
    TracyPlot("aurora: lastVertSize", static_cast<int64_t>(stats.lastVertSize));
    TracyPlot("aurora: lastUniformSize", static_cast<int64_t>(stats.lastUniformSize));
    TracyPlot("aurora: lastIndexSize", static_cast<int64_t>(stats.lastIndexSize));
    TracyPlot("aurora: lastStorageSize", static_cast<int64_t>(stats.lastStorageSize));
    TracyPlot("aurora: lastTextureUploadSize", static_cast<int64_t>(stats.lastTextureUploadSize));
  });

#endif
}
} // namespace

bool headless() noexcept { return headless_flag(); }

// Port: AURORA_HEADLESS renders no frame, so there is no picture to take.
static bool refuse_headless_screenshot() noexcept {
  if (!headless()) {
    return false;
  }
  Log.warn("AURORA_HEADLESS: no frame is rendered, so the screenshot is not taken");
  return true;
}

void request_screenshot() noexcept {
#ifdef AURORA_ENABLE_GX
  if (refuse_headless_screenshot()) {
    return;
  }
  g_screenshotRequested.store(true, std::memory_order_release);
#endif
}
void request_screenshot_png(const char* path) noexcept {
#ifdef AURORA_ENABLE_GX
  if (refuse_headless_screenshot()) {
    g_pngState.store(-1, std::memory_order_release);
    return;
  }
  {
    std::lock_guard lock{g_pngMutex};
    g_pngPath = path != nullptr ? path : "";
  }
  g_pngState.store(0, std::memory_order_release);
  g_screenshotRequested.store(true, std::memory_order_release);
#endif
}

int screenshot_png_state(uint32_t* width, uint32_t* height) noexcept {
#ifdef AURORA_ENABLE_GX
  const int state = g_pngState.load(std::memory_order_acquire);
  if (state > 0) {
    if (width != nullptr) *width = g_pngWidth.load(std::memory_order_relaxed);
    if (height != nullptr) *height = g_pngHeight.load(std::memory_order_relaxed);
  }
  return state;
#else
  return -1;
#endif
}

bool frame_buffer_size_for_scale(float scale, uint32_t* width, uint32_t* height) noexcept {
  return window::get_frame_buffer_size_for_scale(scale, width, height);
}

uint32_t max_texture_dimension() noexcept {
  wgpu::Limits limits{};
  if (!webgpu::g_device || !webgpu::g_device.GetLimits(&limits)) {
    return 0;
  }
  return limits.maxTextureDimension2D;
}
} // namespace aurora

// C API bindings
AuroraInfo aurora_initialize(int argc, char* argv[], const AuroraConfig* config) {
  return aurora::initialize(argc, argv, *config);
}
void aurora_shutdown() {
#ifdef AURORA_ENABLE_GX
  aurora::stereo_host::shutdown();
#endif
  aurora::shutdown();
}
const AuroraEvent* aurora_update() { return aurora::update(); }
bool aurora_begin_frame() { return aurora::begin_frame(AURORA_STEREO_CONTENT_TAG_UNKNOWN); }
bool aurora_begin_frame_tagged(uint64_t contentTag) { return aurora::begin_frame(contentTag); }
void aurora_release_lost_surface() { aurora::release_lost_surface(); }
bool aurora_is_suspended() {
  return aurora::window::is_backgrounded() || !aurora::window::is_surface_ready() ||
         aurora::window::is_surface_changing();
}
void aurora_end_frame() { aurora::end_frame(AURORA_STEREO_CONTENT_TAG_UNKNOWN); }
void aurora_end_frame_tagged(uint64_t contentTag) { aurora::end_frame(contentTag); }
void aurora_end_frame_ex(uint64_t contentTag, void* imguiFrame) {
  (void)imguiFrame;
  aurora::end_frame(contentTag);
}
void aurora_set_background_pipelines_paused(bool paused) { aurora::gfx::set_background_pipelines_paused(paused); }
void aurora_disable_config_persistence() { aurora::io::disable_config_persistence(); }
AuroraBackend aurora_get_backend() { return aurora::g_config.desiredBackend; }
const char* aurora_get_gpu_driver() {
  static std::string driver;
  const wgpu::StringView description = aurora::webgpu::g_adapterInfo.description;
  driver = description.IsUndefined() ? std::string() : std::string(description.data, description.length);
  return driver.c_str();
}
bool aurora_vulkan_library_failed() { return aurora::webgpu::g_vulkanLibraryFailed; }
const AuroraBackend* aurora_get_available_backends(size_t* count) {
  if (count != nullptr) {
    *count = aurora::PreferredBackendOrder.size();
  }
  return aurora::PreferredBackendOrder.data();
}
void aurora_set_log_level(AuroraLogLevel level) { aurora::g_config.logLevel = level; }
void aurora_set_pause_on_focus_lost(bool value) { aurora::g_config.pauseOnFocusLost = value; }
void aurora_set_background_input(bool value) {
  aurora::g_config.allowJoystickBackgroundEvents = value;
  aurora::window::set_background_input(value);
}
void aurora_set_resampler(AuroraSampler sampler) {
#ifdef AURORA_ENABLE_GX
  aurora::webgpu::set_resampler(sampler);
#else
  (void)sampler;
#endif
}

void aurora_set_present_sharpness(float sharpness) {
#ifdef AURORA_ENABLE_GX
  aurora::webgpu::set_present_sharpness(sharpness);
#else
  (void)sharpness;
#endif
}
void aurora_set_graphics_quality(uint32_t msaa, uint16_t maxTextureAnisotropy) {
  aurora::g_pendingQuality.store(std::min(msaa, 4u) << 16 | maxTextureAnisotropy, std::memory_order_release);
}
void aurora_set_per_pixel_lighting(bool enabled) {
#ifdef AURORA_ENABLE_GX
  aurora::gx::set_per_pixel_lighting(enabled);
#else
  (void)enabled;
#endif
}
void aurora_set_timescale(float scale) { aurora::time::set_scale(scale); }
float aurora_get_timescale() { return aurora::time::scale(); }
