#!/usr/bin/env python3
"""Rebuild lib/gx/command_processor.cpp for the Echoes VR merge: the MP1 VR fork's file (cp_v.cpp, =
MP1 upstream 011c131db + PrimedGun) plus MetroidPrime2Port's own additions (cp_b_e.diff): draw
diagnostics, the no-render-pass early return, MP2_GX_STORAGE_LOG, the dstAlpha alpha-only second
draw, AURORA_RENDER_EVERY in the draw paths, and the scan peek / EFB read-back / depth snapshot
subcommands. Echoes' cherry-picks of MP1 commits the fork already has (array check, whole-triangle
handling, display-list chunk source offsets) are not re-applied.

    python -I cp_rebuild.py <cp_v.cpp> <out>
"""
import sys

src, out = sys.argv[1], sys.argv[2]
s = open(src, "rb").read().decode("utf-8")


def sub(old, new, count=1):
    global s
    n = s.count(old)
    if n != count:
        raise SystemExit(f"anchor found {n} times, expected {count}:\n{old}")
    s = s.replace(old, new)


# 1. Includes.
sub('#include "../gfx/depth_peek.hpp"\n#include "geometry_cache.hpp"\n',
    '#include "../gfx/depth_peek.hpp"\n#include "../gfx/perf.hpp"\n#include "geometry_cache.hpp"\n')
sub('#include "dolphin/gx/GXAurora.h"\n#include "gx.hpp"\n',
    '#include "dolphin/gx/GXAurora.h"\n#include "dolphin/gx/GXCpu2Efb.h"\n#include "fifo.hpp"\n#include "gx.hpp"\n')
sub('#include <cstdint>\n#include <cstdlib>\n#include <cstring>\n#include <span>\n',
    '#include <cstdint>\n#include <cstdio>  // port diagnostic: MP2_GX_STORAGE_LOG\n#include <cstdlib>\n'
    '#include <cstring>\n#include <span>\n')

# 2. Draw diagnostics (MetroidPrime2Port).
DIAG = '''// Port diagnostic: when a draw's vertex stream does not match its declared format, too few bytes
// are consumed and the leftovers are read as commands ("unsupported primitive type"). Keep the
// last draw's numbers so the fatal can say which format/attribute was misdeclared.
static GXVtxFmt sLastDrawFmt = GX_VTXFMT0;
static u32 sLastDrawVtxSize = 0;
static u16 sLastDrawVtxCount = 0;
static GXPrimitive sLastDrawPrim = GX_QUADS;

// A ring of the last draws, because the failing draw is the one *after* the misdeclared one: it is
// the previous vertex stream that was under-consumed, and its leftovers that decode as commands.
struct DrawRecord {
  u32 prim;
  u32 fmt;
  u32 vtxCount;
  u32 vtxSize;
  u32 posDesc, posCnt, posType;
  u32 texDesc, texCnt, texType;
};
static DrawRecord sDrawRing[8];
static u32 sDrawRingHead = 0;

// Port diagnostic: print what the last draw declared, so a FIFO decode failure names the caller
// that misdeclared its vertex format.
static void record_draw(GXPrimitive prim, GXVtxFmt fmt, u16 vtxCount, u32 vtxSize) noexcept {
  const auto& f = g_gxState.vtxFmts[fmt];
  DrawRecord& r = sDrawRing[sDrawRingHead++ & 7];
  r.prim = static_cast<u32>(prim);
  r.fmt = static_cast<u32>(fmt);
  r.vtxCount = vtxCount;
  r.vtxSize = vtxSize;
  r.posDesc = static_cast<u32>(g_gxState.vtxDesc[GX_VA_POS]);
  r.posCnt = static_cast<u32>(f.attrs[GX_VA_POS].cnt);
  r.posType = static_cast<u32>(f.attrs[GX_VA_POS].type);
  r.texDesc = static_cast<u32>(g_gxState.vtxDesc[GX_VA_TEX0]);
  r.texCnt = static_cast<u32>(f.attrs[GX_VA_TEX0].cnt);
  r.texType = static_cast<u32>(f.attrs[GX_VA_TEX0].type);
}

static void dump_draw_ring() noexcept {
  Log.error("  last {} draws (oldest first): prim fmt count size posDesc/Cnt/Type texDesc/Cnt/Type",
            sDrawRingHead < 8 ? sDrawRingHead : 8);
  const u32 first = sDrawRingHead < 8 ? 0 : sDrawRingHead;
  for (u32 i = first; i < sDrawRingHead; ++i) {
    const DrawRecord& r = sDrawRing[i & 7];
    Log.error("   {} {} {} {} {}/{}/{} {}/{}/{}", r.prim, r.fmt, r.vtxCount, r.vtxSize, r.posDesc,
              r.posCnt, r.posType, r.texDesc, r.texCnt, r.texType);
  }
}

static void dump_last_draw_state() noexcept {
  const auto& fmt = g_gxState.vtxFmts[sLastDrawFmt];
  Log.error("  last draw: prim={} fmt={} vtxCount={} vtxSize={} (= count*size bytes consumed)",
            static_cast<u32>(sLastDrawPrim), static_cast<u32>(sLastDrawFmt), sLastDrawVtxCount,
            sLastDrawVtxSize);
  for (int i = GX_VA_POS; i <= GX_VA_TEX7; ++i) {
    const auto& attr = fmt.attrs[i];
    Log.error("  fmt{} attr {}: desc={} cnt={} type={} frac={}", static_cast<u32>(sLastDrawFmt), i,
              static_cast<u32>(g_gxState.vtxDesc[i]), static_cast<u32>(attr.cnt),
              static_cast<u32>(attr.type), static_cast<u32>(attr.frac));
  }
}

'''
sub('constexpr Module Log{"aurora::gx::fifo"};\n\n// The triangle-list indices a primitive takes',
    'constexpr Module Log{"aurora::gx::fifo"};\n\n' + DIAG + '// The triangle-list indices a primitive takes')
sub('''    return 6;
  default:
    UNLIKELY FATAL("unsupported primitive type {}", static_cast<u32>(prim));
  }
  return 0;''', '''    return 6;
  default:
    dump_last_draw_state();
    UNLIKELY FATAL("unsupported primitive type {}", static_cast<u32>(prim));
  }
  return 0;''')
sub('''  Log.error("  hex dump around draw cmd (pos {}-{}):{}", dumpStart, dumpEnd - 1, hex);
  FATAL("draw vertex data overrun''', '''  Log.error("  hex dump around draw cmd (pos {}-{}):{}", dumpStart, dumpEnd - 1, hex);
  dump_last_draw_state();
  dump_draw_ring();
  FATAL("draw vertex data overrun''')

# 3. push_gx_draw: no render pass, storage log, alpha-only second draw.
sub('''                         u32 numIndices, bool cachedGeometry = false) noexcept {
  auto& state = g_gxState;
  auto& cache = sDrawCache;
''', '''                         u32 numIndices, bool cachedGeometry = false) noexcept {
  // A frame begin_frame declined (Android surface lost or invalidated, e.g. mid scan-visor open)
  // still runs the game's draws; with no render pass, building a pipeline would index the pass
  // list with UINT32_MAX, so drop the draw like push_command drops the frame's other commands.
  if (!gfx::has_render_pass())
    UNLIKELY { return; }
  auto& state = g_gxState;
  auto& cache = sDrawCache;
''')
sub('''    if (array.cachedRange.size == 0 && !resident::array_range(array.data, array.size, array.cachedRange)) {
      array.cachedRange = gfx::push_storage(static_cast<const uint8_t*>(array.data), array.size);''',
    '''    if (array.cachedRange.size == 0 && !resident::array_range(array.data, array.size, array.cachedRange)) {
      // Port diagnostic (see gfx/recording.cpp's push_storage): the frame's storage is bounded, and
      // this is the upload that usually fills it.
      static const bool sLog = std::getenv("MP2_GX_STORAGE_LOG") != nullptr;
      if (sLog) {
        std::fprintf(stderr, "[gx-storage] vertex array %d: %zu bytes at %p\\n", i, array.size,
                     array.data);
      }
      array.cachedRange = gfx::push_storage(static_cast<const uint8_t*>(array.data), array.size);''')
sub('''    } else {
      gfx::shadow::add_caster(caster);
    }
  }
  if (perfOn) {
    const auto t3 = gfx::perf::tick();''', '''    } else {
      gfx::shadow::add_caster(caster);
    }
  }

  // Port: GXSetDstAlpha under a source-alpha blend on a device without dual-source blending (or
  // AURORA_DUAL_SOURCE=0): dst_alpha_mode gives the draw above None, so it wrote its TEV alpha into
  // the EFB alpha. The console writes the constant: a fading actor in the scan visor otherwise ramps
  // its id through other objects' palettes and the reticle locks onto them (CMinorIng surfacing).
  // Redraw the same fragments alpha-only, which dst_alpha_mode gives Replace (the constant); no
  // blend, and LESS/GREATER become LEQUAL/GEQUAL so the pass survives the depth the draw above wrote.
  if (!state.shadowCasterOnly && !cache.prepassSkip && state.dstAlpha != UINT32_MAX &&
      dst_alpha_mode(cache.config) == DstAlphaMode::None) {
    PipelineConfig alphaConfig = cache.config;
    alphaConfig.blendMode = GX_BM_NONE;
    alphaConfig.blendFacSrc = GX_BL_ONE;
    alphaConfig.blendFacDst = GX_BL_ZERO;
    alphaConfig.blendOp = GX_LO_COPY;
    alphaConfig.colorUpdate = false;
    alphaConfig.depthUpdate = false;
    if (alphaConfig.depthFunc == GX_LESS) {
      alphaConfig.depthFunc = GX_LEQUAL;
    } else if (alphaConfig.depthFunc == GX_GREATER) {
      alphaConfig.depthFunc = GX_GEQUAL;
    }
    if (dst_alpha_mode(alphaConfig) == DstAlphaMode::Replace) {
      aurora::perf::count(AURORA_PERF_DST_ALPHA_DUP);
      DrawData alphaDraw = draw;
      alphaDraw.pipeline = gfx::pipeline_ref(alphaConfig);
      // PrimedGun: the eye passes draw the same alpha-only pass, with that config's eye pipeline.
      if (eyeMode != MultiviewNone) {
        PipelineConfig eyeConfig = alphaConfig;
        eyeConfig.shaderConfig.multiview = eyeMode;
        alphaDraw.multiviewPipeline = gfx::pipeline_ref(eyeConfig, eyeMode != EyeClipImmediate || blocking_pipelines());
      }
      gfx::push_draw_command(alphaDraw);
      // The next draw must not merge its vertices into this alpha pass instead of the draw above.
      state.dirty |= DirtyPipeline;
    }
  }
  if (perfOn) {
    const auto t3 = gfx::perf::tick();''')

# 4. draw_prim: diagnostics record, AURORA_RENDER_EVERY.
sub('''  u32 totalVtxBytes = vtxCount * vtxSize;
  if (totalVtxBytes > reader.remaining())
    UNLIKELY { handle_draw_overrun(totalVtxBytes, reader); }

  // A draw whose indices read past its arrays''', '''  sLastDrawFmt = fmt;
  sLastDrawVtxSize = vtxSize;
  sLastDrawVtxCount = vtxCount;
  sLastDrawPrim = prim;
  record_draw(prim, fmt, vtxCount, vtxSize);

  u32 totalVtxBytes = vtxCount * vtxSize;
  if (totalVtxBytes > reader.remaining())
    UNLIKELY { handle_draw_overrun(totalVtxBytes, reader); }

  // Port: AURORA_RENDER_EVERY: a frame that is not rendered drops its draws. The vertex data is
  // consumed (the stream has to stay in step) but not copied into the staging buffer, and no draw
  // command is built, so the FIFO processor does the register work only. The state itself has to
  // be kept: the game sets the viewport, the projection and the copy-clear outside its per-frame
  // setup, so a frame whose commands were dropped entirely would leave the next rendered frame
  // drawing with a stale state. A frame that holds an EFB copy is raised to "rendered" while it is
  // recorded (gx::fifo::force_frame_rendered), so its draws are recorded after all.
  if (!frame_rendered()) {
    reader.take(totalVtxBytes);
    return;
  }

  // A draw whose indices read past its arrays''')

# 5. GX_AURORA_DRAW_INDEXED: AURORA_RENDER_EVERY before anything is staged.
sub('''    // Index data is always host-endian; push it to the GPU buffer as-is
    const auto indexData = reader.take(idxBytes);
    const gfx::Range idxRange = gfx::push_indices(indexData.data(), indexData.size(), 4);
    u32 vtxSize;
    if (g_gxState.lastVtxFmt == fmt) {
      vtxSize = g_gxState.lastVtxSize;
    } else {
      vtxSize = calc_vtx_size(fmt);
    }
    const u32 totalVtxBytes = vtxCount * vtxSize;
    const auto vertexData = reader.take(totalVtxBytes);''', '''    // Index data is always host-endian; push it to the GPU buffer as-is
    const auto indexData = reader.take(idxBytes);
    u32 vtxSize;
    if (g_gxState.lastVtxFmt == fmt) {
      vtxSize = g_gxState.lastVtxSize;
    } else {
      vtxSize = calc_vtx_size(fmt);
    }
    const u32 totalVtxBytes = vtxCount * vtxSize;
    // Port: AURORA_RENDER_EVERY: a frame that is not rendered records no draw (see draw_prim).
    if (!frame_rendered()) {
      reader.take(totalVtxBytes);
      return;
    }
    const gfx::Range idxRange = gfx::push_indices(indexData.data(), indexData.size(), 4);
    const auto vertexData = reader.take(totalVtxBytes);''')

# 6. Subcommands 0x3B-0x3E (MetroidPrime2Port).
sub('''  } else if (subCmd == GX_AURORA_REQUEST_DEPTH_SNAPSHOT) {
    gfx::depth_peek::request_snapshot();
''', '''  } else if (subCmd == GX_AURORA_REQUEST_DEPTH_SNAPSHOT) {
    gfx::depth_peek::request_snapshot();
  } else if (subCmd == GX_AURORA_SNAPSHOT_DEPTH) {
    gfx::split_pass_for_depth_snapshot();
  } else if (subCmd == GX_AURORA_REQUEST_SCAN_PEEK || subCmd == GX_AURORA_REQUEST_SCAN_PEEK_EX) {
    const s32 left = static_cast<s32>(reader.read<u32>());
    const s32 top = static_cast<s32>(reader.read<u32>());
    const u32 width = reader.read<u32>();
    const u32 height = reader.read<u32>();
    auto* idOut = reinterpret_cast<void*>(reader.read<u64>());
    auto* depthHighOut = reinterpret_cast<void*>(reader.read<u64>());
    auto* depthLowOut = reinterpret_cast<void*>(reader.read<u64>());
    auto* sequence = reinterpret_cast<u32*>(reader.read<u64>());
    u32 flags = 0;
    if (subCmd == GX_AURORA_REQUEST_SCAN_PEEK_EX) {
      flags = reader.read<u32>();
      reader.read<u32>(); // padding
    }
    // The game's coordinates are logical, as GXSetTexCopySrc's are: map them the same way the
    // copy path does, so the peek reads the texels the copy would have written.
    const auto rect = map_logical_scissor({left, top, static_cast<int32_t>(width), static_cast<int32_t>(height)});
    gfx::queue_scan_peek(gfx::scan_peek::Request{
        .idOut = idOut,
        .depthHighOut = depthHighOut,
        .depthLowOut = depthLowOut,
        .sequenceOut = sequence,
        .srcX = rect.x,
        .srcY = rect.y,
        .srcWidth = rect.width,
        .srcHeight = rect.height,
        .width = width,
        .height = height,
        .depthOnly = (flags & GX_SCAN_PEEK_DEPTH_ONLY) != 0,
        .ifRendered = (flags & GX_SCAN_PEEK_IF_RENDERED) != 0,
    });
  } else if (subCmd == GX_AURORA_REQUEST_EFB_COPY) {
    const auto staging = reinterpret_cast<void*>(reader.read<u64>());
    auto* sequence = reinterpret_cast<u32*>(reader.read<u64>());
    const u32 capacity = reader.read<u32>();
    reader.skip(4); // padding
    // The geometry and format are the copy's own, exactly as the console's copy would use them;
    // the destination size is the game's logical one, which is the size it uploads the bytes at.
    if (gfx::efb_copy::has_cpu_format(g_gxState.texCopyFmt)) {
      gfx::queue_efb_cpu_copy(gfx::efb_copy::Request{
          .out = staging,
          .sequenceOut = sequence,
          .capacity = capacity,
          .width = g_gxState.texCopyDstWidth,
          .height = g_gxState.texCopyDstHeight,
          .format = g_gxState.texCopyFmt,
      });
    }
''')
sub('''      GX2_SET_POLYGON_OFFSET,
      // PrimedGun's''', '''      GX2_SET_POLYGON_OFFSET,
      // MetroidPrime2Port's
      GX_AURORA_REQUEST_SCAN_PEEK,        GX_AURORA_REQUEST_EFB_COPY,         GX_AURORA_SNAPSHOT_DEPTH,
      GX_AURORA_REQUEST_SCAN_PEEK_EX,
      // PrimedGun's''')

open(out, "wb").write(s.encode("utf-8"))
print("ok")
