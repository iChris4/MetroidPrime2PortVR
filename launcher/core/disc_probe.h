// SPDX-License-Identifier: GPL-3.0-or-later
//
// Whether a disc image is one the game will boot: Metroid Prime 2: Echoes USA
// (G2ME01) or PAL (G2MP01), or Metroid Prime Trilogy USA (R3ME01) or PAL
// (R3MP01), revision 0, disc 0, as platform/entry.cpp IdentifyDisc takes them
// (the game checks the mounted disc again), in a format the port opens
// (platform/entry.cpp IsDiscImageName).

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace PrimedGunLauncher {

enum class DiscCheck {
  Ok,
  WrongGame,
  WrongRevision,
  UnsupportedFormat, // an extension the port does not open
  Unverified,        // a format whose header is not read here (.wbfs, .gcz); checked at boot
  Unreadable,
};

struct DiscInfo {
  DiscCheck check = DiscCheck::Unreadable;
  std::string gameId; // six characters when the header was found
  int revision = -1;
  // A PAL disc (G2MP01, R3MP01), whatever the check says: the game keeps its
  // saves on the EUR card instead of the USA one (platform/port_save.cpp
  // CardRegion).
  bool Pal() const { return gameId == "G2MP01" || gameId == "R3MP01"; }
  // Metroid Prime Trilogy (a Wii disc), whatever the check says.
  bool Trilogy() const { return gameId == "R3ME01" || gameId == "R3MP01"; }
};

// The port's own extension list: .iso .gcm .rvz .wia .gcz .ciso .wbfs (an NKit
// image ends in .iso).
bool IsSupportedDiscExtension(const std::filesystem::path& path);

DiscInfo ProbeDisc(const std::filesystem::path& path);
// The same on the first bytes of a file (at least 0x8008 for a CISO image).
DiscInfo ProbeDiscBytes(std::string_view extension, const uint8_t* data, size_t size);

} // namespace PrimedGunLauncher
