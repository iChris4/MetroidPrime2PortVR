#include <gtest/gtest.h>

#include "gfx/frame_packet.hpp"
#include "gfx/recording.hpp"
#include "gfx/scan_peek.hpp"
#include "gfx/texture.hpp"
#include "gx/gx.hpp"
#include "webgpu/gpu.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Real Vulkan rendering and CPU readback, not the Null backend: an averaged ID, mismatched
// depth sample, dropped MSAA request or invalid texture copy must fail these tests.
namespace aurora::gfx {
namespace {
class ScanPeekMsaaTest : public ::testing::Test {
protected:
  static inline std::vector<std::string> errors;

  static void SetUpTestSuite() {
    const std::array features{wgpu::InstanceFeatureName::TimedWaitAny};
    const wgpu::InstanceDescriptor descriptor{
        .requiredFeatureCount = features.size(), .requiredFeatures = features.data()};
    webgpu::g_instance = wgpu::CreateInstance(&descriptor);
    ASSERT_TRUE(webgpu::g_instance);
    const wgpu::RequestAdapterOptions options{
        .featureLevel = wgpu::FeatureLevel::Core, .backendType = wgpu::BackendType::Vulkan};
    wgpu::Adapter adapter;
    webgpu::g_instance.WaitAny(webgpu::g_instance.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter result, wgpu::StringView) {
          if (status == wgpu::RequestAdapterStatus::Success) adapter = std::move(result);
        }), 5000000000);
    if (!adapter) return;
    wgpu::DeviceDescriptor deviceDescriptor{};
    deviceDescriptor.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message) {
          errors.emplace_back(std::string_view{message});
        });
    webgpu::g_instance.WaitAny(adapter.RequestDevice(
        &deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView) {
          if (status == wgpu::RequestDeviceStatus::Success) webgpu::g_device = std::move(device);
        }), 5000000000);
  }

  static void TearDownTestSuite() {
    webgpu::g_device = {};
    webgpu::g_instance = {};
  }

  void SetUp() override {
    if (!webgpu::g_device) GTEST_SKIP() << "Vulkan device unavailable; real readback not tested";
    errors.clear();
    webgpu::g_hasCoreFeatures = true;
  }

  void TearDown() override {
    scan_peek::shutdown();
    webgpu::g_hasCoreFeatures = false;
    for (const auto& error : errors) ADD_FAILURE() << error;
  }

  void check_snapshot(uint32_t samples, wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm,
                      int x = 3, int y = 2, int width = 5, int height = 3) {
    auto device = webgpu::g_device;
    device.PushErrorScope(wgpu::ErrorFilter::Validation);
    wgpu::TextureDescriptor descriptor{
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .size = {16, 8, 1}, .format = format, .sampleCount = samples};
    if (samples == 1) descriptor.usage |= wgpu::TextureUsage::CopySrc;
    const auto color = device.CreateTexture(&descriptor);
    descriptor.format = wgpu::TextureFormat::Depth32Float;
    const auto depth = device.CreateTexture(&descriptor);
    descriptor.format = format;
    descriptor.sampleCount = 1;
    descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    const auto resolved = samples > 1 ? device.CreateTexture(&descriptor) : color;

    // Every sample has a DIFFERENT target ID and depth. Sample zero is ID 37, z=0.2;
    // a normal colour resolve would produce ID 103 instead, which is not any input ID.
    std::string shader = R"(
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(corners[i], 0.0, 1.0);
}
struct Output { @location(0) color: vec4f, @builtin(frag_depth) depth: f32 };
)";
    shader += samples > 1 ? "@fragment fn fs(@builtin(position) pos: vec4f, @builtin(sample_index) i: u32) -> Output {\n"
                         : "@fragment fn fs(@builtin(position) pos: vec4f) -> Output { let i = 0u;\n";
    shader += R"(
  let pattern = u32(pos.x) % 3u + (u32(pos.y) % 2u) * 3u;
  return Output(vec4f(0.0, 0.0, 0.0, f32(37u + pattern + 44u * i) / 255.0),
                0.2 + 0.01 * f32(pattern) + 0.1 * f32(i));
})";
    const wgpu::ShaderSourceWGSL source{wgpu::ShaderSourceWGSL::Init{.code = shader.c_str()}};
    const wgpu::ShaderModuleDescriptor moduleDescriptor{.nextInChain = &source};
    const auto module = device.CreateShaderModule(&moduleDescriptor);
    const wgpu::ColorTargetState target{.format = format};
    const wgpu::FragmentState fragment{
        .module = module, .entryPoint = "fs", .targetCount = 1, .targets = &target};
    const wgpu::DepthStencilState depthState{
        .format = wgpu::TextureFormat::Depth32Float, .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::Always};
    const wgpu::RenderPipelineDescriptor pipelineDescriptor{
        .vertex = {.module = module, .entryPoint = "vs"}, .depthStencil = &depthState,
        .multisample = {.count = samples}, .fragment = &fragment};
    const auto pipeline = device.CreateRenderPipeline(&pipelineDescriptor);
    const auto cmd = device.CreateCommandEncoder();
    const wgpu::RenderPassColorAttachment attachment{
        .view = color.CreateView(), .resolveTarget = samples > 1 ? resolved.CreateView() : nullptr,
        .loadOp = wgpu::LoadOp::Clear, .storeOp = wgpu::StoreOp::Store};
    const wgpu::RenderPassDepthStencilAttachment depthAttachment{
        .view = depth.CreateView(), .depthLoadOp = wgpu::LoadOp::Clear,
        .depthStoreOp = wgpu::StoreOp::Store, .depthClearValue = 1.0f};
    const wgpu::RenderPassDescriptor passDescriptor{
        .colorAttachmentCount = 1, .colorAttachments = &attachment, .depthStencilAttachment = &depthAttachment};
    const auto pass = cmd.BeginRenderPass(&passDescriptor);
    pass.SetPipeline(pipeline);
    pass.Draw(3);
    pass.End();

    std::array<uint8_t, 15> ids{};
    std::array<uint16_t, 15> high{};
    std::array<uint8_t, 15> low{};
    uint32_t sequence = 0;
    const scan_peek::Request request{
        .idOut = ids.data(), .depthHighOut = high.data(), .depthLowOut = low.data(),
        .sequenceOut = &sequence, .srcX = x, .srcY = y, .srcWidth = width, .srcHeight = height,
        .width = 5, .height = 3};
    scan_peek::encode(cmd, resolved, depth, samples, request, color.CreateView());
    auto buffer = cmd.Finish();
    device.GetQueue().Submit(1, &buffer);
    scan_peek::after_submit();
    std::atomic_ref<uint32_t> completed(sequence);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (completed.load(std::memory_order_acquire) < 2 && std::chrono::steady_clock::now() < deadline) {
      webgpu::g_instance.ProcessEvents();
      std::this_thread::yield();
    }
    // On failure cancel pending maps before caller-owned arrays leave scope. Successful calls
    // keep slots and pipelines alive so ToggleAndClip also exercises reuse across AA changes.
    if (completed.load(std::memory_order_acquire) < 2) scan_peek::shutdown();
    EXPECT_EQ(completed.load(std::memory_order_acquire), 2u) << "snapshot was dropped or never mapped";
    for (size_t i = 0; i < ids.size(); ++i) {
      const int clippedWidth = std::min(width, 16 - x);
      const int clippedHeight = std::min(height, 8 - y);
      const int sx = x + std::min(clippedWidth - 1, (2 * static_cast<int>(i % 5) + 1) * clippedWidth / 10);
      const int sy = y + std::min(clippedHeight - 1, (2 * static_cast<int>(i / 5) + 1) * clippedHeight / 6);
      const int pattern = sx % 3 + (sy % 2) * 3;
      EXPECT_EQ(ids[i], 37 + pattern) << "pixel " << i << ": target IDs must not be averaged";
      const uint32_t h = ((high[i] & 0xffu) << 8) | (high[i] >> 8);
      const uint32_t z = (h << 8) | low[i];
      const float sampleDepth = 0.2f + 0.01f * static_cast<float>(pattern);
      const uint32_t expected = static_cast<uint32_t>(
          (gx::UseReversedZ ? 1.f - sampleDepth : sampleDepth) * 16777215.f + 0.5f);
      EXPECT_NEAR(z, expected, 1) << "pixel " << i << ": ID and depth must use the same sample";
    }
    std::string validation;
    webgpu::g_instance.WaitAny(device.PopErrorScope(
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::PopErrorScopeStatus, wgpu::ErrorType type, wgpu::StringView message) {
          if (type != wgpu::ErrorType::NoError) validation = std::string_view{message};
        }), 5000000000);
    EXPECT_TRUE(validation.empty()) << validation;
  }
};

TEST_F(ScanPeekMsaaTest, SingleSampleControl) { check_snapshot(1); }
TEST_F(ScanPeekMsaaTest, FourSamplesPreserveIdAndPairedDepth) { check_snapshot(4); }
TEST_F(ScanPeekMsaaTest, FourSamplesBgra) { check_snapshot(4, wgpu::TextureFormat::BGRA8Unorm); }
TEST_F(ScanPeekMsaaTest, FourSamplesHdr) { check_snapshot(4, wgpu::TextureFormat::RGBA16Float); }
TEST_F(ScanPeekMsaaTest, SingleSampleHdr) { check_snapshot(1, wgpu::TextureFormat::RGBA16Float); }
TEST_F(ScanPeekMsaaTest, ToggleAndClipAtFramebufferEdge) {
  check_snapshot(4);
  check_snapshot(1);
  check_snapshot(4, wgpu::TextureFormat::RGBA8Unorm, 14, 7, 5, 3);
}

TEST_F(ScanPeekMsaaTest, ScanPeekRetainsMsaaStoresBeforeFullClear) {
  const auto savedConfig = webgpu::g_graphicsConfig;
  const auto savedSize = webgpu::g_frameBuffer.size;
  webgpu::g_graphicsConfig.surfaceConfiguration.format = wgpu::TextureFormat::RGBA8Unorm;
  webgpu::g_graphicsConfig.depthFormat = wgpu::TextureFormat::Depth32Float;
  webgpu::g_graphicsConfig.msaaSamples = 4;
  webgpu::g_frameBuffer.size = {16, 8, 1};
  detail::FramePacket frame;
  detail::testing::suppress_render_worker(true);
  detail::begin_recording(frame, 0);
  const wgpu::TextureDescriptor descriptor{
      .usage = wgpu::TextureUsage::RenderAttachment, .size = {16, 8, 1},
      .format = wgpu::TextureFormat::RGBA8Unorm};
  const auto view = webgpu::g_device.CreateTexture(&descriptor).CreateView();
  auto& pass = frame.renderPasses.back();
  pass.colorAttachments[SceneColorAttachmentIndex].resolveView = view;
  pass.scanPeek.emplace();
  auto target = std::make_shared<TextureRef>(wgpu::Texture{}, wgpu::TextureView{}, wgpu::TextureView{},
      wgpu::Extent3D{16, 8, 1}, wgpu::TextureFormat::RGBA8Unorm, 1, GX_TF_RGBA8);
  resolve_pass_into(std::move(target), {0, 0, 16, 8}, true, true, true, {}, 1.f);
  EXPECT_EQ(frame.renderPasses.front().colorAttachments[SceneColorAttachmentIndex].storeOp, wgpu::StoreOp::Store);
  EXPECT_EQ(frame.renderPasses.front().depthStoreOp, wgpu::StoreOp::Store);
  finish();
  detail::end_recording();
  detail::shutdown_recording();
  webgpu::g_graphicsConfig = savedConfig;
  webgpu::g_frameBuffer.size = savedSize;
}
} // namespace
} // namespace aurora::gfx
