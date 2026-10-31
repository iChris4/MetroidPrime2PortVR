#include "efb_copy.hpp"

#include "../gx/gx.hpp"
#include "../webgpu/gpu.hpp"
#include "../webgpu/gpu_prof.hpp"

#include <magic_enum.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

namespace aurora::gfx::efb_copy {
namespace {
Module Log("aurora::gfx::efb_copy");

using webgpu::g_device;

constexpr size_t SlotCount = 2;
// WebGPU requires a 256-byte-multiple row stride for texture/buffer copies.
constexpr uint32_t RowStrideAlignment = 256;
// Every resolve target this module reads is an 8-bit-per-channel RGBA texture: the convert paths
// render into RGBA8Unorm, and the surface format is BGRA8Unorm or RGBA8Unorm.
constexpr uint32_t SourceBytesPerTexel = 4;
// How often a completed copy reports itself; the first one always does.
constexpr uint64_t DiagnoseEvery = 64;

enum class SlotState : uint8_t {
  Available,
  CopySubmitted,
  MapPending,
};

struct Slot {
  wgpu::Buffer buffer;
  uint64_t capacity = 0; // the buffer's size; it only grows
  uint64_t bytes = 0;
  uint32_t rowBytes = 0;
  uint32_t srcWidth = 0;
  uint32_t srcHeight = 0;
  bool sourceIsBgra = false;
  Request req;
  SlotState state = SlotState::Available;
  bool mapped = false;
};

std::array<Slot, SlotCount> g_slots;
size_t g_nextSlot = 0;
std::mutex g_mutex;
uint64_t g_copyCount = 0;
uint32_t g_warned = 0;
bool g_loggedFirst = false;

uint32_t align_up(uint32_t value, uint32_t alignment) noexcept {
  return (value + alignment - 1) & ~(alignment - 1);
}

// Sizes (and re-creates) the slot's staging buffer for one request's resolve target.
bool ensure_slot(Slot& slot, const TextureHandle& source, const Request& req) noexcept {
  if (!source || source->size.width == 0 || source->size.height == 0) {
    return false;
  }
  const auto format = source->format;
  if (format != wgpu::TextureFormat::RGBA8Unorm && format != wgpu::TextureFormat::BGRA8Unorm) {
    if (g_warned++ == 0) {
      Log.warn("EFB copy read-back skipped: the resolve target is {} and only RGBA8/BGRA8 copies are read",
               magic_enum::enum_name(format));
    }
    return false;
  }

  const uint32_t srcWidth = source->size.width;
  const uint32_t srcHeight = source->size.height;
  const uint32_t rowBytes = align_up(srcWidth * SourceBytesPerTexel, RowStrideAlignment);
  const uint64_t bytes = static_cast<uint64_t>(rowBytes) * srcHeight;
  // Port: copies alternate between resolve targets of different sizes (the scan visor reads the
  // full framebuffer and two pane-sized ones), so re-creating on every size change made a new
  // buffer of up to ~24 MB on most copies; reuse any buffer that is big enough.
  if (slot.buffer && slot.capacity >= bytes) {
    slot.bytes = bytes;
    slot.rowBytes = rowBytes;
    slot.srcWidth = srcWidth;
    slot.srcHeight = srcHeight;
    slot.sourceIsBgra = format == wgpu::TextureFormat::BGRA8Unorm;
    return true;
  }

  slot.buffer = {};
  slot.capacity = bytes;
  slot.bytes = bytes;
  slot.rowBytes = rowBytes;
  slot.srcWidth = srcWidth;
  slot.srcHeight = srcHeight;
  slot.sourceIsBgra = format == wgpu::TextureFormat::BGRA8Unorm;

  const wgpu::BufferDescriptor descriptor{
      .label = "EFB Copy Readback Buffer",
      .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
      .size = bytes,
  };
  slot.buffer = g_device.CreateBuffer(&descriptor);
  return static_cast<bool>(slot.buffer);
}

void write_snapshot(Slot& slot, const uint8_t* data) noexcept {
  const Request& req = slot.req;
  const uint32_t texelBytes = bytes_per_texel(req.format);
  if (req.out == nullptr || req.sequenceOut == nullptr || req.width == 0 || req.height == 0 ||
      texelBytes == 0) {
    return;
  }

  // Seqlock publish: the sequence is odd while the bytes are being written and even once a whole
  // copy is there, so a reader that samples it twice around its copy can tell.
  std::atomic_ref<uint32_t> sequence(*req.sequenceOut);
  const uint32_t current = sequence.load(std::memory_order_relaxed);
  sequence.store(current + 1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);

  auto* out = static_cast<uint8_t*>(req.out);
  const uint32_t srcWidth = slot.srcWidth;
  const uint32_t srcHeight = slot.srcHeight;
  uint32_t nonZero = 0;
  for (uint32_t y = 0; y < req.height; ++y) {
    // Nearest-neighbour: the resolve target is the framebuffer area the game's logical copy maps
    // to, scaled up when the window is larger than the console's framebuffer.
    const uint32_t sy = std::min(srcHeight - 1, (2 * y + 1) * srcHeight / (2 * req.height));
    const uint8_t* row = data + static_cast<uint64_t>(sy) * slot.rowBytes;
    uint8_t* dst = out + static_cast<uint64_t>(y) * req.width * texelBytes;
    for (uint32_t x = 0; x < req.width; ++x) {
      const uint32_t sx = std::min(srcWidth - 1, (2 * x + 1) * srcWidth / (2 * req.width));
      const uint8_t* texel = row + static_cast<uint64_t>(sx) * SourceBytesPerTexel;
      const uint8_t b = slot.sourceIsBgra ? texel[0] : texel[2];
      const uint8_t g = texel[1];
      const uint8_t r = slot.sourceIsBgra ? texel[2] : texel[0];
      const uint8_t a = texel[3];
      switch (req.format) {
      case GX_CTF_A8:
        dst[x] = a;
        break;
      case GX_CTF_R8:
        dst[x] = r;
        break;
      case GX_CTF_G8:
        dst[x] = g;
        break;
      case GX_CTF_B8:
        dst[x] = b;
        break;
      case GX_TF_I8: {
        // The console's intensity is the BT.601 luma its copy converter uses.
        const float intensity =
            std::min(1.f, 0.257f * (r / 255.f) + 0.504f * (g / 255.f) + 0.098f * (b / 255.f) + 16.f / 255.f);
        dst[x] = static_cast<uint8_t>(intensity * 255.f + 0.5f);
        break;
      }
      case GX_TF_RGB565: {
        // The console writes copy texels big-endian, and the game uploads these bytes as console
        // data, so the high byte goes first.
        const uint16_t value = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        dst[x * 2] = static_cast<uint8_t>(value >> 8);
        dst[x * 2 + 1] = static_cast<uint8_t>(value & 0xff);
        break;
      }
      default:
        break;
      }
      if (a != 0) {
        ++nonZero;
      }
    }
  }

  std::atomic_thread_fence(std::memory_order_release);
  sequence.store(current + 2, std::memory_order_release);

  const uint64_t count = ++g_copyCount;
  if (count == 1 || count % DiagnoseEvery == 0) {
    Log.info("EFB copy read-back {}: {}x{} fmt={} from a {}x{} {} resolve target, {}/{} alpha non-zero", count,
             req.width, req.height, static_cast<int>(req.format), srcWidth, srcHeight,
             slot.sourceIsBgra ? "BGRA8" : "RGBA8", nonZero, req.width * req.height);
  }
}

void complete_map(size_t slotIdx, wgpu::MapAsyncStatus status, wgpu::StringView message) noexcept {
  const uint8_t* data = nullptr;
  {
    std::lock_guard lock{g_mutex};
    auto& slot = g_slots[slotIdx];
    if (slot.state != SlotState::MapPending) {
      return;
    }
    if (status != wgpu::MapAsyncStatus::Success) {
      slot.state = SlotState::Available;
      if (status != wgpu::MapAsyncStatus::CallbackCancelled && status != wgpu::MapAsyncStatus::Aborted) {
        Log.warn("EFB copy read-back mapping failed: {}", message);
      }
      return;
    }
    // Port: the slot stays MapPending until it is unmapped below. Freeing it here let encode()
    // pick it and after_submit() MapAsync the still-mapped buffer ("is already mapped", fatal;
    // issue #29, after the Dark Alpha Splinter's death cutscene).
    slot.mapped = true;
    data = static_cast<const uint8_t*>(slot.buffer.GetConstMappedRange(0, static_cast<size_t>(slot.bytes)));
  }

  if (data != nullptr) {
    write_snapshot(g_slots[slotIdx], data);
  }

  std::lock_guard lock{g_mutex};
  auto& slot = g_slots[slotIdx];
  if (slot.mapped) {
    slot.buffer.Unmap();
    slot.mapped = false;
  }
  slot.state = SlotState::Available;
}
} // namespace

bool has_cpu_format(GXTexFmt format) noexcept {
  switch (format) {
  case GX_CTF_A8:
  case GX_CTF_R8:
  case GX_CTF_G8:
  case GX_CTF_B8:
  case GX_TF_I8:
  case GX_TF_RGB565:
    return true;
  default:
    return false;
  }
}

uint32_t bytes_per_texel(GXTexFmt format) noexcept {
  switch (format) {
  case GX_CTF_A8:
  case GX_CTF_R8:
  case GX_CTF_G8:
  case GX_CTF_B8:
  case GX_TF_I8:
    return 1;
  case GX_TF_RGB565:
    return 2;
  default:
    return 0;
  }
}

void encode(const wgpu::CommandEncoder& cmd, const TextureHandle& source, const Request& request) noexcept {
  if (request.out == nullptr || request.sequenceOut == nullptr || request.width == 0 ||
      request.height == 0) {
    return;
  }
  const uint32_t texelBytes = bytes_per_texel(request.format);
  const uint64_t needed = static_cast<uint64_t>(request.width) * request.height * texelBytes;
  if (texelBytes == 0 || needed > request.capacity) {
    if (g_warned++ == 0) {
      // GXTexFmt is not a magic_enum-friendly enum (its values run past 0x30), so log the raw value.
      Log.warn("EFB copy read-back skipped: {}x{} fmt={} needs {} bytes of staging, the caller offers {}",
               request.width, request.height, static_cast<int>(request.format), needed,
               request.capacity);
    }
    return;
  }
  if (!source) {
    return;
  }

  Request req = request;
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
    if (slot == nullptr || !ensure_slot(*slot, source, req)) {
      return;
    }
    slot->state = SlotState::CopySubmitted;
    slot->req = req;
    slot->mapped = false;

    if (!g_loggedFirst) {
      g_loggedFirst = true;
      Log.info("EFB copy read-back: {}x{} fmt={} of a {}x{} {} resolve target (to main memory)",
               req.width, req.height, static_cast<int>(req.format), source->size.width,
               source->size.height, magic_enum::enum_name(source->format));
    }
  }

  const Slot& slot = g_slots[slotIdx];
  const wgpu::TexelCopyTextureInfo src{
      .texture = source->texture,
      .mipLevel = 0,
      .origin = wgpu::Origin3D{.x = 0, .y = 0, .z = 0},
      .aspect = wgpu::TextureAspect::All,
  };
  const wgpu::TexelCopyBufferInfo dst{
      .layout =
          wgpu::TexelCopyBufferLayout{
              .offset = 0,
              .bytesPerRow = slot.rowBytes,
              .rowsPerImage = slot.srcHeight,
          },
      .buffer = slot.buffer,
  };
  const wgpu::Extent3D copySize{
      .width = slot.srcWidth,
      .height = slot.srcHeight,
      .depthOrArrayLayers = 1,
  };
  const webgpu::gpu_prof::Zone zone{cmd, "EFB copy read-back"};
  cmd.CopyTextureToBuffer(&src, &dst, &copySize);
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
      pending.push_back(i);
    }
  }

  for (const size_t slotIdx : pending) {
    wgpu::Buffer buffer;
    uint64_t bytes = 0;
    {
      std::lock_guard lock{g_mutex};
      buffer = g_slots[slotIdx].buffer;
      bytes = g_slots[slotIdx].bytes;
    }
    buffer.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::AllowSpontaneous,
                    [slotIdx](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                      complete_map(slotIdx, status, message);
                    });
  }
}

void shutdown() noexcept {
  // Port: the buffers die outside g_mutex. Destroying a buffer with a read-back still pending
  // completes its MapAsync inline, and that callback (complete_map) locks g_mutex: holding it here
  // deadlocked the shutdown (the space pirate's cloak leaves a small copy in flight).
  std::array<Slot, SlotCount> dying;
  {
    std::lock_guard lock{g_mutex};
    for (size_t i = 0; i < SlotCount; ++i) {
      dying[i] = std::move(g_slots[i]);
      g_slots[i] = {};
    }
    g_loggedFirst = false;
  }
  dying = {};
}
} // namespace aurora::gfx::efb_copy
