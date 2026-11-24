#ifndef AURORA_PERF_H
#define AURORA_PERF_H

// Port addition: per-frame counters for the work that costs most on a tiled GPU (render pass
// breaks, EFB copies) and on the FIFO thread (texture cache misses, re-hashes, extra draws).
// Counting is a relaxed atomic add; the recording thread publishes one frame's totals at the end
// of each recorded frame. MP2_PERF_COUNTERS=1 also logs average/max every 120 frames.

#ifdef __cplusplus
#include <cstdint>

extern "C" {
#else
#include "stdint.h"
#endif

enum {
  AURORA_PERF_BREAK_COPY = 0,       // GXCopyTex resolve (resolve_pass_into)
  AURORA_PERF_BREAK_DEPTH_SNAPSHOT, // GXSnapshotDepth (split_pass_for_depth_snapshot)
  AURORA_PERF_BREAK_ENCODER_TASK,   // push_encoder_task
  AURORA_PERF_BREAK_OFFSCREEN,      // begin_offscreen / end_offscreen
  AURORA_PERF_BREAK_RESOLVE,        // resolve_pass (custom-pass API)
  AURORA_PERF_BREAK_COUNT,
};

enum {
  AURORA_PERF_COPY_TEX = 0,     // GXCopyTex calls (game thread)
  AURORA_PERF_TEX_OBJ_HIT,      // static texture object-cache hits (FIFO thread)
  AURORA_PERF_TEX_OBJ_MISS,     // object-cache misses: resolve went past the cache
  AURORA_PERF_TEX_HASH,         // hash_texture_source calls (mincore + XXH3 of the texture)
  AURORA_PERF_TEX_UPLOAD,       // new GPU textures created from content
  AURORA_PERF_TEX_INIT,         // GXInitTexObj/GXInitTexObjCI (a new texObjId each)
  AURORA_PERF_TEX_INIT_DATA,    // GXInitTexObjData (bumps texDataVersion)
  AURORA_PERF_DST_ALPHA_DUP,    // dstAlpha alpha-only duplicate draws
  AURORA_PERF_BIND_INVALIDATE,  // texture::invalidate_bindings calls
  AURORA_PERF_DRAWS,            // recorded draw calls
  AURORA_PERF_ARRAY_UPLOAD,       // vertex arrays copied into the frame's storage (FIFO thread)
  AURORA_PERF_ARRAY_UPLOAD_BYTES, // their size in bytes
  AURORA_PERF_ARRAY_REUSE,        // array binds served by a copy made earlier in the frame
  AURORA_PERF_COUNTER_COUNT,
};

#define AURORA_PERF_MAX_COPY_SITES 24

typedef struct {
  uint32_t passBreaks[AURORA_PERF_BREAK_COUNT];
  uint32_t counters[AURORA_PERF_COUNTER_COUNT];
  // GXCopyTex callers by return address, for offline symbolisation (addr2line -f -e <binary>
  // <offset>); copySiteOffset is the address minus the containing module's load base.
  uint32_t copySiteCount;
  uint64_t copySiteOffset[AURORA_PERF_MAX_COPY_SITES];
  uint32_t copySiteHits[AURORA_PERF_MAX_COPY_SITES];
} AuroraPerfFrame;

// The last published frame's counters (may tear against the next publish; diagnostics only).
const AuroraPerfFrame* aurora_perf_last_frame(void);

#ifdef __cplusplus
}
#endif

#endif
