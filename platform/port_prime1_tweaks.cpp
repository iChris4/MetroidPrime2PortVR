// Prime 1's Boost Ball values from the player's own Prime 1 disc (port_prime1_tweaks.h): the disc
// is opened as a second image beside the game's (aurora_disc_image_*), Tweaks.pak read whole and
// the "Ball" CTWK found, inflated if the PAK compressed it, and parsed.

#include "port_prime1_tweaks.h"

#include "port_overlay.h"

#include <aurora/dvd.h>
#include <zlib.h>

#include <cctype>
#include <cstring>
#include <mutex>
#include <vector>

namespace PortPrime1Tweaks {
namespace {

// Prime 1's Tweaks.pak is under 100 KB; a file past this is not it, and is not read into memory.
constexpr int64_t kMaxPakBytes = 16 * 1024 * 1024;

bool ReadImageFile(AuroraDiscImage* image, uint32_t index, std::vector< uint8_t >& out) {
  void* file = aurora_disc_image_file_open(image, index);
  if (file == nullptr) {
    return false;
  }
  const int64_t size = aurora_dvd_base_seek(file, 0, 2);
  bool ok = size > 0 && size <= kMaxPakBytes && aurora_dvd_base_seek(file, 0, 0) == 0;
  if (ok) {
    out.resize(static_cast< size_t >(size));
    size_t done = 0;
    while (ok && done < out.size()) {
      const int64_t got = aurora_dvd_base_read(file, out.data() + done, out.size() - done);
      ok = got > 0;
      done += ok ? static_cast< size_t >(got) : 0;
    }
  }
  aurora_dvd_base_close(file);
  return ok;
}

// A file of the disc by name, ignoring case as Aurora's own lookup does (Prime 1's GameCube disc
// spells it Tweaks.Pak); the .pak names seen go into the error when it is not found.
struct Find {
  const char* name;
  int64_t index = -1;
  std::string paks;
};

bool EqualsIgnoreCase(const char* a, const char* b) {
  for (; *a != '\0' && *b != '\0'; ++a, ++b) {
    if (std::tolower(static_cast< unsigned char >(*a)) != std::tolower(static_cast< unsigned char >(*b))) {
      return false;
    }
  }
  return *a == *b;
}

bool EndsWithPak(const char* name) {
  const size_t length = std::strlen(name);
  return length >= 4 && EqualsIgnoreCase(name + length - 4, ".pak");
}

} // namespace

bool LoadFromDisc(const std::string& path, BoostBall& out, std::string& source, std::string& error) {
  char gameId[6] = {};
  uint8_t discNumber = 0;
  uint8_t discVersion = 0;
  AuroraDiscImage* image = aurora_disc_image_open(path.c_str(), gameId, &discNumber, &discVersion);
  if (image == nullptr) {
    error = "cannot open " + path;
    return false;
  }
  const std::string id(gameId, 6);
  std::vector< uint8_t > pak;
  bool ok = false;
  if (id.compare(0, 3, "GM8") != 0) {
    error = id + " is not a GameCube Metroid Prime disc (GM8E01, GM8P01 or GM8J01)";
  } else {
    Find tweaks{"Tweaks.pak"};
    aurora_disc_image_list(
        image,
        [](u32 index, const char* name, u32, void* user) {
          auto* find = static_cast< Find* >(user);
          if (name == nullptr) {
            return;
          }
          if (find->index < 0 && EqualsIgnoreCase(name, find->name)) {
            find->index = index;
          } else if (EndsWithPak(name) && find->paks.size() < 200) {
            find->paks += find->paks.empty() ? "" : ", ";
            find->paks += name;
          }
        },
        &tweaks);
    if (tweaks.index < 0) {
      error = id + " has no Tweaks.pak" +
              (tweaks.paks.empty() ? std::string(" (no .pak files listed)") : " (has " + tweaks.paks + ")");
    } else if (!ReadImageFile(image, static_cast< uint32_t >(tweaks.index), pak)) {
      error = "cannot read Tweaks.pak from " + id;
    } else {
      ok = true;
    }
  }
  aurora_disc_image_close(image);
  if (!ok) {
    return false;
  }

  PakEntry entry;
  if (!FindPakResource(pak.data(), pak.size(), "CTWK", "Ball", entry, error)) {
    return false;
  }
  const uint8_t* body = pak.data() + entry.offset;
  size_t bodySize = entry.size;
  std::vector< uint8_t > inflated;
  if (entry.compressed) {
    // A compressed resource is its inflated size, then a zlib stream.
    if (bodySize < 4) {
      error = "the Ball tweak is truncated";
      return false;
    }
    // The size is the file's word: one ParseBall would refuse is refused before it is allocated.
    uLongf length = ReadBE32(body);
    if (!BallSizeOk(length)) {
      error = "the Ball tweak inflates to " + std::to_string(length) + " bytes, not Prime 1's " +
              std::to_string(kBallBytes);
      return false;
    }
    inflated.resize(length);
    if (uncompress(inflated.data(), &length, body + 4, static_cast< uLong >(bodySize - 4)) != Z_OK) {
      error = "the Ball tweak does not inflate";
      return false;
    }
    body = inflated.data();
    bodySize = length;
  }
  if (!ParseBall(body, bodySize, out, error)) {
    return false;
  }
  source = id + " rev " + std::to_string(discVersion);
  return true;
}

namespace {
std::mutex sMutex;
std::string sLoadedPath;
bool sAttempted = false;
bool sValid = false;
BoostBall sValues;
std::string sStatus = "no Prime 1 disc set";

// Loads the setting's disc once per path; a new path loads again.
void Refresh() {
  const std::string path = PortOverlay::Prime1DiscPath();
  if (sAttempted && path == sLoadedPath) {
    return;
  }
  sAttempted = true;
  sLoadedPath = path;
  sValid = false;
  if (path.empty()) {
    sStatus = "no Prime 1 disc set";
    return;
  }
  std::string source;
  std::string error;
  sValid = LoadFromDisc(path, sValues, source, error);
  sStatus = sValid ? source : error;
}
} // namespace

bool Values(BoostBall& out) {
  std::lock_guard< std::mutex > lock(sMutex);
  Refresh();
  if (sValid) {
    out = sValues;
  }
  return sValid;
}

void Reload() {
  std::lock_guard< std::mutex > lock(sMutex);
  sAttempted = false;
}

std::string Status() {
  std::lock_guard< std::mutex > lock(sMutex);
  Refresh();
  return sStatus;
}

} // namespace PortPrime1Tweaks
