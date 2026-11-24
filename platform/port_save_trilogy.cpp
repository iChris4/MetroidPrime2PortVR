#include "port_save_trilogy.h"

#include "port_bytes.h"
#include "port_crc32.h"

#include <algorithm>
#include <cstring>

namespace PortSave::Trilogy {
namespace {

constexpr size_t kEntry = 0x11;
constexpr size_t kSummary[kFiles] = {0x64, 0x2BC, 0x514};
constexpr size_t kOwner = 0x3F;
constexpr size_t kHash = 0x76C;
constexpr uint32_t kHashValue = 0x4F54DF87;
constexpr size_t kCrcEnd = 0x1980C;
constexpr uint32_t kFormat = 0x030D1624;

double ToDouble(uint64_t bits) {
  double d;
  std::memcpy(&d, &bits, sizeof d);
  return d;
}
uint64_t FromDouble(double d) {
  uint64_t bits;
  std::memcpy(&bits, &d, sizeof bits);
  return bits;
}

} // namespace

uint32_t Checksum(const uint8_t* bin) { return port::Crc32Register(0xFFFFFFFFu, bin + 4, kCrcEnd - 4); }

bool Validate(const uint8_t* bin, size_t size, std::string& error) {
  if (size != kSize) {
    error = "save.bin is " + std::to_string(size) + " bytes, not " + std::to_string(kSize);
    return false;
  }
  if (port::ReadBE32(bin + 4) != kFormat) {
    error = "save.bin has an unknown format id";
    return false;
  }
  if (port::ReadBE32(bin) != Checksum(bin)) {
    error = "save.bin fails its checksum";
    return false;
  }
  return true;
}

bool FileUsed(const uint8_t* bin, int file) {
  if (file < 0 || file >= int(kFiles)) {
    return false;
  }
  const uint8_t* e = bin + kEntry + 7 * size_t(file);
  return e[0] != 0 && e[4] != 0;
}

bool FileOwned(const uint8_t* bin, int file) {
  if (file < 0 || file >= int(kFiles)) {
    return false;
  }
  const uint8_t* id = bin + kSummary[file] + kOwner;
  return std::any_of(id, id + 8, [](uint8_t b) { return b != 0xFF; });
}

int TargetFile(const uint8_t* bin) {
  const uint32_t current = port::ReadBE32(bin + 8);
  if (current < kFiles && FileOwned(bin, int(current))) {
    return int(current);
  }
  for (int i = 0; i < int(kFiles); ++i) {
    if (FileOwned(bin, i)) {
      return i;
    }
  }
  return -1;
}

bool HasEchoes(const uint8_t* bin, int file) {
  if (file < 0 || file >= int(kFiles)) {
    return false;
  }
  const uint8_t* m = bin + kSummary[file] + kSummaryEchoes;
  return port::ReadBE32(m + 4) != 0xFFFFFFFFu && port::ReadBE32(m + 24) == 3;
}

bool Read(const uint8_t* bin, int file, Payload& out) {
  if (file < 0 || file >= int(kFiles) || !FileUsed(bin, file) || !HasEchoes(bin, file)) {
    return false;
  }
  const uint8_t* slot = bin + kSlotBase + kSlotSize * size_t(file);
  std::memcpy(out.partA, slot, kPartSize);
  std::memcpy(out.partB, slot + kPartSize, kPartSize);
  const uint8_t* m = bin + kSummary[file] + kSummaryEchoes;
  out.summary.difficulty = port::ReadBE32(m);
  out.summary.percent = port::ReadBE32(m + 8);
  out.summary.playTime = ToDouble(port::ReadBE64(m + 12));
  out.summary.tanks = port::ReadBE32(m + 20);
  out.summary.mlvl = port::ReadBE32(m + 35);
  return true;
}

bool Write(const std::vector<uint8_t>& base, int file, const Payload& payload, std::vector<uint8_t>& out,
           std::string& error) {
  if (!Validate(base.data(), base.size(), error)) {
    error = "the Trilogy save to write into: " + error;
    return false;
  }
  if (file < 0 || file >= int(kFiles)) {
    error = "no Trilogy file to write into";
    return false;
  }
  out = base;
  uint8_t* d = out.data();
  uint8_t* slot = d + kSlotBase + kSlotSize * size_t(file);
  uint8_t* sum = d + kSummary[file];
  uint8_t* entry = d + kEntry + 7 * size_t(file);
  if (!FileUsed(d, file)) {
    // A used file is the model for an empty one; the owner id (in the summary, which the MP1 copy
    // would cover) stays the file's own.
    int model = -1;
    for (int i = 0; i < int(kFiles) && model < 0; ++i) {
      if (i != file && FileUsed(d, i)) {
        model = i;
      }
    }
    if (model >= 0) {
      std::memcpy(entry, d + kEntry + 7 * size_t(model), 7);
    } else {
      static constexpr uint8_t kNew[7] = {1, 1, 0, 0, 1, 0, 0}; // a real PAL save's
      std::memcpy(entry, kNew, 7);
    }
    std::memset(slot, 0, kSlotSize);
  }
  std::memcpy(slot, payload.partA, kPartSize);
  std::memcpy(slot + kPartSize, payload.partB, kPartSize);

  // The summary line: difficulty, a u32 (9 for Echoes), the item percentage, the play time (a
  // double), the energy tanks, a count of 3 and three bytes behind it, a u32, the world, and two
  // trailing bytes. The bytes of unknown meaning stay as the file had them.
  uint8_t* m = sum + kSummaryEchoes;
  const bool had = HasEchoes(d, file);
  uint8_t keep[5] = {0, 1, 1, 0, 1};
  uint32_t second = 9;
  if (had) {
    std::memcpy(keep, m + 28, 3);
    std::memcpy(keep + 3, m + 39, 2);
    second = port::ReadBE32(m + 4);
  }
  std::memset(m, 0, kSummaryBytes);
  port::WriteBE32(m, payload.summary.difficulty);
  port::WriteBE32(m + 4, second);
  port::WriteBE32(m + 8, payload.summary.percent);
  port::WriteBE64(m + 12, FromDouble(payload.summary.playTime));
  port::WriteBE32(m + 20, payload.summary.tanks);
  port::WriteBE32(m + 24, 3);
  std::memcpy(m + 28, keep, 3);
  port::WriteBE32(m + 35, payload.summary.mlvl);
  std::memcpy(m + 39, keep + 3, 2);

  if (port::ReadBE32(d + kHash) == 0) {
    port::WriteBE32(d + kHash, kHashValue);
  }
  port::WriteBE32(d, Checksum(d));
  return true;
}

} // namespace PortSave::Trilogy
