#include "fifo.hpp"
#include "../gfx/perf_counters.hpp"

#include "../thread.hpp"
#include "command_processor.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>

#include <tracy/Tracy.hpp>

#if defined(__ANDROID__)
#include <unistd.h>
#endif

namespace aurora::gx::fifo {
namespace detail {
uint8_t* sBufferData = nullptr;
uint32_t sBufferSize = 0;
uint32_t sBufferCapacity = 0;
bool sInDisplayList = false;
uint8_t* sDlBuffer = nullptr;
uint32_t sDlSize = 0;
uint32_t sDlWritePos = 0;
} // namespace detail

namespace {
constexpr Module Log{"aurora::gx::fifo"};
constexpr auto kProcessingMode = ProcessingMode::Thread;
constexpr uint32_t kDrawBatchSize = 1;

bool sFrameActive = false;
uint32_t sPendingDraws = 0;
// Port: AURORA_RENDER_EVERY: whether the frame being recorded is rendered at all.
std::atomic_bool sFrameRendered{true};
// Port: how many frames were raised to "rendered" because they hold an EFB copy, and how many of
// them were raised by a read-back rather than by a copy into a texture.
std::atomic_uint32_t sFramesForcedRendered{0};
std::atomic_uint32_t sFramesForcedByReadback{0};

namespace {
// Port: AURORA_RENDERSKIP_COPYDEST: the GXCopyTex destinations and the frame each was last copied
// in. The table is small: the game copies into a handful of buffers (the scan visor's window, the
// spare buffer, the cloaks).
struct CopyDestination {
  uint64_t address = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t format = 0;
  uint64_t lastCopiedFrame = 0;
  bool used = false;
};
constexpr size_t MaxCopyDestinations = 32;
CopyDestination sCopyDestinations[MaxCopyDestinations];
// The frame the game is recording (the FIFO's frame boundary).
uint64_t sFrameNumber = 0;
// The render interval: the default limit on how stale a copy's texture may be.
uint32_t sRenderInterval = 1;
std::atomic_uint32_t sCopiesSkipped{0};

// AURORA_RENDERSKIP_COPYDEST=1 turns the destination rule on, 0 turns it off.
bool copy_destination_rule_enabled() {
  static const bool enabled = [] {
    const char* env = std::getenv("AURORA_RENDERSKIP_COPYDEST");
    if (env != nullptr) {
      return std::strcmp(env, "0") != 0;
    }
    return true;
  }();
  return enabled;
}

// How many frames a copy's texture may lag behind: the render interval, or
// AURORA_RENDERSKIP_COPYDEST_N when that is set.
uint32_t copy_age_limit() {
  static const uint32_t limit = [] {
    const char* env = std::getenv("AURORA_RENDERSKIP_COPYDEST_N");
    if (env != nullptr) {
      const long v = std::strtol(env, nullptr, 10);
      if (v > 0) {
        return static_cast<uint32_t>(v);
      }
    }
    return 0u; // 0: the render interval
  }();
  return limit != 0 ? limit : sRenderInterval;
}

CopyDestination* find_copy_destination(uint64_t address, uint32_t width, uint32_t height,
                                        uint32_t format) noexcept {
  for (auto& candidate : sCopyDestinations) {
    if (candidate.used && candidate.address == address && candidate.width == width &&
        candidate.height == height && candidate.format == format) {
      return &candidate;
    }
  }
  return nullptr;
}

CopyDestination* add_copy_destination(uint64_t address, uint32_t width, uint32_t height,
                                       uint32_t format) noexcept {
  CopyDestination* slot = nullptr;
  for (auto& candidate : sCopyDestinations) {
    if (!candidate.used) {
      slot = &candidate;
      break;
    }
  }
  if (slot == nullptr) {
    // The table is full: reuse the oldest entry.
    slot = &sCopyDestinations[0];
    for (auto& candidate : sCopyDestinations) {
      if (candidate.lastCopiedFrame < slot->lastCopiedFrame) {
        slot = &candidate;
      }
    }
  }
  *slot = CopyDestination{
      .address = address,
      .width = width,
      .height = height,
      .format = format,
      .lastCopiedFrame = 0,
      .used = true,
  };
  return slot;
}
} // namespace
std::atomic<uint64_t> sPublished{0};
std::atomic<uint64_t> sProcessed{0};
uint64_t sStreamBase = 0;
std::mutex sBufferMutex;
std::atomic<uint32_t> sWorkerWake{0};
thread::Thread sWorkerThread;
// The kernel's id for the worker, for an OpenXR runtime's scheduling hints.
std::atomic<uint32_t> sWorkerNativeId{0};
std::atomic<DrawDoneCallback> sDrawDoneCallback{nullptr};

void dispatch_draw_done() noexcept {
  if (const auto callback = sDrawDoneCallback.load(std::memory_order_acquire); callback != nullptr) {
    callback();
  }
}

void wake_worker() noexcept {
  sWorkerWake.fetch_add(1, std::memory_order_release);
  sWorkerWake.notify_all();
}

void process_to(uint64_t target, std::memory_order order) noexcept {
  uint64_t processed = sProcessed.load(std::memory_order_relaxed);
  while (processed < target) {
    ProcessResult result{};
    {
      std::lock_guard lock{sBufferMutex};
      // The producer may already be appending the next batch. Only its atomic
      // published watermark is shared with us; sBufferSize is producer-owned.
      const uint64_t published = sPublished.load(std::memory_order_acquire);
      AURORA_ASSERT(processed >= sStreamBase && target <= published,
                    "FIFO processing range [{}, {}) is outside published range [{}, {})", processed, target, sStreamBase,
                    published);
      const auto start = static_cast<uint32_t>(processed - sStreamBase);
      const auto size = static_cast<uint32_t>(target - processed);
      {
        const gfx::perf::Timer timer{gfx::perf::g_fifoNs};
        result = process(detail::sBufferData + start, size);
      }
      if (gfx::perf::enabled()) {
        gfx::perf::g_fifoBytes.fetch_add(result.bytesProcessed, std::memory_order_relaxed);
      }
    }
    AURORA_ASSERT(result.bytesProcessed > 0 && result.bytesProcessed <= target - processed,
                  "FIFO processor made invalid progress: processed {} of {} remaining bytes", result.bytesProcessed,
                  target - processed);
    if (result.drawDone) {
      dispatch_draw_done();
    }
    processed += result.bytesProcessed;
    sProcessed.store(processed, order);
    sProcessed.notify_all();
  }
}

void worker_main(std::stop_token token) noexcept {
  std::stop_callback wakeOnStop{token, wake_worker};
#if defined(__ANDROID__)
  sWorkerNativeId.store(static_cast<uint32_t>(gettid()), std::memory_order_release);
#endif
  while (true) {
    const uint32_t event = sWorkerWake.load(std::memory_order_acquire);
    const uint64_t processed = sProcessed.load(std::memory_order_relaxed);
    const uint64_t published = sPublished.load(std::memory_order_acquire);
    if (published != processed) {
      process_to(published, std::memory_order_release);
      continue;
    }

    if (token.stop_requested()) {
      break;
    }
    sWorkerWake.wait(event, std::memory_order_acquire);
  }
}

void start_worker() {
  if (kProcessingMode != ProcessingMode::Thread || sWorkerThread.joinable()) {
    return;
  }
  sWorkerThread = thread::Thread{{
                                     .name = "Aurora FIFO processor",
                                     .affinity = thread::Affinity::SharedCache,
                                 },
                                 worker_main};
}

void stop_worker() {
  if (!sWorkerThread.joinable()) {
    return;
  }
  sWorkerThread.request_stop();
  sWorkerThread.join();
  sWorkerNativeId.store(0, std::memory_order_release);
}
} // namespace

ProcessingMode processing_mode() noexcept { return kProcessingMode; }

uint32_t native_thread_id() noexcept { return sWorkerNativeId.load(std::memory_order_acquire); }

void init() {
  stop_worker();
  reset_draw_cache();

  constexpr uint32_t initialCapacity = 64 * 1024;
  free(detail::sBufferData);
  detail::sBufferData = static_cast<uint8_t*>(malloc(initialCapacity));
  AURORA_ASSERT(detail::sBufferData != nullptr, "fifo::init: failed to allocate {} bytes", initialCapacity);
  detail::sBufferSize = 0;
  detail::sBufferCapacity = initialCapacity;
  detail::sInDisplayList = false;
  detail::sDlBuffer = nullptr;
  detail::sDlSize = 0;
  detail::sDlWritePos = 0;

  sFrameActive = false;
  sPendingDraws = 0;
  sStreamBase = 0;
  sPublished.store(0, std::memory_order_relaxed);
  sProcessed.store(0, std::memory_order_relaxed);
  sWorkerWake.store(0, std::memory_order_relaxed);

  start_worker();
}

void shutdown() {
  stop_worker();
  reset_draw_cache();
}

void begin_frame() noexcept {
  sFrameActive = true;
  sFrameRendered.store(true, std::memory_order_release);
  ++sFrameNumber;
}

void end_frame() noexcept {
  sFrameActive = false;
  clear_draw_cache(); // command_processor
}

void set_frame_rendered(bool rendered) noexcept { sFrameRendered.store(rendered, std::memory_order_release); }

bool frame_rendered() noexcept { return sFrameRendered.load(std::memory_order_acquire); }

static bool sHeadless = false;

void set_headless(bool headless) noexcept { sHeadless = headless; }

bool headless() noexcept { return sHeadless; }

void force_frame_rendered(FrameRenderReason reason) noexcept {
  if (sHeadless) {
    return;
  }
  if (!sFrameRendered.exchange(true, std::memory_order_acq_rel)) {
    sFramesForcedRendered.fetch_add(1, std::memory_order_acq_rel);
    if (reason == FrameRenderReason::Readback) {
      sFramesForcedByReadback.fetch_add(1, std::memory_order_acq_rel);
    }
  }
}

uint32_t frames_forced_rendered() noexcept { return sFramesForcedRendered.load(std::memory_order_acquire); }

uint32_t frames_forced_by_readback() noexcept { return sFramesForcedByReadback.load(std::memory_order_acquire); }

void set_render_interval(uint32_t interval) noexcept { sRenderInterval = interval; }

void note_copy_destination(uint64_t address, uint32_t width, uint32_t height, uint32_t format) noexcept {
  const uint64_t frame = sFrameNumber;
  CopyDestination* entry = find_copy_destination(address, width, height, format);
  bool force = true;
  if (copy_destination_rule_enabled() && entry != nullptr) {
    // The destination holds an image that was copied at most N frames ago: a copy into it can be
    // skipped (the texture lags by up to N frames), so the frame does not have to be rendered.
    const uint64_t age = frame - entry->lastCopiedFrame;
    force = age > copy_age_limit();
  }
  if (force) {
    // A new or one-shot destination (a render-to-texture at an area load) has to be produced, so
    // the frame that carries the copy is rendered in full.
    force_frame_rendered(FrameRenderReason::Copy);
  }
  if (frame_rendered()) {
    // The copy is made in this frame.
    if (entry == nullptr) {
      entry = add_copy_destination(address, width, height, format);
    }
    entry->lastCopiedFrame = frame;
    return;
  }
  // The copy was skipped: the texture keeps the image of the last frame that was rendered.
  sCopiesSkipped.fetch_add(1, std::memory_order_relaxed);
}

uint32_t copies_skipped() noexcept { return sCopiesSkipped.load(std::memory_order_acquire); }

bool frame_active() noexcept { return sFrameActive; }

void write_data_grow(const void* data, uint32_t length) {
  const uint64_t needed64 = static_cast<uint64_t>(detail::sBufferSize) + length;
  AURORA_ASSERT(needed64 <= std::numeric_limits<uint32_t>::max(), "fifo::write_data: buffer size overflow");
  const auto needed = static_cast<uint32_t>(needed64);
  const auto doubledCapacity = static_cast<uint64_t>(detail::sBufferCapacity) * 2;
  const auto newCapacity = static_cast<uint32_t>(
      std::min<uint64_t>(std::max(doubledCapacity, needed64), std::numeric_limits<uint32_t>::max()));
  const auto grow = [newCapacity] {
    auto* resized = static_cast<uint8_t*>(realloc(detail::sBufferData, newCapacity));
    AURORA_ASSERT(resized != nullptr, "fifo::write_data: failed to allocate {} bytes", newCapacity);
    detail::sBufferData = resized;
  };
  if (sWorkerThread.joinable()) {
    std::lock_guard lock{sBufferMutex};
    grow();
  } else {
    grow();
  }
  std::memcpy(detail::sBufferData + detail::sBufferSize, data, length);
  detail::sBufferSize = needed;
  detail::sBufferCapacity = newCapacity;
}

void publish() noexcept {
  if (!sFrameActive || kProcessingMode == ProcessingMode::Drain || detail::sInDisplayList) {
    return;
  }

  const uint64_t target = sStreamBase + detail::sBufferSize;
  if (target > sPublished.load(std::memory_order_relaxed)) {
    sPendingDraws = 0;
    sPublished.store(target, std::memory_order_release);
    if (kProcessingMode == ProcessingMode::Thread) {
      wake_worker();
    } else {
      process_to(target, std::memory_order_relaxed);
    }
  }
}

DrawDoneCallback set_draw_done_callback(DrawDoneCallback callback) noexcept {
  return sDrawDoneCallback.exchange(callback, std::memory_order_acq_rel);
}

void finish_draw() noexcept {
  if (!sFrameActive || kProcessingMode == ProcessingMode::Drain || detail::sInDisplayList) {
    return;
  }
  if (++sPendingDraws >= kDrawBatchSize) {
    publish();
  }
}

void patch_u32(uint32_t offset, uint32_t val) {
  AURORA_ASSERT(!detail::sInDisplayList && offset <= detail::sBufferSize &&
                    sizeof(uint32_t) <= detail::sBufferSize - offset,
                "fifo::patch_u32: invalid patch offset {} (buffer size {})", offset, detail::sBufferSize);
  AURORA_ASSERT(sStreamBase + offset >= sPublished.load(std::memory_order_relaxed),
                "fifo::patch_u32: offset {} is below the published watermark", offset);
  const auto out = bswap(val);
  std::memcpy(detail::sBufferData + offset, &out, sizeof(out));
}

void begin_display_list(uint8_t* buf, uint32_t size) {
  detail::sInDisplayList = true;
  detail::sDlBuffer = buf;
  detail::sDlSize = size;
  detail::sDlWritePos = 0;
}

uint32_t end_display_list() {
  detail::sInDisplayList = false;
  const uint32_t bytesWritten = detail::sDlWritePos;
  const uint32_t padded = (bytesWritten + 31) & ~31u;
  while (detail::sDlWritePos < padded && detail::sDlWritePos < detail::sDlSize) {
    detail::sDlBuffer[detail::sDlWritePos++] = 0;
  }
  detail::sDlBuffer = nullptr;
  detail::sDlSize = 0;
  detail::sDlWritePos = 0;
  return padded;
}

bool in_display_list() { return detail::sInDisplayList; }

void drain() {
  if (detail::sBufferSize == 0) {
    return;
  }

  ZoneScoped;
  const uint64_t target = sStreamBase + detail::sBufferSize;

  switch (kProcessingMode) {
  case ProcessingMode::Drain:
  case ProcessingMode::Inline:
    sPublished.store(target, std::memory_order_relaxed);
    process_to(target, std::memory_order_relaxed);
    break;
  case ProcessingMode::Thread: {
    sPublished.store(target, std::memory_order_release);
    wake_worker();

    if (gfx::perf::enabled()) {
      gfx::perf::g_drainCalls.fetch_add(1, std::memory_order_relaxed);
    }
    uint64_t processed = sProcessed.load(std::memory_order_acquire);
    if (processed < target) {
      const gfx::perf::Timer timer{gfx::perf::g_drainWaitNs};
      do {
        sProcessed.wait(processed, std::memory_order_acquire);
        processed = sProcessed.load(std::memory_order_acquire);
      } while (processed < target);
    }
    break;
  }
  }

  {
    std::lock_guard lock{sBufferMutex};
    sStreamBase = target;
    detail::sBufferSize = 0;
  }
  sPendingDraws = 0;
}

const uint8_t* get_buffer_data() { return detail::sBufferData; }
uint32_t get_buffer_size() { return detail::sBufferSize; }

void clear_buffer() {
  const uint64_t processed = sProcessed.load(std::memory_order_acquire);
  AURORA_ASSERT(sPublished.load(std::memory_order_acquire) == processed,
                "fifo::clear_buffer: published commands are still pending");
  std::lock_guard lock{sBufferMutex};
  sStreamBase = processed;
  detail::sBufferSize = 0;
  sPendingDraws = 0;
}

} // namespace aurora::gx::fifo
