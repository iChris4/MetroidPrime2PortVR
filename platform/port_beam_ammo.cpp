#include "port_beam_ammo.h"

#include "port_save.h"
#include "port_utf8_path.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace PortBeamAmmo {
namespace {

struct Record {
  uint64_t hash = 0;
  Amounts amounts;
  bool persist = false;
};

constexpr size_t kMaxMemory = 64;
constexpr size_t kMaxFile = 16;

std::mutex sMutex;
std::vector< Record > sRecords; // oldest first
bool sLoaded = false;

uint64_t Hash(const void* data, size_t size) {
  const unsigned char* bytes = static_cast< const unsigned char* >(data);
  uint64_t hash = 1469598103934665603ull;
  for (size_t i = 0; i < size; ++i) {
    hash = (hash ^ bytes[i]) * 1099511628211ull;
  }
  return hash ^ (static_cast< uint64_t >(size) << 56);
}

// `<card folder>/../beam_ammo.sidecar` (`<user>/USA/` for the default card); empty when there is no
// card yet, in which case the records live in memory only.
std::string FilePath() {
  std::string card = PortSave::Directory();
  if (card.empty()) {
    card = PortSave::ImagePath();
    if (card.empty()) {
      return {};
    }
    return (std::filesystem::path(PortUtf8::FromUtf8(card)).parent_path() / "beam_ammo.sidecar")
        .string();
  }
  while (card.size() > 1 && (card.back() == '/' || card.back() == '\\')) {
    card.pop_back();
  }
  return (std::filesystem::path(PortUtf8::FromUtf8(card)).parent_path() / "beam_ammo.sidecar")
      .string();
}

void Load() {
  if (sLoaded) {
    return;
  }
  const std::string path = FilePath();
  if (path.empty()) {
    return; // try again once the card exists
  }
  sLoaded = true;
  std::ifstream in(PortUtf8::FromUtf8(path));
  std::string line;
  while (std::getline(in, line)) {
    unsigned long long hash = 0;
    Record record;
    if (std::sscanf(line.c_str(), "%llx %d %d %d %d", &hash, &record.amounts.darkAmount,
                    &record.amounts.darkCapacity, &record.amounts.lightAmount,
                    &record.amounts.lightCapacity) == 5) {
      record.hash = hash;
      record.persist = true;
      sRecords.push_back(record);
    }
  }
}

void Save() {
  const std::string path = FilePath();
  if (path.empty()) {
    return;
  }
  std::vector< Record > keep;
  for (const Record& record : sRecords) {
    if (record.persist) {
      keep.push_back(record);
    }
  }
  if (keep.size() > kMaxFile) {
    keep.erase(keep.begin(), keep.end() - kMaxFile);
  }
  const std::string temp = path + ".tmp";
  {
    std::ofstream out(PortUtf8::FromUtf8(temp), std::ios::trunc);
    if (!out) {
      return;
    }
    char text[128];
    for (const Record& record : keep) {
      std::snprintf(text, sizeof(text), "%016llx %d %d %d %d\n",
                    static_cast< unsigned long long >(record.hash), record.amounts.darkAmount,
                    record.amounts.darkCapacity, record.amounts.lightAmount,
                    record.amounts.lightCapacity);
      out << text;
    }
  }
  std::error_code ec;
  std::filesystem::rename(PortUtf8::FromUtf8(temp), PortUtf8::FromUtf8(path), ec);
  if (ec) {
    std::filesystem::remove(PortUtf8::FromUtf8(temp), ec);
  }
}

} // namespace

void Remember(const void* data, size_t size, const Amounts& amounts, bool persist) {
  std::lock_guard< std::mutex > lock(sMutex);
  Load();
  const uint64_t hash = Hash(data, size);
  sRecords.erase(std::remove_if(sRecords.begin(), sRecords.end(),
                                [hash](const Record& r) { return r.hash == hash; }),
                 sRecords.end());
  Record record;
  record.hash = hash;
  record.amounts = amounts;
  record.persist = persist;
  sRecords.push_back(record);
  if (sRecords.size() > kMaxMemory) {
    sRecords.erase(sRecords.begin(), sRecords.begin() + (sRecords.size() - kMaxMemory));
  }
  if (persist) {
    Save();
  }
}

bool Find(const void* data, size_t size, Amounts& amounts) {
  std::lock_guard< std::mutex > lock(sMutex);
  Load();
  const uint64_t hash = Hash(data, size);
  for (const Record& record : sRecords) {
    if (record.hash == hash) {
      amounts = record.amounts;
      return true;
    }
  }
  return false;
}

} // namespace PortBeamAmmo
