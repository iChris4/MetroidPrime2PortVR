// Feasibility spike: Dawn (GL/GLES backend) on Nintendo Switch homebrew via Mesa/nouveau EGL.
#include <switch.h>

#include <EGL/egl.h>

#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <cstring>

#include "egl_sync_shim.h"

#include <dawn/native/DawnNative.h>
#include <dawn/native/OpenGLBackend.h>
#include <webgpu/webgpu_cpp.h>

static FILE* gLog;

static void Log(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  svcOutputDebugString(buf, n < 0 ? 0 : (n < int(sizeof(buf)) ? n : int(sizeof(buf)) - 1));
  fputs(buf, stdout);
  if (gLog) {
    va_start(ap, fmt);
    vfprintf(gLog, fmt, ap);
    va_end(ap);
    fflush(gLog);
  }
}

static const char* kShader = R"(
struct Uniforms { t : f32 };
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut { @builtin(position) pos : vec4f, @location(0) col : vec3f };

@vertex fn vs(@builtin(vertex_index) i : u32) -> VsOut {
  var p = array(vec2f(0.0, 0.6), vec2f(-0.6, -0.5), vec2f(0.6, -0.5));
  var c = array(vec3f(1.0, 0.2, 0.2), vec3f(0.2, 1.0, 0.2), vec3f(0.2, 0.4, 1.0));
  let a = u.t;
  let r = mat2x2f(cos(a), sin(a), -sin(a), cos(a));
  var o : VsOut;
  o.pos = vec4f(r * p[i] * vec2f(9.0 / 16.0, 1.0), 0.0, 1.0);
  o.col = c[i];
  return o;
}

@fragment fn fs(v : VsOut) -> @location(0) vec4f { return vec4f(v.col, 1.0); }
)";

// Renders one frame into an offscreen texture and writes it to sdmc:/dawn-nx.ppm.
static void LogMem(const char* tag) {
  u64 total = 0, used = 0;
  svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
  svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
  Log("mem %s: used %lu MiB / total %lu MiB, applet type %d\n", tag, used >> 20, total >> 20, int(appletGetAppletType()));
}

static void DumpFrame(const wgpu::Instance& instance, const wgpu::Device& device,
                      const wgpu::RenderPipeline& pipeline, const wgpu::BindGroup& bg,
                      wgpu::TextureFormat format) {
  constexpr uint32_t W = 1280, H = 720, D = 256;
  wgpu::TextureDescriptor td{};
  td.size = {W, H, 1};
  td.format = format;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
  wgpu::Texture tex = device.CreateTexture(&td);
  wgpu::BufferDescriptor bd{};
  bd.size = uint64_t(D) * D * 4;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer buf = device.CreateBuffer(&bd);

  wgpu::RenderPassColorAttachment color{};
  color.view = tex.CreateView();
  color.loadOp = wgpu::LoadOp::Clear;
  color.storeOp = wgpu::StoreOp::Store;
  color.clearValue = {0.05, 0.05, 0.2, 1.0};
  wgpu::RenderPassDescriptor rp{};
  rp.colorAttachmentCount = 1;
  rp.colorAttachments = &color;
  wgpu::CommandEncoder enc = device.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, bg);
  pass.Draw(3);
  pass.End();
  // Only the central 256x256 block: under Ryujinx, a full-width readback keeps just the first
  // quarter of each row (plain GLES does the same), while a rect this narrow reads back right.
  wgpu::TexelCopyTextureInfo src{};
  src.texture = tex;
  src.origin = {W / 2 - D / 2, H / 2 - D / 2, 0};
  wgpu::TexelCopyBufferInfo dst{};
  dst.buffer = buf;
  dst.layout.bytesPerRow = D * 4;
  dst.layout.rowsPerImage = D;
  wgpu::Extent3D size{D, D, 1};
  enc.CopyTextureToBuffer(&src, &dst, &size);
  wgpu::CommandBuffer cb = enc.Finish();
  device.GetQueue().Submit(1, &cb);

  bool ok = false;
  instance.WaitAny(buf.MapAsync(wgpu::MapMode::Read, 0, bd.size, wgpu::CallbackMode::WaitAnyOnly,
                                [&](wgpu::MapAsyncStatus status, wgpu::StringView) {
                                  ok = status == wgpu::MapAsyncStatus::Success;
                                }),
                   UINT64_MAX);
  if (!ok) {
    Log("readback map failed\n");
    return;
  }
  const auto* px = static_cast<const uint8_t*>(buf.GetConstMappedRange(0, bd.size));
  const bool bgra = format == wgpu::TextureFormat::BGRA8Unorm || format == wgpu::TextureFormat::BGRA8UnormSrgb;
  uint32_t clear = 0, triangle = 0, zero = 0;
  FILE* f = fopen("sdmc:/dawn-nx.ppm", "wb");
  if (f) {
    fprintf(f, "P6\n%u %u\n255\n", D, D);
  }
  for (uint32_t i = 0; i < D * D; ++i) {
    const uint8_t rgb[3] = {px[i * 4 + (bgra ? 2 : 0)], px[i * 4 + 1], px[i * 4 + (bgra ? 0 : 2)]};
    if (f) {
      fwrite(rgb, 1, 3, f);
    }
    if (rgb[0] == 13 && rgb[1] == 13 && rgb[2] == 51) {
      ++clear;
    } else if (rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0) {
      ++zero;
    } else {
      ++triangle;
    }
  }
  if (f) {
    fclose(f);
  }
  const uint8_t* c = &px[(D / 2 * D + D / 2) * 4];
  Log("dumped centre %ux%u: clear %u, triangle %u, zero %u; centre %u,%u,%u\n", D, D, clear, triangle,
      zero, c[bgra ? 2 : 0], c[1], c[bgra ? 0 : 2]);
  buf.Unmap();
}

static bool Run(wgpu::BackendType backend) {
  static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  dawn::native::DawnInstanceDescriptor dawnDesc;
  dawnDesc.loggingCallbackInfo.callback = [](WGPULoggingType type, WGPUStringView msg, void*, void*) {
    Log("dawn log %d: %.*s\n", int(type), int(msg.length), msg.data);
  };
  wgpu::InstanceDescriptor instanceDesc{};
  instanceDesc.nextInChain = &dawnDesc;
  instanceDesc.requiredFeatureCount = 1;
  instanceDesc.requiredFeatures = &kTimedWaitAny;
  wgpu::Instance instance = wgpu::CreateInstance(&instanceDesc);
  if (!instance) {
    Log("CreateInstance failed\n");
    return false;
  }

  dawn::native::opengl::RequestAdapterOptionsGetGLProc glProc;
  glProc.getProc = EGLSyncShimGetProc;
  glProc.display = EGL_NO_DISPLAY;
  wgpu::RequestAdapterOptions adapterOpts{};
  adapterOpts.nextInChain = &glProc;
  adapterOpts.backendType = backend;
  adapterOpts.featureLevel = wgpu::FeatureLevel::Compatibility;  // Dawn GL adapters are compat-only

  wgpu::Adapter adapter;
  instance.WaitAny(
      instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::WaitAnyOnly,
                              [&](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView msg) {
                                if (status != wgpu::RequestAdapterStatus::Success) {
                                  Log("RequestAdapter: %.*s\n", int(msg.length), msg.data);
                                }
                                adapter = std::move(a);
                              }),
      UINT64_MAX);
  if (!adapter) {
    return false;
  }
  wgpu::AdapterInfo info;
  adapter.GetInfo(&info);
  Log("adapter: backend=%d vendor='%.*s' device='%.*s' desc='%.*s'\n", int(info.backendType),
      int(info.vendor.length), info.vendor.data, int(info.device.length), info.device.data,
      int(info.description.length), info.description.data);

  wgpu::DeviceDescriptor deviceDesc{};
  deviceDesc.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView msg) {
        Log("device error %d: %.*s\n", int(type), int(msg.length), msg.data);
      });
  deviceDesc.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView msg) {
        Log("device lost %d: %.*s\n", int(reason), int(msg.length), msg.data);
      });
  wgpu::Device device;
  instance.WaitAny(
      adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::WaitAnyOnly,
                            [&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView msg) {
                              if (status != wgpu::RequestDeviceStatus::Success) {
                                Log("RequestDevice: %.*s\n", int(msg.length), msg.data);
                              }
                              device = std::move(d);
                            }),
      UINT64_MAX);
  if (!device) {
    return false;
  }

  NWindow* win = nwindowGetDefault();
  wgpu::SurfaceSourceSwitchNWindow nwSrc;
  nwSrc.window = win;
  wgpu::SurfaceDescriptor surfDesc{};
  surfDesc.nextInChain = &nwSrc;
  wgpu::Surface surface = instance.CreateSurface(&surfDesc);
  wgpu::SurfaceCapabilities caps;
  if (!surface || surface.GetCapabilities(adapter, &caps) != wgpu::Status::Success || caps.formatCount == 0) {
    Log("surface creation/capabilities failed\n");
    return false;
  }
  const wgpu::TextureFormat format = caps.formats[0];
  wgpu::SurfaceConfiguration config{};
  config.device = device;
  config.format = format;
  config.width = 1280;
  config.height = 720;
  config.presentMode = wgpu::PresentMode::Fifo;
  surface.Configure(&config);
  Log("surface configured: format=%d\n", int(format));

  wgpu::ShaderSourceWGSL wgsl;
  wgsl.code = kShader;
  wgpu::ShaderModuleDescriptor smDesc{};
  smDesc.nextInChain = &wgsl;
  wgpu::ShaderModule sm = device.CreateShaderModule(&smDesc);

  wgpu::ColorTargetState target{};
  target.format = format;
  wgpu::FragmentState fragment{};
  fragment.module = sm;
  fragment.entryPoint = "fs";
  fragment.targetCount = 1;
  fragment.targets = &target;
  wgpu::RenderPipelineDescriptor rpDesc{};
  rpDesc.vertex.module = sm;
  rpDesc.vertex.entryPoint = "vs";
  rpDesc.fragment = &fragment;
  wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&rpDesc);

  wgpu::BufferDescriptor ubDesc{};
  ubDesc.size = 16;
  ubDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer ub = device.CreateBuffer(&ubDesc);
  wgpu::BindGroupEntry entry{};
  entry.binding = 0;
  entry.buffer = ub;
  entry.size = 16;
  wgpu::BindGroupDescriptor bgDesc{};
  bgDesc.layout = pipeline.GetBindGroupLayout(0);
  bgDesc.entryCount = 1;
  bgDesc.entries = &entry;
  wgpu::BindGroup bg = device.CreateBindGroup(&bgDesc);
  Log("pipeline ready\n");
  LogMem("pipeline");
  DumpFrame(instance, device, pipeline, bg, format);

  PadState pad;
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);

  const u64 start = armGetSystemTick();
  u64 last = start;
  u32 frame = 0;
  while (appletMainLoop()) {
    padUpdate(&pad);
    if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
      break;
    }
    const float t[4] = {float(armTicksToNs(armGetSystemTick() - start)) * 1e-9f, 0, 0, 0};
    device.GetQueue().WriteBuffer(ub, 0, t, sizeof(t));

    wgpu::SurfaceTexture st;
    surface.GetCurrentTexture(&st);
    if (st.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        st.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
      Log("GetCurrentTexture status %d\n", int(st.status));
      break;
    }
    if (frame == 120) {
      LogMem("before dump");
      DumpFrame(instance, device, pipeline, bg, format);
    }
    wgpu::RenderPassColorAttachment color{};
    color.view = st.texture.CreateView();
    color.loadOp = wgpu::LoadOp::Clear;
    color.storeOp = wgpu::StoreOp::Store;
    color.clearValue = {0.05, 0.05, 0.1 + 0.1 * (frame % 60) / 60.0, 1.0};
    wgpu::RenderPassDescriptor rp{};
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &color;
    wgpu::CommandEncoder enc = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bg);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer cb = enc.Finish();
    device.GetQueue().Submit(1, &cb);
    surface.Present();
    instance.ProcessEvents();

    ++frame;
    const u64 now = armGetSystemTick();
    if (armTicksToNs(now - last) >= 1000000000ull) {
      Log("frame %u: %.1f fps\n", frame, frame / (armTicksToNs(now - start) * 1e-9));
      last = now;
    }
  }
  Log("exit after %u frames\n", frame);
  return true;
}

int main(int, char**) {
  socketInitializeDefault();
  nxlinkStdio();
  consoleDebugInit(debugDevice_SVC);  // stderr (Dawn warnings) -> svcOutputDebugString
  gLog = fopen("sdmc:/dawn-nx.log", "w");
  if (!gLog) Log("fopen sdmc log failed errno=%d\n", errno);
  Log("dawn-nx spike start\n");
  LogMem("start");
  {
    EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj = 0, min = 0;
    eglInitialize(d, &maj, &min);
    Log("EGL %d.%d vendor='%s' apis='%s'\nEGL ext: %s\n", maj, min, eglQueryString(d, EGL_VENDOR),
        eglQueryString(d, EGL_CLIENT_APIS), eglQueryString(d, EGL_EXTENSIONS));
  }
  if (!Run(wgpu::BackendType::OpenGLES)) {
    Log("GLES failed, trying desktop GL\n");
    if (!Run(wgpu::BackendType::OpenGL)) {
      Log("both GL backends failed\n");
    }
  }
  if (gLog) {
    fclose(gLog);
  }
  socketExit();
  return 0;
}
