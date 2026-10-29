#pragma once

#include "../internal.hpp"

#include <cstring>

namespace aurora::gx::fifo {

namespace detail {
extern uint8_t* sBufferData;
extern uint32_t sBufferSize;
extern uint32_t sBufferCapacity;
extern bool sInDisplayList;
extern uint8_t* sDlBuffer;
extern uint32_t sDlSize;
extern uint32_t sDlWritePos;
} // namespace detail

enum class ProcessingMode : uint8_t {
  // Process FIFO synchronously at drain()
  Drain,
  // Process FIFO synchronously at publish() (useful for profiling)
  Inline,
  // Process FIFO asynchronously on a worker thread; drain() synchronizes
  Thread
};
ProcessingMode processing_mode() noexcept;
// The FIFO processor thread's kernel id where the platform has one (Android), else 0.
uint32_t native_thread_id() noexcept;

void init();
void shutdown();

void begin_frame() noexcept;
void end_frame() noexcept;

// Port: AURORA_RENDER_EVERY=<n> renders one frame in n (1, the default, renders every frame).
// A frame that is not rendered records no draw: the command stream is still walked, so the
// register state survives, but no vertex data is copied and no draw command is built.
void set_frame_rendered(bool rendered) noexcept;
bool frame_rendered() noexcept;
// Port: a frame that holds an EFB copy is rendered in full, whatever the counter said: the copy's
// texture would otherwise never be produced (a one-shot copy at an area load would stay blank for
// good). The copy path raises the flag before the pass that carries the copy is sealed, so the
// rest of the frame is recorded and encoded too.
enum class FrameRenderReason : uint8_t {
  Copy,     // a copy of the EFB into a texture the game keeps (GXCopyTex)
  Readback, // a read-back of the EFB the game asks for (the spare buffer, the scan target)
};
void force_frame_rendered(FrameRenderReason reason) noexcept;
// Port: AURORA_HEADLESS: no frame is rendered, not even one that holds an EFB copy or a read-back
// (force_frame_rendered does nothing; the read-back requests are dropped where they are queued).
void set_headless(bool headless) noexcept;
bool headless() noexcept;
// How many frames were raised this way (logged at shutdown), and how many of them were read-backs.
uint32_t frames_forced_rendered() noexcept;
uint32_t frames_forced_by_readback() noexcept;

// Port: AURORA_RENDERSKIP_COPYDEST: a GXCopyTex destination (the game's destination address, the
// copied size and format) and the frame it was last copied in. A copy into a destination that was
// copied within the last N frames can be skipped: the texture still holds a fresh-enough image (a
// lag of up to N frames). A new or one-shot destination (a render-to-texture at an area load) has
// to be produced, so the frame that carries it is rendered in full.
void note_copy_destination(uint64_t address, uint32_t width, uint32_t height, uint32_t format) noexcept;
// How many copies were skipped because their destination was fresh enough.
uint32_t copies_skipped() noexcept;
// The render interval (AURORA_RENDER_EVERY): the default limit on how stale a copy's texture may
// be before its frame is rendered in full.
void set_render_interval(uint32_t interval) noexcept;

// Whether a frame is currently being recorded, i.e. publish() will be honored.
bool frame_active() noexcept;

// Out-of-line slow path: grows internal buffer then appends data
void write_data_grow(const void* data, uint32_t length);

inline void write_data(const void* data, const uint32_t length) {
  if (!detail::sInDisplayList)
    LIKELY {
      if (length <= detail::sBufferCapacity - detail::sBufferSize)
        LIKELY {
          std::memcpy(detail::sBufferData + detail::sBufferSize, data, length);
          detail::sBufferSize += length;
          return;
        }
      write_data_grow(data, length);
    }
  else if (length <= detail::sDlSize - detail::sDlWritePos) {
    std::memcpy(detail::sDlBuffer + detail::sDlWritePos, data, length);
    detail::sDlWritePos += length;
  }
}

inline void write_u8(const uint8_t val) {
  if (!detail::sInDisplayList)
    LIKELY {
      if (detail::sBufferSize < detail::sBufferCapacity)
        LIKELY {
          detail::sBufferData[detail::sBufferSize++] = val;
          return;
        }
      write_data_grow(&val, 1);
    }
  else if (detail::sDlWritePos < detail::sDlSize) {
    detail::sDlBuffer[detail::sDlWritePos++] = val;
  }
}

inline void write_u16(const uint16_t val) {
  const auto out = bswap(val);
  write_data(&out, sizeof(out));
}

inline void write_u32(const uint32_t val) {
  const auto out = bswap(val);
  write_data(&out, sizeof(out));
}

inline void write_u64(const uint64_t val) {
  const auto out = bswap(val);
  write_data(&out, sizeof(out));
}

inline void write_f32(const float val) {
  const auto out = bswap(val);
  write_data(&out, sizeof(out));
}

// Overwrites an unpublished u32 previously written at the given offset.
void patch_u32(uint32_t offset, uint32_t val);

// Marks a complete draw and publishes when the configured draw batch is full.
void finish_draw() noexcept;

// Makes commands written so far available to the FIFO processor.
void publish() noexcept;

using DrawDoneCallback = void (*)();
DrawDoneCallback set_draw_done_callback(DrawDoneCallback callback) noexcept;

// Display list recording
void begin_display_list(uint8_t* buf, uint32_t size);
uint32_t end_display_list();
bool in_display_list();

// Ensure all buffered commands have been processed.
void drain();

// Internal buffer inspection
const uint8_t* get_buffer_data();
uint32_t get_buffer_size();
void clear_buffer();

} // namespace aurora::gx::fifo
