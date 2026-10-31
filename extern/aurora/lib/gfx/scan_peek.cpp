#include "scan_peek.hpp"

#include "../gx/gx.hpp"
#include "../webgpu/gpu.hpp"
#include "../webgpu/gpu_prof.hpp"

#include <fmt/format.h>
#include <magic_enum.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

namespace aurora::gfx::scan_peek {
namespace {
Module Log("aurora::gfx::scan_peek");

using webgpu::g_device;

constexpr size_t SlotCount = 2;
// WebGPU requires a 256-byte-multiple row stride for texture/buffer copies.
constexpr uint32_t RowStrideAlignment = 256;
constexpr uint32_t BytesPerTexel = 4; // the EFB is RGBA8/BGRA8, the depth buffer R32Float
// How often a completed peek reports its id histogram; the first one always does.
constexpr uint64_t DiagnoseEvery = 32;

enum class SlotState : uint8_t {
  Available,
  CopySubmitted,
  MapPending,
};

struct Slot {
  wgpu::Buffer colorBuffer;
  wgpu::Buffer depthBuffer;
  wgpu::Texture resolvedColor;
  wgpu::Texture resolvedDepth;
  wgpu::Buffer resolveParams;
  uint64_t bytes = 0;
  uint32_t rowBytes = 0;
  uint32_t rows = 0;
  Request req;
  uint64_t generation = 0;
  uint32_t mapsRemaining = 0;
  bool colorMapped = false;
  bool depthMapped = false;
  // PrimedGun: false when the request had no depth texture to read (encode): the snapshot's
  // depth is then zero rather than whatever the buffer holds.
  bool depthRead = true;
  SlotState state = SlotState::Available;
};

std::array<Slot, SlotCount> g_slots;
size_t g_nextSlot = 0;
std::mutex g_mutex;
std::atomic<uint64_t> g_nextGeneration{1};
std::atomic<uint64_t> g_writtenGeneration{0};
uint64_t g_peekCount = 0;
uint32_t g_skippedWarned = 0;
bool g_loggedFirstRequest = false;
bool g_loggedEyeRequest = false;
// [multisampled][compat]; a Core device uses the first column only.
std::array<std::array<wgpu::RenderPipeline, 2>, 2> g_resolvePipelines;
// Port (issue #24): the Compatibility variants below are kept apart from the Core shader so Core
// behaviour is untouched.
wgpu::Sampler g_depthSampler;

// Alpha is an integer scan ID, not opacity. A normal MSAA colour resolve averages IDs into
// unrelated IDs at silhouettes. Pick sample zero for BOTH ID and depth, as GX depth copies do.
// Resolve only the requested region into copyable single-sample textures; never alter the EFB.
constexpr std::string_view ResolveShader = R"(
@group(0) @binding(0) var color: texture_multisampled_2d<f32>;
@group(0) @binding(1) var depth: texture_depth_multisampled_2d;
@group(0) @binding(2) var<uniform> origin: vec2u;

@vertex fn vs_main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(corners[i], 0.0, 1.0);
}
struct Output {
    @location(0) color: vec4f,
    @location(1) depth: f32,
};
@fragment fn fs_main(@builtin(position) pos: vec4f) -> Output {
    let coord = vec2i(pos.xy) + vec2i(origin);
    return Output(textureLoad(color, coord, 0), textureLoad(depth, coord, 0));
}
)";

// Port (issue #24): Compatibility variants of the resolve. A Compatibility device cannot bind a
// multisampled depth texture, so the MSAA variant has no depth input and writes a fixed far-plane depth (FAR_DEPTH, patched to the
// raw far value for the depth convention), the 'no data' value the Splitter beam expects.
// It reads the already-resolved single-sample colour (the multisampled view holds no ids there),
// so ids at silhouettes are averaged; the 1x variant reads depth like the Core one.
constexpr std::string_view CompatResolveShaderNoDepth = R"(
@group(0) @binding(0) var color: texture_2d<f32>;
@group(0) @binding(2) var<uniform> origin: vec2u;

@vertex fn vs_main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(corners[i], 0.0, 1.0);
}
struct Output {
    @location(0) color: vec4f,
    @location(1) depth: f32,
};
@fragment fn fs_main(@builtin(position) pos: vec4f) -> Output {
    let coord = vec2i(pos.xy) + vec2i(origin);
    return Output(textureLoad(color, coord, 0), FAR_DEPTH);
}
)";

// Compatibility 1x: textureLoad and non-comparison samplers are both rejected on depth textures,
// so depth is recovered by bisecting with a LessEqual comparison sampler at the texel centre.
constexpr std::string_view CompatResolveShader1x = R"(
@group(0) @binding(0) var color: texture_2d<f32>;
@group(0) @binding(1) var depth: texture_depth_2d;
@group(0) @binding(2) var<uniform> origin: vec2u;
@group(0) @binding(3) var depthSampler: sampler_comparison;

@vertex fn vs_main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
    var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return vec4f(corners[i], 0.0, 1.0);
}
struct Output {
    @location(0) color: vec4f,
    @location(1) depth: f32,
};
@fragment fn fs_main(@builtin(position) pos: vec4f) -> Output {
    let coord = vec2i(pos.xy) + vec2i(origin);
    let uv = (vec2f(coord) + vec2f(0.5)) / vec2f(textureDimensions(depth));
    // Compat allows only comparison samplers on depth: bisect the texel (ref <= depth), 24 steps (z24 precision).
    var lo = 0.0;
    var hi = 1.0;
    for (var i = 0; i < 24; i++) {
        let mid = (lo + hi) * 0.5;
        if (textureSampleCompareLevel(depth, depthSampler, uv, mid) > 0.5) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return Output(textureLoad(color, coord, 0), lo);
}
)";

wgpu::RenderPipeline resolve_pipeline(bool multisampled, bool compat) {
  auto& pipeline = g_resolvePipelines[multisampled ? 1 : 0][compat ? 1 : 0];
  if (pipeline) {
    return pipeline;
  }
  std::string shader{compat ? (multisampled ? CompatResolveShaderNoDepth : CompatResolveShader1x)
                            : ResolveShader};
  if (compat && multisampled) {
    shader.replace(shader.find("FAR_DEPTH"), 9, gx::UseReversedZ ? "0.0" : "1.0");
  }
  if (!multisampled && !compat) {
    const std::string colorType = "texture_multisampled_2d<f32>";
    const std::string depthType = "texture_depth_multisampled_2d";
    shader.replace(shader.find(colorType), colorType.size(), "texture_2d<f32>");
    shader.replace(shader.find(depthType), depthType.size(), "texture_depth_2d");
  }
  const wgpu::ShaderSourceWGSL source{wgpu::ShaderSourceWGSL::Init{.code = shader.c_str()}};
  const wgpu::ShaderModuleDescriptor moduleDesc{.nextInChain = &source, .label = "Scan Peek Resolve"};
  const auto module = g_device.CreateShaderModule(&moduleDesc);
  const std::array targets{
      wgpu::ColorTargetState{.format = wgpu::TextureFormat::RGBA8Unorm},
      wgpu::ColorTargetState{.format = wgpu::TextureFormat::R32Float},
  };
  const wgpu::FragmentState fragment{
      .module = module, .entryPoint = "fs_main", .targetCount = targets.size(), .targets = targets.data()};
  const wgpu::RenderPipelineDescriptor descriptor{
      .label = "Scan Peek Resolve",
      .vertex = {.module = module, .entryPoint = "vs_main"},
      .fragment = &fragment,
  };
  pipeline = g_device.CreateRenderPipeline(&descriptor);
  return pipeline;
}

// PrimedGun: CompatResolveShaderNoDepth on any device, for an eye peek that has no depth to read.
wgpu::RenderPipeline g_resolveNoDepthPipeline;
wgpu::RenderPipeline resolve_pipeline_no_depth() {
  if (g_resolveNoDepthPipeline) {
    return g_resolveNoDepthPipeline;
  }
  std::string shader{CompatResolveShaderNoDepth};
  shader.replace(shader.find("FAR_DEPTH"), 9, gx::UseReversedZ ? "0.0" : "1.0");
  const wgpu::ShaderSourceWGSL source{wgpu::ShaderSourceWGSL::Init{.code = shader.c_str()}};
  const wgpu::ShaderModuleDescriptor moduleDesc{.nextInChain = &source, .label = "Scan Peek Resolve (no depth)"};
  const auto module = g_device.CreateShaderModule(&moduleDesc);
  const std::array targets{
      wgpu::ColorTargetState{.format = wgpu::TextureFormat::RGBA8Unorm},
      wgpu::ColorTargetState{.format = wgpu::TextureFormat::R32Float},
  };
  const wgpu::FragmentState fragment{
      .module = module, .entryPoint = "fs_main", .targetCount = targets.size(), .targets = targets.data()};
  const wgpu::RenderPipelineDescriptor descriptor{
      .label = "Scan Peek Resolve (no depth)",
      .vertex = {.module = module, .entryPoint = "vs_main"},
      .fragment = &fragment,
  };
  g_resolveNoDepthPipeline = g_device.CreateRenderPipeline(&descriptor);
  return g_resolveNoDepthPipeline;
}

void resolve_snapshot(const wgpu::CommandEncoder& cmd, Slot& slot, const wgpu::TextureView& color,
                      const wgpu::Texture& depth, const Request& req, bool multisampled, uint32_t layer) {
  // Port (issue #24): `compat` selects the Compatibility variants; the multisampled one has no depth.
  const bool compat = !webgpu::g_hasCoreFeatures;
  // PrimedGun: an eye peek without depth (a multisampled eye, encoding.cpp) takes the no-depth
  // variant too, over its single-sample colour.
  const bool noDepthInput = (compat && multisampled) || !depth;
  const auto pipeline = noDepthInput && !multisampled ? resolve_pipeline_no_depth() : resolve_pipeline(multisampled, compat);
  const uint32_t width = static_cast<uint32_t>(req.srcWidth);
  const uint32_t height = static_cast<uint32_t>(req.srcHeight);
  if (!slot.resolvedColor || slot.resolvedColor.GetWidth() != width || slot.resolvedColor.GetHeight() != height) {
    wgpu::TextureDescriptor descriptor{
        .label = "Scan Peek Resolved ID",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        .size = {width, height, 1},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };
    slot.resolvedColor = g_device.CreateTexture(&descriptor);
    descriptor.label = "Scan Peek Resolved Depth";
    descriptor.format = wgpu::TextureFormat::R32Float;
    slot.resolvedDepth = g_device.CreateTexture(&descriptor);
  }
  if (!slot.resolveParams) {
    const wgpu::BufferDescriptor descriptor{
        .label = "Scan Peek Resolve Origin",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = 8,
    };
    slot.resolveParams = g_device.CreateBuffer(&descriptor);
  }
  const std::array origin{static_cast<uint32_t>(req.srcX), static_cast<uint32_t>(req.srcY)};
  g_device.GetQueue().WriteBuffer(slot.resolveParams, 0, origin.data(), sizeof(origin));
  const wgpu::TextureViewDescriptor depthDescriptor{.dimension = wgpu::TextureViewDimension::e2D,
                                                    .baseArrayLayer = layer,
                                                    .arrayLayerCount = 1,
                                                    .aspect = wgpu::TextureAspect::DepthOnly};
  auto& nearest = g_depthSampler;
  if (compat && !multisampled && !nearest) {
    const wgpu::SamplerDescriptor samplerDescriptor{.label = "Scan Peek Depth",
                                                    .compare = wgpu::CompareFunction::LessEqual};
    nearest = g_device.CreateSampler(&samplerDescriptor);
  }
  std::array<wgpu::BindGroupEntry, 4> entries{
      wgpu::BindGroupEntry{.binding = 0, .textureView = color},
      wgpu::BindGroupEntry{.binding = 1},
      wgpu::BindGroupEntry{.binding = 2, .buffer = slot.resolveParams, .size = 8},
      wgpu::BindGroupEntry{.binding = 3, .sampler = nearest},
  };
  // [port] the no-depth shader declares no binding 1, so it is left out of the group
  if (noDepthInput) {
    entries[1] = entries[2];
  } else {
    entries[1].textureView = depth.CreateView(&depthDescriptor);
  }
  const wgpu::BindGroupDescriptor bindGroupDescriptor{
      .layout = pipeline.GetBindGroupLayout(0), .entryCount = noDepthInput ? 2u : (compat ? 4u : 3u), .entries = entries.data()};
  const auto bindGroup = g_device.CreateBindGroup(&bindGroupDescriptor);
  const std::array attachments{
      wgpu::RenderPassColorAttachment{.view = slot.resolvedColor.CreateView(),
                                      .loadOp = wgpu::LoadOp::Clear,
                                      .storeOp = wgpu::StoreOp::Store},
      wgpu::RenderPassColorAttachment{.view = slot.resolvedDepth.CreateView(),
                                      .loadOp = wgpu::LoadOp::Clear,
                                      .storeOp = wgpu::StoreOp::Store},
  };
  const wgpu::RenderPassDescriptor passDescriptor{
      .label = "Scan Peek Resolve",
      .colorAttachmentCount = attachments.size(),
      .colorAttachments = attachments.data()};
  const auto pass = cmd.BeginRenderPass(&passDescriptor);
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, bindGroup);
  pass.Draw(3);
  pass.End();
}

uint32_t align_up(uint32_t value, uint32_t alignment) noexcept {
  return (value + alignment - 1) & ~(alignment - 1);
}

// Sizes (and re-creates) the slot's read-back buffers for one request's source region, clipping
// the region to the textures it will be read from: it can run past the framebuffer when the
// game's viewport does, and a copy that overruns fails validation.
bool ensure_slot(Slot& slot, const wgpu::Texture& color, const wgpu::Texture& depth, Request& req) noexcept {
  const uint32_t textureWidth = depth ? std::min(color.GetWidth(), depth.GetWidth()) : color.GetWidth();
  const uint32_t textureHeight = depth ? std::min(color.GetHeight(), depth.GetHeight()) : color.GetHeight();
  if (textureWidth == 0 || textureHeight == 0) {
    return false;
  }

  const int32_t maxWidth = static_cast<int32_t>(textureWidth);
  const int32_t maxHeight = static_cast<int32_t>(textureHeight);
  const int32_t x = std::clamp(req.srcX, 0, maxWidth - 1);
  const int32_t y = std::clamp(req.srcY, 0, maxHeight - 1);
  req.srcX = x;
  req.srcY = y;
  req.srcWidth = std::clamp(req.srcWidth, 1, maxWidth - x);
  req.srcHeight = std::clamp(req.srcHeight, 1, maxHeight - y);

  const uint32_t rowBytes = align_up(static_cast<uint32_t>(req.srcWidth) * BytesPerTexel, RowStrideAlignment);
  const uint32_t rows = static_cast<uint32_t>(req.srcHeight);
  const uint64_t bytes = static_cast<uint64_t>(rowBytes) * rows;

  if (slot.colorBuffer && slot.depthBuffer && slot.rowBytes == rowBytes && slot.rows == rows) {
    return true;
  }

  slot.colorBuffer = {};
  slot.depthBuffer = {};
  slot.bytes = bytes;
  slot.rowBytes = rowBytes;
  slot.rows = rows;

  const wgpu::BufferDescriptor descriptor{
      .label = "Scan Peek Readback Buffer",
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = bytes,
  };
  slot.colorBuffer = g_device.CreateBuffer(&descriptor);
  slot.depthBuffer = g_device.CreateBuffer(&descriptor);
  return static_cast<bool>(slot.colorBuffer) && static_cast<bool>(slot.depthBuffer);
}

void write_snapshot(Slot& slot, const uint8_t* colorData, const float* depthData) noexcept {
  const Request& req = slot.req;
  if (req.sequenceOut == nullptr || req.idOut == nullptr || req.width == 0 || req.height == 0) {
    return;
  }

  // A late completion must not overwrite a snapshot that is already newer.
  if (slot.generation <= g_writtenGeneration.load(std::memory_order_acquire)) {
    return;
  }
  g_writtenGeneration.store(slot.generation, std::memory_order_release);

  auto* idOut = static_cast<uint8_t*>(req.idOut);
  auto* depthHighOut = static_cast<uint16_t*>(req.depthHighOut);
  auto* depthLowOut = static_cast<uint8_t*>(req.depthLowOut);
  const bool writeDepth = depthHighOut != nullptr && depthLowOut != nullptr;
  const bool depthRead = slot.depthRead;

  // Seqlock publish: the sequence is odd while the buffers are being written and even once a
  // whole snapshot is there, so a reader that samples it twice around its copy can tell.
  std::atomic_ref<uint32_t> sequence(*req.sequenceOut);
  const uint32_t current = sequence.load(std::memory_order_relaxed);
  sequence.store(current + 1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);

  uint32_t nonZeroIds = 0;
  uint32_t idHistogram[256] = {};
  const uint32_t sourceWidth = static_cast<uint32_t>(req.srcWidth);
  const uint32_t sourceHeight = static_cast<uint32_t>(req.srcHeight);
  for (uint32_t j = 0; j < req.height; ++j) {
    // Nearest-neighbour: the source region is the framebuffer-area the game's logical viewport
    // maps to, which only matches the requested pixels when no scaling is in play.
    const uint32_t sy = std::min(sourceHeight - 1, (2 * j + 1) * sourceHeight / (2 * req.height));
    const uint8_t* colorRow = colorData + static_cast<uint64_t>(sy) * slot.rowBytes;
    const float* depthRow = depthData + static_cast<uint64_t>(sy) * slot.rowBytes / sizeof(float);
    for (uint32_t i = 0; i < req.width; ++i) {
      const uint32_t sx = std::min(sourceWidth - 1, (2 * i + 1) * sourceWidth / (2 * req.width));
      const uint8_t id = colorRow[sx * BytesPerTexel + 3]; // alpha, whatever the channel order
      const uint32_t index = j * req.width + i;
      idOut[index] = id;
      idHistogram[id] += 1;
      if (id != 0) {
        ++nonZeroIds;
      }
      if (writeDepth) {
        // A console Z16 copy holds the top 16 bits of the 24-bit z; the game's own reader
        // byte-swaps that half, so the high byte goes first here. Z8L is the bottom 8 bits.
        const float sampled = depthRow[sx];
        const float normalized = std::clamp(gx::UseReversedZ ? 1.f - sampled : sampled, 0.f, 1.f);
        // [port #24] float32 rounds 16777215.5 up to 2^24, which wrapped far-plane depth to 0
        // PrimedGun: an eye peek without depth (encoding.cpp) reads ids only; its depth stays zero.
        const uint32_t z24 =
            depthRead ? std::min(static_cast<uint32_t>(normalized * 16777215.f + 0.5f), 0xFFFFFFu) : 0u;
        const uint32_t high = z24 >> 8;
        depthHighOut[index] = static_cast<uint16_t>(((high >> 8) & 0xff) | ((high & 0xff) << 8));
        depthLowOut[index] = static_cast<uint8_t>(z24 & 0xff);
      }
    }
  }

  std::atomic_thread_fence(std::memory_order_release);
  sequence.store(current + 2, std::memory_order_release);

  const uint64_t count = ++g_peekCount;
  if (!req.depthOnly && (count == 1 || count % DiagnoseEvery == 0)) {
    if (nonZeroIds == 0) {
      Log.warn("EFB alpha peek {}: all {}x{} pixels are zero, so no scan id reaches the framebuffer",
               count, req.width, req.height);
    } else {
      uint32_t distinct = 0;
      uint32_t bestValue = 0;
      uint32_t bestCount = 0;
      for (uint32_t value = 1; value < 256; ++value) {
        if (idHistogram[value] != 0) {
          ++distinct;
        }
        if (idHistogram[value] > bestCount) {
          bestCount = idHistogram[value];
          bestValue = value;
        }
      }
      Log.info("EFB alpha peek {}: {}/{} pixels non-zero, {} distinct values, most common {} ({} pixels)",
               count, nonZeroIds, req.width * req.height, distinct, bestValue, bestCount);
    }
  }
}

void complete_map(size_t slotIdx, bool isColor, wgpu::MapAsyncStatus status, wgpu::StringView message) noexcept {
  const uint8_t* colorData = nullptr;
  const float* depthData = nullptr;
  bool ready = false;
  {
    std::lock_guard lock{g_mutex};
    auto& slot = g_slots[slotIdx];
    if (slot.state != SlotState::MapPending || slot.mapsRemaining == 0) {
      return;
    }
    if (status == wgpu::MapAsyncStatus::Success) {
      (isColor ? slot.colorMapped : slot.depthMapped) = true;
    } else if (status != wgpu::MapAsyncStatus::CallbackCancelled && status != wgpu::MapAsyncStatus::Aborted) {
      Log.warn("Scan peek readback mapping failed: {}", message);
    }
    if (--slot.mapsRemaining != 0) {
      return;
    }
    if (slot.colorMapped && slot.depthMapped) {
      colorData = static_cast<const uint8_t*>(
          slot.colorBuffer.GetConstMappedRange(0, static_cast<size_t>(slot.bytes)));
      depthData = static_cast<const float*>(
          slot.depthBuffer.GetConstMappedRange(0, static_cast<size_t>(slot.bytes)));
      ready = colorData != nullptr && depthData != nullptr;
    }
  }

  if (ready) {
    write_snapshot(g_slots[slotIdx], colorData, depthData);
  }

  {
    std::lock_guard lock{g_mutex};
    auto& slot = g_slots[slotIdx];
    if (slot.colorBuffer) {
      slot.colorBuffer.Unmap();
    }
    if (slot.depthBuffer) {
      slot.depthBuffer.Unmap();
    }
    slot.colorMapped = false;
    slot.depthMapped = false;
    slot.state = SlotState::Available;
  }
}
} // namespace

void encode(const wgpu::CommandEncoder& cmd, const wgpu::Texture& color, const wgpu::Texture& depth,
            uint32_t msaaSamples, const Request& request, const wgpu::TextureView& multisampledColor,
            uint32_t layer) noexcept {
  if (request.sequenceOut == nullptr || request.idOut == nullptr || request.width == 0 ||
      request.height == 0) {
    return;
  }
  // [port #24] Compatibility devices now resolve too (see resolve_snapshot); only a missing EFB
  // colour view skips.
  if (msaaSamples > 1 && !multisampledColor) {
    if (g_skippedWarned++ == 0) {
      Log.warn("Scan peek skipped: multisampled readback requires an EFB color view");
    }
    return;
  }
  // PrimedGun: an eye's peek may come without depth (encoding.cpp: a multisampled eye).
  if (!color || (!depth && request.eye < 0)) {
    if (g_skippedWarned++ == 0) {
      Log.warn("Scan peek skipped: no framebuffer color/depth texture to read");
    }
    return;
  }

  Request req = request;
  if (req.eye >= 0) {
    // The eye's own region, in place of the EFB's.
    req.srcX = req.eyeX;
    req.srcY = req.eyeY;
    req.srcWidth = req.eyeWidth;
    req.srcHeight = req.eyeHeight;
  }
  size_t slotIdx = 0;
  {
    std::lock_guard lock{g_mutex};
    Slot* slot = nullptr;
    for (size_t i = 0; i < g_slots.size(); ++i) {
      const size_t idx = (g_nextSlot + i) % g_slots.size();
      if (g_slots[idx].state == SlotState::Available) {
        slot = &g_slots[idx];
        slotIdx = idx;
        g_nextSlot = (idx + 1) % g_slots.size();
        break;
      }
    }
    if (slot == nullptr || !ensure_slot(*slot, color, depth, req)) {
      if (g_skippedWarned++ == 0) {
        Log.warn("Scan peek skipped: no read-back slot is free");
      }
      return;
    }
    slot->state = SlotState::CopySubmitted;
    slot->req = req;
    slot->generation = g_nextGeneration.fetch_add(1);
    slot->mapsRemaining = 0;
    slot->colorMapped = false;
    slot->depthMapped = false;
    slot->depthRead = static_cast<bool>(depth);

    if (!g_loggedFirstRequest || (req.eye >= 0) != g_loggedEyeRequest) {
      g_loggedFirstRequest = true;
      g_loggedEyeRequest = req.eye >= 0;
      Log.info("Scan peek: {}x{} out of ({},{}) {}x{} of a {}x{} {} (color {}, depth {})", req.width, req.height,
               req.srcX, req.srcY, req.srcWidth, req.srcHeight, color.GetWidth(), color.GetHeight(),
               req.eye >= 0 ? fmt::format("eye {} image", req.eye) : std::string("framebuffer"),
               magic_enum::enum_name(color.GetFormat()),
               depth ? magic_enum::enum_name(depth.GetFormat()) : std::string_view("none"));
    }
  }

  auto& slot = g_slots[slotIdx];
  const bool multisampled = msaaSamples > 1;
  // Even at 1x, WebGPU only permits whole-subresource copies from depth formats. Convert the
  // requested depth region to R32Float before copying it; this also handles HDR colour safely.
  // [port #24] Always resolved: the old Compatibility path copied a partial depth region straight
  // out of the depth texture, which WebGPU rejects.
  const bool resolved = true;
  // PrimedGun: an eye's image is a layer of the eye array (multiview); the EFB is layer 0.
  const wgpu::TextureViewDescriptor colorLayer{
      .dimension = wgpu::TextureViewDimension::e2D, .baseArrayLayer = layer, .arrayLayerCount = 1};
  resolve_snapshot(cmd, slot,
                   (multisampled && webgpu::g_hasCoreFeatures) ? multisampledColor : color.CreateView(&colorLayer),
                   depth, req, multisampled, layer);
  const wgpu::Extent3D copySize{
      .width = static_cast<uint32_t>(req.srcWidth),
      .height = static_cast<uint32_t>(req.srcHeight),
      .depthOrArrayLayers = 1,
  };
  const wgpu::Origin3D origin{
      .x = resolved ? 0u : static_cast<uint32_t>(req.srcX),
      .y = resolved ? 0u : static_cast<uint32_t>(req.srcY),
      .z = 0,
  };
  {
    const wgpu::TexelCopyTextureInfo source{
        .texture = resolved ? slot.resolvedColor : color,
        .mipLevel = 0,
        .origin = origin,
        .aspect = wgpu::TextureAspect::All,
    };
    const wgpu::TexelCopyBufferInfo destination{
        .layout =
            wgpu::TexelCopyBufferLayout{
                .offset = 0,
                .bytesPerRow = slot.rowBytes,
                .rowsPerImage = slot.rows,
            },
        .buffer = slot.colorBuffer,
    };
    const webgpu::gpu_prof::Zone zone{cmd, "Scan peek color"};
    cmd.CopyTextureToBuffer(&source, &destination, &copySize);
  }
  if (depth) {
    const wgpu::TexelCopyTextureInfo source{
        .texture = resolved ? slot.resolvedDepth : depth,
        .mipLevel = 0,
        .origin = origin,
        .aspect = resolved ? wgpu::TextureAspect::All : wgpu::TextureAspect::DepthOnly,
    };
    const wgpu::TexelCopyBufferInfo destination{
        .layout =
            wgpu::TexelCopyBufferLayout{
                .offset = 0,
                .bytesPerRow = slot.rowBytes,
                .rowsPerImage = slot.rows,
            },
        .buffer = slot.depthBuffer,
    };
    const webgpu::gpu_prof::Zone zone{cmd, "Scan peek depth"};
    cmd.CopyTextureToBuffer(&source, &destination, &copySize);
  }
}

void after_submit() noexcept {
  std::vector<size_t> pending;
  {
    std::lock_guard lock{g_mutex};
    for (size_t i = 0; i < g_slots.size(); ++i) {
      auto& slot = g_slots[i];
      if (slot.state != SlotState::CopySubmitted) {
        continue;
      }
      slot.state = SlotState::MapPending;
      slot.mapsRemaining = 2;
      slot.colorMapped = false;
      slot.depthMapped = false;
      pending.push_back(i);
    }
  }

  for (const size_t slotIdx : pending) {
    wgpu::Buffer colorBuffer;
    wgpu::Buffer depthBuffer;
    uint64_t bytes = 0;
    {
      std::lock_guard lock{g_mutex};
      colorBuffer = g_slots[slotIdx].colorBuffer;
      depthBuffer = g_slots[slotIdx].depthBuffer;
      bytes = g_slots[slotIdx].bytes;
    }
    colorBuffer.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::AllowSpontaneous,
                         [slotIdx](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                           complete_map(slotIdx, true, status, message);
                         });
    depthBuffer.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::AllowSpontaneous,
                         [slotIdx](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                           complete_map(slotIdx, false, status, message);
                         });
  }
}

void shutdown() noexcept {
  g_resolvePipelines = {};
  g_resolveNoDepthPipeline = {};
  g_depthSampler = {};
  // The buffers are released after the lock: releasing one with a map still pending runs
  // complete_map at once, which takes g_mutex (it then sees an Available slot and returns).
  std::array<Slot, SlotCount> released;
  {
    std::lock_guard lock{g_mutex};
    for (size_t i = 0; i < g_slots.size(); ++i) {
      released[i] = std::move(g_slots[i]);
      g_slots[i] = {};
    }
    g_loggedFirstRequest = false;
  }
}
} // namespace aurora::gfx::scan_peek
