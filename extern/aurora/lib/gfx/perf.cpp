#include "perf.hpp"

#include "../internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace aurora::perf {
namespace {
constexpr Module Log{"aurora::perf"};
constexpr uint32_t LogInterval = 120;

// Caller slots persist for the run, so a site keeps its index; only the hit counts reset per frame.
std::atomic<uintptr_t> s_sitePc[AURORA_PERF_MAX_COPY_SITES];
std::atomic<uint64_t> s_siteOffset[AURORA_PERF_MAX_COPY_SITES];
std::atomic<uint32_t> s_siteHits[AURORA_PERF_MAX_COPY_SITES];
std::atomic<uint32_t> s_siteOverflow{0};

AuroraPerfFrame s_published[2];
std::atomic<uint32_t> s_publishedIndex{0};

struct Window {
  uint32_t frames = 0;
  uint64_t breakSum[AURORA_PERF_BREAK_COUNT]{};
  uint32_t breakMax[AURORA_PERF_BREAK_COUNT]{};
  uint64_t counterSum[AURORA_PERF_COUNTER_COUNT]{};
  uint32_t counterMax[AURORA_PERF_COUNTER_COUNT]{};
  uint64_t siteSum[AURORA_PERF_MAX_COPY_SITES]{};
  uint32_t siteMax[AURORA_PERF_MAX_COPY_SITES]{};
  uint64_t totalBreakSum = 0;
  uint32_t totalBreakMax = 0;
};
Window s_window;

constexpr const char* BreakNames[AURORA_PERF_BREAK_COUNT] = {"copy", "depthsnap", "encoder", "offscreen", "resolve"};
constexpr const char* CounterNames[AURORA_PERF_COUNTER_COUNT] = {
    "copytex", "texhit", "texmiss", "texhash", "texupload", "texinit", "texinitdata", "dstalphadup", "bindinval", "draws",
    "arrayupload", "arraybytes", "arrayreuse",
};

uint64_t module_offset(uintptr_t pc) noexcept {
#if defined(__unix__) || defined(__APPLE__)
  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(pc), &info) != 0 && info.dli_fbase != nullptr) {
    return pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
  }
#endif
  return pc;
}

void log_window() noexcept {
  const double n = s_window.frames;
  std::string line = fmt::format("perf {} frames avg/max: breaks {:.2f}/{}", s_window.frames,
                                 s_window.totalBreakSum / n, s_window.totalBreakMax);
  for (int i = 0; i < AURORA_PERF_BREAK_COUNT; ++i) {
    line += fmt::format(" {} {:.2f}/{}", BreakNames[i], s_window.breakSum[i] / n, s_window.breakMax[i]);
  }
  line += " |";
  for (int i = 0; i < AURORA_PERF_COUNTER_COUNT; ++i) {
    line += fmt::format(" {} {:.2f}/{}", CounterNames[i], s_window.counterSum[i] / n, s_window.counterMax[i]);
  }
  line += " | sites";
  for (int i = 0; i < AURORA_PERF_MAX_COPY_SITES; ++i) {
    if (s_window.siteSum[i] != 0) {
      line += fmt::format(" {:#x}:{:.2f}/{}", s_siteOffset[i].load(std::memory_order_relaxed),
                          s_window.siteSum[i] / n, s_window.siteMax[i]);
    }
  }
  if (const uint32_t overflow = s_siteOverflow.load(std::memory_order_relaxed); overflow != 0) {
    line += fmt::format(" (+{} unslotted)", overflow);
  }
  Log.info("{}", line);
  s_window = {};
}
} // namespace

std::atomic<uint32_t> g_passBreaks[AURORA_PERF_BREAK_COUNT];
std::atomic<uint32_t> g_counters[AURORA_PERF_COUNTER_COUNT];

void copy_site(const void* pc) noexcept {
  count(AURORA_PERF_COPY_TEX);
  const auto key = reinterpret_cast<uintptr_t>(pc);
  for (int i = 0; i < AURORA_PERF_MAX_COPY_SITES; ++i) {
    uintptr_t cur = s_sitePc[i].load(std::memory_order_acquire);
    if (cur == 0) {
      if (s_sitePc[i].compare_exchange_strong(cur, key, std::memory_order_acq_rel)) {
        s_siteOffset[i].store(module_offset(key), std::memory_order_relaxed);
        cur = key;
      }
    }
    if (cur == key) {
      s_siteHits[i].fetch_add(1, std::memory_order_relaxed);
      return;
    }
  }
  s_siteOverflow.fetch_add(1, std::memory_order_relaxed);
}

void end_frame() noexcept {
  static const bool sLog = [] {
    const char* env = std::getenv("MP2_PERF_COUNTERS");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
  }();
  const uint32_t next = s_publishedIndex.load(std::memory_order_relaxed) ^ 1u;
  AuroraPerfFrame& out = s_published[next];
  uint32_t totalBreaks = 0;
  for (int i = 0; i < AURORA_PERF_BREAK_COUNT; ++i) {
    out.passBreaks[i] = g_passBreaks[i].exchange(0, std::memory_order_relaxed);
    totalBreaks += out.passBreaks[i];
  }
  for (int i = 0; i < AURORA_PERF_COUNTER_COUNT; ++i) {
    out.counters[i] = g_counters[i].exchange(0, std::memory_order_relaxed);
  }
  out.copySiteCount = 0;
  uint32_t siteHits[AURORA_PERF_MAX_COPY_SITES];
  for (int i = 0; i < AURORA_PERF_MAX_COPY_SITES; ++i) {
    siteHits[i] = s_siteHits[i].exchange(0, std::memory_order_relaxed);
    if (siteHits[i] != 0) {
      out.copySiteOffset[out.copySiteCount] = s_siteOffset[i].load(std::memory_order_relaxed);
      out.copySiteHits[out.copySiteCount] = siteHits[i];
      ++out.copySiteCount;
    }
  }
  s_publishedIndex.store(next, std::memory_order_release);

  if (!sLog) {
    return;
  }
  ++s_window.frames;
  s_window.totalBreakSum += totalBreaks;
  s_window.totalBreakMax = std::max(s_window.totalBreakMax, totalBreaks);
  for (int i = 0; i < AURORA_PERF_BREAK_COUNT; ++i) {
    s_window.breakSum[i] += out.passBreaks[i];
    s_window.breakMax[i] = std::max(s_window.breakMax[i], out.passBreaks[i]);
  }
  for (int i = 0; i < AURORA_PERF_COUNTER_COUNT; ++i) {
    s_window.counterSum[i] += out.counters[i];
    s_window.counterMax[i] = std::max(s_window.counterMax[i], out.counters[i]);
  }
  for (int i = 0; i < AURORA_PERF_MAX_COPY_SITES; ++i) {
    s_window.siteSum[i] += siteHits[i];
    s_window.siteMax[i] = std::max(s_window.siteMax[i], siteHits[i]);
  }
  if (s_window.frames >= LogInterval) {
    log_window();
  }
}
} // namespace aurora::perf

extern "C" const AuroraPerfFrame* aurora_perf_last_frame(void) {
  return &aurora::perf::s_published[aurora::perf::s_publishedIndex.load(std::memory_order_acquire)];
}
