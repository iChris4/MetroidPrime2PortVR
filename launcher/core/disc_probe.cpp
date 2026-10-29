// SPDX-License-Identifier: GPL-3.0-or-later

#include "disc_probe.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace PrimedGunLauncher {
namespace {

// WIA and RVZ: a 0x48-byte file header, then the disc struct whose first 16
// bytes (type, compression, level, chunk size) precede a copy of the disc's
// own 0x80-byte header.
constexpr size_t kWiaDiscHeaderOffset = 0x48 + 0x10;
// CISO: a 0x8000-byte header (magic, block size, block map), then the blocks
// in order; the disc header is at the start of block 0 when the map has it.
constexpr size_t kCisoDataOffset = 0x8000;
constexpr size_t kProbeSize = kCisoDataOffset + 8;

std::string LowerExtension(const std::filesystem::path& path) {
  const std::u8string u8 = path.extension().u8string();
  std::string ext(u8.begin(), u8.end());
  for (char& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return ext;
}

DiscInfo FromHeader(const uint8_t* header) {
  DiscInfo info;
  info.gameId.assign(reinterpret_cast<const char*>(header), 6);
  info.revision = header[7];
  // Keep the launcher in step with platform/entry.cpp IdentifyDisc: the four
  // game ids, revision 0, disc 0.
  if ((info.gameId != "G2ME01" && info.gameId != "G2MP01" && info.gameId != "R3ME01" &&
       info.gameId != "R3MP01") ||
      header[6] != 0) {
    info.check = DiscCheck::WrongGame;
  } else if (header[7] != 0) {
    info.check = DiscCheck::WrongRevision;
  } else {
    info.check = DiscCheck::Ok;
  }
  return info;
}

} // namespace

bool IsSupportedDiscExtension(const std::filesystem::path& path) {
  static const char* const kExtensions[] = {".iso", ".gcm", ".rvz", ".wia", ".gcz", ".ciso", ".wbfs"};
  const std::string ext = LowerExtension(path);
  for (const char* candidate : kExtensions) {
    if (ext == candidate) {
      return true;
    }
  }
  return false;
}

DiscInfo ProbeDiscBytes(std::string_view extension, const uint8_t* data, size_t size) {
  DiscInfo unreadable;
  static const uint8_t kRvzMagic[4] = {'R', 'V', 'Z', 0x01};
  static const uint8_t kWiaMagic[4] = {'W', 'I', 'A', 0x01};
  static const uint8_t kGczMagic[4] = {0x01, 0xC0, 0x0B, 0xB1};
  if (size >= 4 && (std::memcmp(data, kRvzMagic, 4) == 0 || std::memcmp(data, kWiaMagic, 4) == 0)) {
    return size >= kWiaDiscHeaderOffset + 8 ? FromHeader(data + kWiaDiscHeaderOffset) : unreadable;
  }
  if (size >= 4 && std::memcmp(data, "CISO", 4) == 0) {
    // data[8] is block 0's entry in the map.
    if (size < kProbeSize || data[8] == 0) {
      return unreadable;
    }
    return FromHeader(data + kCisoDataOffset);
  }
  std::string ext(extension);
  for (char& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  // GCZ: compressed blocks; the disc header is not read here, so the game checks it once mounted.
  if (ext == ".gcz" || (size >= 4 && std::memcmp(data, kGczMagic, 4) == 0)) {
    DiscInfo info;
    info.check = DiscCheck::Unverified;
    return info;
  }
  // WBFS, known by its magic or by its name in any case (the Quest launcher
  // passes a picked document's extension as the name has it). Disc slot 0's
  // header copy starts at the second hard-disk sector (2^data[8] bytes); when
  // the slot is used and that copy is within reach it is checked, else the
  // image is taken on trust and checked once mounted.
  const bool wbfsMagic = size >= 4 && std::memcmp(data, "WBFS", 4) == 0;
  if (ext == ".wbfs" || wbfsMagic) {
    if (wbfsMagic && size > 12 && data[8] >= 9 && data[8] <= 14 && data[12] != 0) {
      const size_t header = size_t{1} << data[8];
      if (header + 8 <= size) {
        return FromHeader(data + header);
      }
    }
    DiscInfo info;
    info.check = DiscCheck::Unverified;
    return info;
  }
  return size >= 8 ? FromHeader(data) : unreadable;
}

DiscInfo ProbeDisc(const std::filesystem::path& path) {
  if (!IsSupportedDiscExtension(path)) {
    DiscInfo info;
    info.check = DiscCheck::UnsupportedFormat;
    return info;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  std::vector<uint8_t> bytes(kProbeSize);
  in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  const size_t got = static_cast<size_t>(in.gcount());
  return ProbeDiscBytes(LowerExtension(path), bytes.data(), got);
}

} // namespace PrimedGunLauncher
