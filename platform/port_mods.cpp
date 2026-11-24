// Mods folder startup (port_mods.h): finds the mods, works out which disc files
// they replace and which PAKs their loose resources patch, and registers the
// result with Aurora's DVD overlays.

#include "port_mods.h"
#include "port_utf8_path.h"
#include "port_strings.h"

#include "port_entry.h"
#include "port_mods_disc.h"
#include "port_suit_colours.h"

#include <aurora/dvd.h>
#include <dolphin/dvd.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace PortMods {
namespace {

Status sStatus;
// The settings (settings.cfg keys mods / mods_disabled); read at startup only.
bool sEnabled = true;
std::string sDisabled;
// Registered with Aurora; filled once, so the pointers handed out stay put.
std::vector<std::shared_ptr<const VirtualFile>> sFiles;
std::vector<std::string> sOverlayNames;

std::string PathString(const fs::path& path) {
  const auto u8 = path.u8string();
  return std::string(u8.begin(), u8.end());
}

using port::EndsWith;
using port::Lower;

// --- Base disc access -------------------------------------------------------

struct BaseFile {
  void* handle = nullptr;
  int64_t pos = -1;
};

void* OpenBase(const VirtualFile& file) {
  void* handle = aurora_dvd_base_open(file.sourceEntry);
  return handle != nullptr ? new BaseFile{handle, -1} : nullptr;
}

int64_t ReadBaseAt(void* opaque, uint64_t offset, uint8_t* buffer, size_t length) {
  auto* file = static_cast<BaseFile*>(opaque);
  if (file->pos != int64_t(offset)) {
    if (aurora_dvd_base_seek(file->handle, int64_t(offset), 0) != int64_t(offset)) {
      file->pos = -1;
      return -1;
    }
    file->pos = int64_t(offset);
  }
  size_t done = 0;
  while (done < length) {
    const int64_t got = aurora_dvd_base_read(file->handle, buffer + done, length - done);
    if (got <= 0) {
      break;
    }
    done += size_t(got);
  }
  file->pos += int64_t(done);
  return int64_t(done);
}

void CloseBase(void* opaque) {
  auto* file = static_cast<BaseFile*>(opaque);
  aurora_dvd_base_close(file->handle);
  delete file;
}

const SourceIo sBaseIo{OpenBase, ReadBaseAt, CloseBase};

void* OverlayOpen(void* userData) {
  const auto& file = *static_cast<const std::shared_ptr<const VirtualFile>*>(userData);
  return new Reader(file, &sBaseIo);
}

void OverlayClose(void* handle) { delete static_cast<Reader*>(handle); }

int64_t OverlayRead(void* handle, uint8_t* buffer, size_t length) {
  return static_cast<Reader*>(handle)->Read(buffer, length);
}

int64_t OverlaySeek(void* handle, int64_t offset, int32_t whence) {
  return static_cast<Reader*>(handle)->Seek(offset, whence);
}

// The PAK's header: its first bytes, read until the table fits.
bool ReadPakHeader(const std::string& hostPath, int32_t entry, std::vector<uint8_t>& header, uint64_t& size,
                   PakTable& table) {
  VirtualFile probe;
  probe.sourceEntry = entry;
  Segment whole;
  if (!hostPath.empty()) {
    std::error_code ec;
    size = fs::file_size(PortUtf8::FromUtf8(hostPath), ec);
    if (ec) {
      return false;
    }
    whole.kind = Segment::kHost;
    whole.hostPath = hostPath;
    whole.hostSize = size;
  } else {
    void* handle = aurora_dvd_base_open(entry);
    if (handle == nullptr) {
      return false;
    }
    const int64_t end = aurora_dvd_base_seek(handle, 0, 2);
    aurora_dvd_base_close(handle);
    if (end < 0) {
      return false;
    }
    size = uint64_t(end);
    whole.kind = Segment::kSource;
  }
  whole.length = size;
  probe.size = size;
  probe.segments.push_back(whole);
  Reader reader(std::make_shared<const VirtualFile>(std::move(probe)), &sBaseIo);

  size_t want = 64 * 1024;
  for (;;) {
    want = size_t(std::min<uint64_t>(want, size));
    header.resize(want);
    reader.Seek(0, 0);
    if (reader.Read(header.data(), want) != int64_t(want)) {
      return false;
    }
    size_t needed = 0;
    if (ParsePakTable(header.data(), header.size(), table, needed)) {
      return true;
    }
    if (needed == 0 || needed > size || want >= size) {
      return false;
    }
    want = std::max(needed, want * 2);
  }
}

std::string DiscPathFor(int32_t entry) {
  char path[512];
  if (!DVDConvertEntrynumToPath(entry, path, sizeof(path))) {
    return {};
  }
  std::string text = path;
  if (text.empty() || text[0] != '/') {
    text.insert(text.begin(), '/');
  }
  return text;
}

void Message(std::string text) {
  std::fprintf(stderr, "mods: %s\n", text.c_str());
  sStatus.messages.push_back(std::move(text));
}

// A mod's own notes are not disc files, and saying so for each is noise.
bool IsDocument(const std::string& name) {
  const std::string lower = Lower(name);
  return EndsWith(lower, ".txt") || EndsWith(lower, ".md") || EndsWith(lower, ".json") ||
         EndsWith(lower, ".png") || EndsWith(lower, ".jpg") || EndsWith(lower, ".ini") || lower == "license";
}

struct Replacement {
  std::string hostPath;
  uint64_t size = 0;
  size_t mod = 0;
};

} // namespace

// Every Echoes .pak on the mounted disc, as (original entryNum, path).
std::vector<std::pair<int32_t, std::string>> DiscPaks() {
  return detail::DiscPaks(port::entry::IsTrilogy(port::entry::CurrentDiscVersion()));
}

std::string Folder(const std::string& userPath) {
  std::string dir;
  if (const char* env = std::getenv("MP2_MODS"); env != nullptr && env[0] != '\0') {
    dir = env;
  } else {
    if (userPath.empty()) {
      return {};
    }
    dir = userPath;
    if (dir.back() != '/' && dir.back() != '\\') {
      dir += '/';
    }
    dir += "mods";
  }
  return dir;
}

bool Enabled() { return sEnabled; }
void SetEnabled(bool enabled) { sEnabled = enabled; }
const std::string& Disabled() { return sDisabled; }
void SetDisabled(const std::string& list) { sDisabled = list; }

const Status& CurrentStatus() { return sStatus; }

namespace {
std::string sUserPath;
// Aurora holds pointers into sFiles while an overlay is registered.
bool sOverlayRegistered = false;
}  // namespace

void Initialize(const std::string& userPath) {
  sUserPath = userPath;
  if (sOverlayRegistered) {
    // A reload: take the old overlay off the disc first, so the scan below sees the base disc and
    // nothing points into the files about to be dropped.
    aurora_dvd_overlay_files(nullptr, 0, nullptr);
    sOverlayRegistered = false;
  }
  sStatus = {};
  sStatus.folder = Folder(userPath);
  sStatus.active = sEnabled;
  PortSuitColours::SetServed({});
  if (sStatus.folder.empty()) {
    return;
  }
  const std::vector<std::string> disabled = SplitDisabled(sDisabled);

  std::error_code ec;
  // Created on first start so the user has somewhere to drop mods.
  fs::create_directories(PortUtf8::FromUtf8(sStatus.folder), ec);
  ec.clear();
  std::vector<fs::path> modDirs;
  for (fs::directory_iterator it(PortUtf8::FromUtf8(sStatus.folder), ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = PathString(it->path().filename());
    if (!name.empty() && name[0] != '.' && it->is_directory(ec)) {
      modDirs.push_back(it->path());
    }
  }
  std::sort(modDirs.begin(), modDirs.end());

  // Keyed by disc entry and by (type, id); a later mod overwrites an earlier.
  std::map<int32_t, Replacement> replaced;
  std::map<std::string, std::pair<std::string, Replacement>> addedMovies; // by lower-case disc path
  std::map<std::pair<uint32_t, uint32_t>, LooseResource> loose;
  std::map<std::pair<uint32_t, uint32_t>, size_t> looseMod;
  for (const fs::path& modDir : modDirs) {
    ModInfo& info = sStatus.mods.emplace_back();
    info.name = PathString(modDir.filename());
    info.enabled = sStatus.active && std::find(disabled.begin(), disabled.end(), info.name) == disabled.end();
    if (!info.enabled) {
      continue;
    }
    const size_t modIndex = sStatus.mods.size() - 1;
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(modDir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
      const std::string name = PathString(it->path().filename());
      if (!name.empty() && name[0] == '.') {
        if (it->is_directory(ec)) {
          it.disable_recursion_pending();
        }
        continue;
      }
      if (it->is_regular_file(ec)) {
        files.push_back(it->path());
      }
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
      const std::string name = PathString(file.filename());
      const std::string relative = PathString(file.lexically_relative(modDir).generic_u8string());
      const uint64_t size = fs::file_size(file, ec);
      if (ec) {
        Message(info.name + ": cannot read " + relative);
        continue;
      }
      uint32_t type = 0;
      uint32_t id = 0;
      if (ParseLooseName(name, type, id)) {
        if (size == 0) {
          Message(info.name + ": " + relative + " is empty; ignored");
          continue;
        }
        loose[{type, id}] = {type, id, PathString(file), size, info.name};
        looseMod[{type, id}] = modIndex;
        continue;
      }
      const int32_t entry = DVDConvertPathToEntrynum(("/" + relative).c_str());
      DVDDir dir;
      if (entry < 0 || entry >= aurora_dvd_base_entry_count() || DVDFastOpenDir(entry, &dir)) {
        // A movie the disc lacks is added: the game opens movies by name, and
        // Trilogy's MP1 has none of the GameCube ones (the front end falls back
        // to its own only when the GameCube name is missing).
        if (EndsWith(Lower(name), ".thp") && (entry < 0 || entry >= aurora_dvd_base_entry_count()) && size > 0 &&
            size < 0xFFFFFFE0u) {
          addedMovies[Lower("/" + relative)] = {"/" + relative, {PathString(file), size, modIndex}};
          continue;
        }
        if (!IsDocument(name)) {
          Message(info.name + ": " + relative + " is not a disc file; ignored");
        }
        continue;
      }
      if (size >= 0xFFFFFFE0u) {
        Message(info.name + ": " + relative + " is too big; ignored");
        continue;
      }
      replaced[entry] = {PathString(file), size, modIndex};
    }
  }

  // The suits' recoloured textures (port_suit_colours.h): under every mod, with
  // mods on or off, and a texture a mod brings stays the mod's.
  {
    std::array<std::string, PortSuitColours::kSuitCount> served;
    for (int suit = 0; suit < PortSuitColours::kSuitCount; ++suit) {
      const PortSuitColours::Picks picks = PortSuitColours::SuitPicks(suit);
      served[suit] = PortSuitColours::PicksKey(suit, picks);
      for (PortMods::LooseResource& resource : PortSuitColours::Collect(suit, picks, loose, userPath)) {
        loose.emplace(std::make_pair(resource.type, resource.id), std::move(resource));
      }
    }
    PortSuitColours::SetServed(served);
  }

  // Which loose resources each PAK takes.
  std::set<std::pair<uint32_t, uint32_t>> used;
  std::map<int32_t, std::shared_ptr<VirtualFile>> patched;
  if (!loose.empty()) {
    for (const auto& [entry, path] : DiscPaks()) {
      const auto whole = replaced.find(entry);
      const std::string hostPath = whole != replaced.end() ? whole->second.hostPath : std::string();
      std::vector<uint8_t> header;
      uint64_t size = 0;
      PakTable table;
      if (!ReadPakHeader(hostPath, entry, header, size, table)) {
        Message("cannot read the table of " + path + (hostPath.empty() ? "" : " (from a mod)"));
        continue;
      }
      std::vector<const LooseResource*> take;
      std::set<std::pair<uint32_t, uint32_t>> taken;
      for (const PakResource& resource : table.resources) {
        const auto found = loose.find({resource.type, resource.id});
        if (found != loose.end() && taken.insert(found->first).second) {
          take.push_back(&found->second);
          used.insert(found->first);
        }
      }
      if (take.empty()) {
        continue;
      }
      auto file = std::make_shared<VirtualFile>(PatchPak(header, table, size, take, hostPath));
      if (file->size == 0 || file->size >= 0xFFFFFFE0u) {
        Message("cannot patch " + path);
        continue;
      }
      file->discPath = path;
      file->sourceEntry = entry;
      patched[entry] = std::move(file);
    }
    for (const auto& [key, resource] : loose) {
      if (looseMod.count(key) == 0) {
        continue;  // generated (suit colours), not a mod's
      }
      if (used.count(key) != 0) {
        ++sStatus.mods[looseMod[key]].resources;
      } else {
        Message(resource.mod + ": " + PathString(PortUtf8::FromUtf8(resource.hostPath).filename()) + " is in no PAK; ignored");
      }
    }
  }

  sFiles.clear();
  sOverlayNames.clear();
  for (const auto& [entry, replacement] : replaced) {
    ++sStatus.mods[replacement.mod].files;
    if (patched.count(entry) != 0) {
      continue;
    }
    auto file = std::make_shared<VirtualFile>();
    file->discPath = DiscPathFor(entry);
    file->sourceEntry = entry;
    file->size = replacement.size;
    Segment whole;
    whole.kind = Segment::kHost;
    whole.length = replacement.size;
    whole.hostPath = replacement.hostPath;
    whole.hostSize = replacement.size;
    if (whole.length > 0) {
      file->segments.push_back(whole);
    }
    sFiles.push_back(std::move(file));
  }
  for (const auto& [key, movie] : addedMovies) {
    const Replacement& replacement = movie.second;
    ++sStatus.mods[replacement.mod].files;
    auto file = std::make_shared<VirtualFile>();
    file->discPath = movie.first;
    file->size = replacement.size;
    Segment whole;
    whole.kind = Segment::kHost;
    whole.length = replacement.size;
    whole.hostPath = replacement.hostPath;
    whole.hostSize = replacement.size;
    file->segments.push_back(whole);
    sFiles.push_back(std::move(file));
  }
  for (auto& [entry, file] : patched) {
    sFiles.push_back(std::move(file));
  }
  if (sFiles.empty()) {
    if (!sStatus.mods.empty()) {
      std::fprintf(stderr, "mods: nothing to apply from %s\n", sStatus.folder.c_str());
    }
    return;
  }

  std::vector<AuroraOverlayFile> overlays;
  overlays.reserve(sFiles.size());
  for (const auto& file : sFiles) {
    overlays.push_back({file->discPath.c_str(), const_cast<std::shared_ptr<const VirtualFile>*>(&file),
                        size_t(file->size)});
  }
  static const AuroraOverlayCallbacks callbacks{OverlayOpen, OverlayClose, OverlayRead, OverlaySeek};
  aurora_dvd_overlay_callbacks(&callbacks);
  std::vector<s32> entries(overlays.size(), -1);
  aurora_dvd_overlay_files(overlays.data(), overlays.size(), entries.data());
  sOverlayRegistered = true;
  for (size_t i = 0; i < overlays.size(); ++i) {
    if (entries[i] < 0) {
      Message("Aurora refused " + sFiles[i]->discPath);
    } else {
      ++sStatus.overlays;
    }
  }
  for (const ModInfo& mod : sStatus.mods) {
    if (mod.enabled) {
      std::fprintf(stderr, "mods: %s: %d file(s), %d resource(s)\n", mod.name.c_str(), mod.files, mod.resources);
    }
  }
  std::fprintf(stderr, "mods: %d disc file(s) served from %s\n", sStatus.overlays, sStatus.folder.c_str());
}

// Reads the mods folder again with the current settings. The caller (CResLoader::PortReopenPaks)
// has closed every PAK, so none holds a table of the old overlay.
void Reload() { Initialize(sUserPath); }

} // namespace PortMods
