#include "port_save_convert.h"

#include "Kyoto/CCrc32.hpp"
#include "port_bitstream.h"
#include "port_bytes.h"
#include "port_save_layout.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "port_save_convert_tables.inc"

namespace PortSave {
namespace Convert {

namespace {

// The .gci / save-data layout; the same numbers as port_save.cpp (they describe the same file).
constexpr size_t kDirEntry = 0x40;
constexpr size_t kBlock = 0x2000;
using PortSaveLayout::kSlotsStart;
using PortSaveLayout::kSlotStride;
constexpr size_t kUserDataSize = 0x1ff8;

using port::BitReader;
using port::BitWriter;

void Copy(BitReader& r, BitWriter& w, int n) {
  while (n > 0) {
    const int step = std::min(n, 32);
    w.Put(r.Get(step), step);
    n -= step;
  }
}

bool Expect(BitReader& r, BitWriter& w, uint32_t marker, const char* what, std::string& error) {
  const uint32_t got = r.Get(32);
  if (got != marker) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s marker is %08x, not %08x", what, got, marker);
    error = buf;
    return false;
  }
  w.Put(marker, 32);
  return true;
}

// Target entry t takes source entry map[t] (-1: a new entry, all clear). `dropped` counts the
// source entries nothing maps from that were set. A null map is an identical list.
bool Remap(const std::vector<uint8_t>& src, const int16_t* map, size_t targetCount,
           std::vector<uint8_t>& out, int& dropped, const char* what, std::string& error) {
  if (map == nullptr) {
    if (src.size() != targetCount) {
      error = std::string(what) + ": the lists are the same but have " + std::to_string(src.size()) +
              " and " + std::to_string(targetCount) + " entries";
      return false;
    }
    out = src;
    return true;
  }
  out.assign(targetCount, 0);
  std::vector<uint8_t> used(src.size(), 0);
  for (size_t t = 0; t < targetCount; ++t) {
    const int s = map[t];
    if (s >= 0 && size_t(s) < src.size()) {
      out[t] = src[size_t(s)];
      used[size_t(s)] = 1;
    }
  }
  for (size_t s = 0; s < src.size(); ++s) {
    if (!used[s] && src[s]) {
      ++dropped;
    }
  }
  return true;
}

void ReadBits(BitReader& r, std::vector<uint8_t>& v, size_t n) {
  v.resize(n);
  for (size_t i = 0; i < n; ++i) {
    v[i] = uint8_t(r.Get(1));
  }
}
void WriteBits(BitWriter& w, const std::vector<uint8_t>& v) {
  for (uint8_t b : v) {
    w.Put(b, 1);
  }
}

const SConvDirection* DirectionFor(const std::string& from, const std::string& to) {
  if (from == "G2ME" && to == "G2MP") {
    return &kMEtoMP;
  }
  if (from == "G2MP" && to == "G2ME") {
    return &kMPtoME;
  }
  return nullptr;
}

size_t LayerBits(const SConvWorld& w) {
  size_t n = 0;
  for (int a = 0; a < w.areas; ++a) {
    n += w.layerCounts[a] - 1;
  }
  return n;
}

size_t BodyBits(const SConvWorld& w) {
  return 64 + w.relays + 2 * size_t(w.areas) + w.doors + w.unmappable + 1 + 10 + LayerBits(w);
}

// One world record's bits: CWorldState::PutTo.
bool Body(const SConvDirection& d, int wi, BitReader& r, BitWriter& w, Report& report,
          std::string& error) {
  const SConvWorld& fw = d.from->worlds[wi];
  const SConvWorld& tw = d.to->worlds[wi];
  if (fw.areas != tw.areas) {
    error = "world " + std::to_string(wi) + " has a different area count in the two releases";
    return false;
  }
  const SConvWorldMap& wm = d.worlds[wi];
  std::vector<uint8_t> src, dst;

  Copy(r, w, 64); // area ID, desired area asset ID

  ReadBits(r, src, fw.relays);
  if (!Remap(src, wm.relays, tw.relays, dst, report.droppedRelays, "relays", error)) {
    return false;
  }
  WriteBits(w, dst);

  Copy(r, w, 2 * fw.areas); // visited, mapped

  ReadBits(r, src, fw.doors);
  if (!Remap(src, wm.doors, tw.doors, dst, report.droppedDoors, "doors", error)) {
    return false;
  }
  WriteBits(w, dst);

  ReadBits(r, src, fw.unmappable);
  if (!Remap(src, wm.unmappable, tw.unmappable, dst, report.droppedUnmappable, "unmappable", error)) {
    return false;
  }
  WriteBits(w, dst);

  Copy(r, w, 1); // map station used

  const uint32_t layerTotal = r.Get(10);
  if (layerTotal != LayerBits(fw)) {
    error = "world " + std::to_string(wi) + ": layer total " + std::to_string(layerTotal) +
            ", the release has " + std::to_string(LayerBits(fw));
    return false;
  }
  w.Put(uint32_t(LayerBits(tw)), 10);
  for (int a = 0; a < fw.areas; ++a) {
    const int fc = fw.layerCounts[a];
    const int tc = tw.layerCounts[a];
    std::vector<uint8_t> in(size_t(fc), 0);
    for (int l = 1; l < fc; ++l) {
      in[size_t(l)] = uint8_t(r.Get(1));
    }
    const int8_t* map = nullptr;
    for (int i = 0; i < d.layerMapCount; ++i) {
      if (d.layerMaps[i].world == wi && d.layerMaps[i].area == a) {
        map = d.layerMaps[i].map;
        if (d.layerMaps[i].count != tc) {
          error = "layer map of world " + std::to_string(wi) + " area " + std::to_string(a) +
                  " does not match the target's layer count";
          return false;
        }
      }
    }
    std::vector<uint8_t> used(size_t(fc), 0);
    for (int l = 1; l < tc; ++l) {
      const int s = map != nullptr ? map[l] : (l < fc ? l : -1);
      if (s > 0 && s < fc) {
        w.Put(in[size_t(s)], 1);
        used[size_t(s)] = 1;
      } else {
        // A layer the source release has no counterpart of starts as the target's own default.
        w.Put(uint32_t((tw.layerDefaults[a] >> l) & 1), 1);
      }
    }
    for (int l = 1; l < fc; ++l) {
      if (!used[size_t(l)] && in[size_t(l)]) {
        ++report.droppedLayers;
      }
    }
  }
  return true;
}

// One slot: CGameState::PutTo.
bool Slot(const SConvDirection& d, const uint8_t* in, size_t size, std::vector<uint8_t>& out,
          Report& report, std::string& error) {
  BitReader r(in, size);
  BitWriter w;

  if (!Expect(r, w, port::FourCC('G', 'M', 'S', 'T'), "GMST", error)) {
    return false;
  }
  Copy(r, w, 32 + 3); // time, hard, init powerups, dark world
  const uint32_t desired = r.Get(32);
  uint32_t mapped = desired;
  for (int i = 0; i < d.from->worldCount; ++i) {
    if (d.from->worlds[i].mlvl == desired) {
      mapped = d.worldIds[i];
    }
  }
  w.Put(mapped, 32);
  Copy(r, w, 64); // play time

  for (int p = 0; p < 4; ++p) {
    if (!Expect(r, w, port::FourCC('P', 'L', 'S', 'T'), "PLST", error)) {
      return false;
    }
    Copy(r, w, 32 + 32 + 3 + 2 + 3 + 3 + 2);
    if (!Expect(r, w, port::FourCC('P', 'R', 'S', 'T'), "PRST", error)) {
      return false;
    }
    Copy(r, w, 186);
    if (!Expect(r, w, port::FourCC('P', 'O', 'W', 'R'), "POWR", error)) {
      return false;
    }
    Copy(r, w, 4 * 2); // the four fixed scan entries
    std::vector<uint8_t> scans(size_t(d.from->scanCount));
    for (uint8_t& s : scans) {
      s = uint8_t(r.Get(2));
    }
    std::vector<uint8_t> used(scans.size(), 0);
    for (int t = 0; t < d.to->scanCount; ++t) {
      const int s = d.scans[t];
      if (s >= 0 && size_t(s) < scans.size()) {
        w.Put(scans[size_t(s)], 2);
        used[size_t(s)] = 1;
      } else {
        w.Put(0, 2);
      }
    }
    for (size_t s = 0; s < scans.size(); ++s) {
      if (!used[s] && scans[s]) {
        ++report.droppedScans;
      }
    }
    Copy(r, w, 18); // scan completion rates
    if (!Expect(r, w, port::FourCC('S', 'C', 'A', 'N'), "SCAN", error)) {
      return false;
    }
  }

  struct Hint {
    uint32_t state, time;
  };
  std::vector<Hint> hints(size_t(d.from->hintCount));
  for (Hint& h : hints) {
    h.state = r.Get(2);
    h.time = r.Get(32);
  }
  for (int t = 0; t < d.to->hintCount; ++t) {
    const int s = d.hints[t];
    const Hint h = s >= 0 && size_t(s) < hints.size() ? hints[size_t(s)] : Hint{0, 0};
    w.Put(h.state, 2);
    w.Put(h.time, 32);
  }

  if (!Expect(r, w, port::FourCC('P', 'R', 'E', 'V'), "PREV", error)) {
    return false;
  }
  Copy(r, w, 32 + 1 + 8 + 3 + 4 * (2 + 16 + 16));
  r.Align();
  w.Align();
  Copy(r, w, 32 + d.from->gameVarBits); // escape time, game environment variables
  r.Align();
  w.Align();

  const uint32_t worldCount = r.Get(8);
  if (worldCount != uint32_t(d.from->worldCount)) {
    error = "slot holds " + std::to_string(worldCount) + " worlds, the release has " +
            std::to_string(d.from->worldCount);
    return false;
  }
  w.Put(worldCount, 8);
  for (int i = 0; i < d.from->worldCount; ++i) {
    const uint32_t id = r.Get(32);
    const uint32_t bitCount = r.Get(16);
    if (r.bad || id != d.from->worlds[i].mlvl) {
      error = "world record " + std::to_string(i) + " has ID " + std::to_string(id) +
              ", not the release's";
      return false;
    }
    if (bitCount != BodyBits(d.from->worlds[i])) {
      error = "world record " + std::to_string(i) + " is " + std::to_string(bitCount) +
              " bits, the release's layout is " + std::to_string(BodyBits(d.from->worlds[i]));
      return false;
    }
    const size_t start = r.pos / 8;
    const size_t bytes = (bitCount + 7) / 8;
    if (start + bytes > size) {
      error = "world record " + std::to_string(i) + " runs past the slot";
      return false;
    }
    BitReader body(in + start, bytes);
    w.Put(d.worldIds[i], 32);
    w.Put(uint32_t(BodyBits(d.to->worlds[i])), 16);
    const size_t before = w.pos;
    if (!Body(d, i, body, w, report, error)) {
      return false;
    }
    if (body.bad || body.pos != bitCount || w.pos - before != BodyBits(d.to->worlds[i])) {
      error = "world record " + std::to_string(i) + " did not transcode to its layout";
      return false;
    }
    w.Align();
    r.pos += bytes * 8;
  }
  if (!Expect(r, w, port::FourCC('G', 'M', 'N', 'D'), "GMND", error)) {
    return false;
  }
  if (r.bad) {
    error = "the slot ends before its GMND marker";
    return false;
  }
  if (w.bytes.size() > kSlotStride) {
    error = "the converted slot is " + std::to_string(w.bytes.size()) + " bytes, over " +
            std::to_string(kSlotStride);
    return false;
  }
  out = w.bytes;
  out.resize(kSlotStride, 0);
  return true;
}

uint32_t NewestTimestamp(const uint8_t* user, size_t size) {
  uint32_t newest = 0;
  for (int i = 0; i < 3; ++i) {
    const size_t at = PortSaveLayout::SlotOffset(user, i);
    if (user[8 + i] != 0 && at + 8 <= size) {
      newest = std::max(newest, port::ReadBE32(user + at + 4));
    }
  }
  return newest;
}

bool ValidHeader(const uint8_t* user, size_t size) {
  return size >= kSlotsStart + kSlotStride && std::memcmp(user + 11, "SAVH", 4) == 0;
}

} // namespace

std::string Report::Summary() const {
  std::string s = from + " -> " + to + ", " + std::to_string(slots) + " slot(s)";
  if (!Lossy()) {
    return s + ", nothing lost";
  }
  s += "; dropped (set in the source, absent from " + to + "):";
  auto add = [&s](int n, const char* what) {
    if (n != 0) {
      s += " " + std::to_string(n) + " " + what + ",";
    }
  };
  add(droppedScans, "scan(s)");
  add(droppedRelays, "relay(s)");
  add(droppedDoors, "door(s)");
  add(droppedUnmappable, "unmappable object(s)");
  add(droppedLayers, "layer(s)");
  s.pop_back();
  return s;
}

bool CanConvert(const std::string& from, const std::string& to) {
  return DirectionFor(from, to) != nullptr;
}

bool UserData(const uint8_t* in, size_t size, const std::string& from, const std::string& to,
              std::vector<uint8_t>& out, Report& report, std::string& error) {
  const SConvDirection* d = DirectionFor(from, to);
  if (d == nullptr) {
    error = "no conversion from " + from + " to " + to;
    return false;
  }
  if (!ValidHeader(in, size)) {
    error = "the save data has no SAVH header, or is too short";
    return false;
  }
  const uint32_t signature = port::ReadBE32(in);
  if (signature != d->from->signature && signature != d->from->portSignature &&
      signature != d->from->legacySignature) {
    char buf[128];
    std::snprintf(buf, sizeof buf,
                  "signature %08x is not a %s save's (%08x, or older builds' %08x or %08x)", signature,
                  from.c_str(), d->from->signature, d->from->portSignature, d->from->legacySignature);
    error = buf;
    return false;
  }

  report.from = from;
  report.to = to;
  out.assign(in, in + size);
  port::WriteBE32(out.data(), d->to->signature);
  for (int i = 0; i < 3; ++i) {
    if (in[8 + i] == 0) {
      continue;
    }
    const size_t at = PortSaveLayout::SlotOffset(in, i);
    if (at + kSlotStride > size) {
      error = "slot " + std::to_string(i + 1) + " runs past the save data";
      return false;
    }
    std::vector<uint8_t> slot;
    if (!Slot(*d, in + at, kSlotStride, slot, report, error)) {
      error = "slot " + std::to_string(i + 1) + ": " + error;
      return false;
    }
    std::memcpy(out.data() + at, slot.data(), kSlotStride);
    ++report.slots;
  }
  return true;
}

bool Gci(const std::vector<uint8_t>& in, const std::string& targetCode, std::vector<uint8_t>& out,
         Report& report, std::string& error) {
  if (in.size() < kDirEntry + 3 * kBlock || (in.size() - kDirEntry) % kBlock != 0) {
    error = "not a save file (size is not a directory entry and whole 0x2000 blocks)";
    return false;
  }
  const size_t blocks = (in.size() - kDirEntry) / kBlock;
  if ((blocks - 1) % 2 != 0) {
    error = "not a header block and two copies";
    return false;
  }
  const size_t copyBytes = (blocks - 1) / 2 * kBlock;
  const std::string from(reinterpret_cast<const char*>(in.data()), 4);
  if (std::memcmp(in.data() + 4, "01", 2) != 0) {
    error = "maker code is not 01";
    return false;
  }
  if (!CanConvert(from, targetCode)) {
    error = "no conversion from " + from + " to " + targetCode;
    return false;
  }

  struct SCopy {
    bool ok = false;
    bool little = false;
    uint32_t generation = 0;
    uint32_t newest = 0;
  } copies[2];
  for (int i = 0; i < 2; ++i) {
    const uint8_t* block = in.data() + kDirEntry + kBlock + size_t(i) * copyBytes;
    const uint32_t crc = CCRC32::Calculate(block + 4, uint(copyBytes - 4), 0xFFFFFFFFu);
    const bool little = crc == port::ReadLE32(block);
    const bool big = crc == port::ReadBE32(block);
    const size_t user = std::min(copyBytes - 8, kUserDataSize);
    if ((!little && !big) || !ValidHeader(block + 8, user)) {
      continue;
    }
    copies[i].ok = true;
    copies[i].little = little;
    copies[i].generation = little ? port::ReadLE32(block + 4) : port::ReadBE32(block + 4);
    copies[i].newest = NewestTimestamp(block + 8, user);
  }
  int chosen = -1;
  for (int i = 0; i < 2; ++i) {
    if (copies[i].ok && (chosen < 0 || copies[i].newest > copies[chosen].newest)) {
      chosen = i;
    }
  }
  if (chosen < 0) {
    error = "no readable save copy (both copies have a bad CRC or header)";
    return false;
  }

  const uint8_t* src = in.data() + kDirEntry + kBlock + size_t(chosen) * copyBytes;
  const size_t user = std::min(copyBytes - 8, kUserDataSize);
  std::vector<uint8_t> converted;
  if (!UserData(src + 8, user, from, targetCode, converted, report, error)) {
    return false;
  }

  out = in;
  std::memcpy(out.data(), targetCode.data(), 4);
  for (int i = 0; i < 2; ++i) {
    uint8_t* block = out.data() + kDirEntry + kBlock + size_t(i) * copyBytes;
    std::memcpy(block, src, copyBytes); // the chosen copy's generation, trailing bytes and all
    std::memcpy(block + 8, converted.data(), user);
    const bool little = copies[chosen].little;
    (little ? port::WriteLE32 : port::WriteBE32)(block + 4, copies[chosen].generation);
    const uint32_t crc = CCRC32::Calculate(block + 4, uint(copyBytes - 4), 0xFFFFFFFFu);
    (little ? port::WriteLE32 : port::WriteBE32)(block, crc);
  }
  return true;
}

} // namespace Convert
} // namespace PortSave
