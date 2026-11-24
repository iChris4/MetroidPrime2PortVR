// Suit colour customisation's sources and cache (port_suit_colours.h): reads each
// suit's retail textures from the base disc's PAKs, recolours them to the picks,
// and keeps them in <user folder>/generated/suit-colours/<suit>/ under a stamp of
// the generator version, the sources and the picks.
//
// How the texture ids were found (USA disc, G2ME01; no names are on the disc, so
// the PAK tables' resource names and the models' own material lists were used):
//  - The first-person arm cannon is CMDL 4AD9BBAC (SamusGun); the Dark and Light
//    suits draw their left arms from LeftArm_Dark_CMDL 078B71CB and
//    LeftArm_Light_CMDL BACC46C8 (CGrappleArm.cpp kSuitModels). The morph balls
//    are SamusBallCMDL F57D2B74 and the Dark/Light ball models and transitions.
//    The pause menu's Inventory Varia Suit model (LogBook.pak) has its own sheets.
//  - A CMDL lists its textures in its first material set: after the 12-byte
//    header and the 24-byte bounds come the section and material counts, the
//    section sizes padded to 32 bytes, then a u32 count and that many TXTR ids.
//  - Each texture was decoded and its texels' hue histograms taken; the textures
//    that carry the suit's colour are listed below. Glows, lenses and grey metal
//    (Varia 4AEE8588, 5E9D4A06, 0E398F0B, 2FCFEA7D, 8C878F6A, 9486384E), and the
//    Light arm's AA2D0CCF (not CMPR) are left out. A shared texture (the Dark
//    balls share a few) is simply served under its one id: PortMods patches it
//    into every PAK that holds it.

#include "port_suit_colours.h"
#include "port_utf8_path.h"

#include <algorithm>
#include <Kyoto/Streams/CLZOSupport.hpp>
#include <aurora/dvd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace PortSuitColours {
namespace {

constexpr uint32_t kTXTR = 0x54585452;
constexpr const char* kStampName = "stamp.txt";

std::string FileName(uint32_t id) {
  char name[16];
  std::snprintf(name, sizeof(name), "%08X.TXTR", id);
  return name;
}

std::string PathString(const fs::path& path) {
  const auto u8 = path.u8string();
  return std::string(u8.begin(), u8.end());
}

constexpr int kVaria = 0, kDark = 1, kLight = 2;

// Each suit's colour-carrying diffuse textures: the arm cannon (or left arm),
// then the morph balls (and their transitions).
std::vector<uint32_t> Sources(int suit) {
  switch (suit) {
  case kVaria:
    return {0x4CC24B16, 0x65E6F516, 0x86F76718, 0xCEA098FE, 0x2777D332, 0xEB669581, 0x9D214FBC,
            // The pause menu's Inventory/Armor Varia Suit model (LogBook.pak, its SCAN's
            // dependency list): the armour sheets E8AF7174 and FFD9D212 and the green
            // visor/plate textures 788432da and C2DE6C2A. Its grey metal, face and
            // gradient textures stay.
            0xE8AF7174, 0xFFD9D212, 0x788432DA, 0xC2DE6C2A};
  case kDark:
    return {0x80A7CF0D, 0xB34CE6A0, 0xD31B3FB2, 0x33CE9EE5, 0x52E13615, 0x9255BD83, 0xD0041B18, 0x719F387E,
            0x59C822A0, 0x8CF8043C, 0xF85301C6, 0x07E032B3, 0x7F5781D8, 0xDD1473FC, 0x091ACC33, 0xFF45E6C4};
  case kLight:
    return {0x087D0789, 0x0C32C6DA, 0xA240A5D3, 0xC9B164A1, 0x0662A1C7, 0x091A32D6, 0xC6A37961, 0x8A598DD2,
            0x95E79836};
  default: return {};
  }
}

// Whether the suit's body slot applies to this texture. The Light Suit's body
// is silver and white, and its other textures (glow blob 0662A1C7, ring 087D0789,
// lights 95E79836, trim C6A37961, the small white C9B164A1) are glows, lenses and
// trim whose neutral texels must stay: only the four with the armour plates and
// arm are body. The other suits have no body slot.
bool BodyTexture(int suit, uint32_t id) {
  if (suit != kLight) {
    return true;
  }
  return id == 0x091A32D6 || id == 0x0C32C6DA || id == 0x8A598DD2 || id == 0xA240A5D3;
}

// Every resource of the base disc's PAKs, read on demand.
class DiscReader {
public:
  DiscReader() {
    const int32_t baseCount = aurora_dvd_base_entry_count();
    for (const auto& [entry, path] : PortMods::DiscPaks()) {
      if (entry < 0 || entry >= baseCount) {
        continue;
      }
      void* handle = aurora_dvd_base_open(entry);
      if (handle == nullptr) {
        continue;
      }
      m_handles[entry] = handle;
      std::vector<uint8_t> header;
      PortMods::PakTable table;
      size_t needed = 0x10000;
      bool parsed = false;
      while (needed <= (64u << 20)) {
        header.resize(needed);
        const size_t got = ReadAt(handle, 0, header.data(), header.size());
        if (PortMods::ParsePakTable(header.data(), got, table, needed)) {
          parsed = true;
          break;
        }
        if (got < header.size() || needed <= header.size()) {
          break;
        }
      }
      if (!parsed) {
        continue;
      }
      for (const PortMods::PakResource& res : table.resources) {
        if (res.type == kTXTR) {
          m_where.emplace(res.id, Where{entry, res.offset, res.size, res.compressed != 0});
        }
      }
    }
  }
  DiscReader(const DiscReader&) = delete;
  DiscReader& operator=(const DiscReader&) = delete;
  ~DiscReader() {
    for (auto& [entry, handle] : m_handles) {
      aurora_dvd_base_close(handle);
    }
  }

  // The size and offset of a TXTR in its PAK (for the stamp); 0 when absent.
  uint64_t Signature(uint32_t id) const {
    const auto found = m_where.find(id);
    return found == m_where.end() ? 0 : (uint64_t(found->second.size) << 32) | found->second.offset;
  }

  // The TXTR, decompressed. False when no PAK has it or it cannot be read.
  bool Read(uint32_t id, std::vector<uint8_t>& out) {
    const auto found = m_where.find(id);
    if (found == m_where.end()) {
      return false;
    }
    const Where& where = found->second;
    std::vector<uint8_t> raw(where.size);
    if (ReadAt(m_handles[where.entry], where.offset, raw.data(), raw.size()) != raw.size()) {
      return false;
    }
    if (!where.compressed) {
      out = std::move(raw);
      return true;
    }
    // Retro's chunked LZO (CResFactory::SLoadingData::PumpDecompression): a
    // big-endian decompressed length, then chunks, each a big-endian s16 size
    // (negative: that many bytes stored as they are) and its data.
    if (raw.size() < 6) {
      return false;
    }
    const uint32_t length = (uint32_t(raw[0]) << 24) | (uint32_t(raw[1]) << 16) | (uint32_t(raw[2]) << 8) | raw[3];
    if (length > (64u << 20)) {
      return false;
    }
    out.assign(length, 0);
    size_t in = 4;
    uint32_t done = 0;
    while (done < length) {
      if (in + 2 > raw.size()) {
        return false;
      }
      const int16_t size = int16_t((uint16_t(raw[in]) << 8) | raw[in + 1]);
      in += 2;
      if (size < 0) {
        const size_t stored = size_t(-int(size));
        if (in + stored > raw.size() || done + stored > length) {
          return false;
        }
        std::copy_n(raw.begin() + in, stored, out.begin() + done);
        in += stored;
        done += uint32_t(stored);
      } else {
        if (in + size > raw.size()) {
          return false;
        }
        uint32_t outSize = length - done;
        CLZOSupport::Inflate(raw.data() + in, uint32_t(size), out.data() + done, outSize);
        if (outSize == 0) {
          return false;
        }
        in += size;
        done += outSize;
      }
    }
    return true;
  }

private:
  struct Where {
    int32_t entry = -1;
    uint32_t offset = 0;
    uint32_t size = 0;
    bool compressed = false;
  };

  static size_t ReadAt(void* handle, uint64_t offset, uint8_t* out, size_t size) {
    if (aurora_dvd_base_seek(handle, int64_t(offset), 0) != int64_t(offset)) {
      return 0;
    }
    size_t done = 0;
    while (done < size) {
      const int64_t got = aurora_dvd_base_read(handle, out + done, size - done);
      if (got <= 0) {
        break;
      }
      done += size_t(got);
    }
    return done;
  }

  std::map<int32_t, void*> m_handles;
  std::map<uint32_t, Where> m_where;
};

std::string ReadText(std::ifstream& in) {
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

std::vector<PortMods::LooseResource> Collect(
    int suit, const Picks& picks, const std::map<std::pair<uint32_t, uint32_t>, PortMods::LooseResource>& loose,
    const std::string& userPath) {
  const std::string picksKey = PicksKey(suit, picks);
  if (picksKey.empty() || userPath.empty()) {
    return {};
  }
  const Suit& info = SuitInfo(suit);

  // What to make: a mod's own texture stays.
  std::vector<uint32_t> jobs;
  for (const uint32_t id : Sources(suit)) {
    if (loose.count({kTXTR, id}) != 0) {
      std::fprintf(stderr, "suit colours: %s: a mod has its own %s; left as it is\n", info.name, FileName(id).c_str());
      continue;
    }
    jobs.push_back(id);
  }
  if (jobs.empty()) {
    return {};
  }

  DiscReader disc;
  const fs::path dir = PortUtf8::FromUtf8(userPath) / "generated" / "suit-colours" / info.key;
  std::string stamp = std::string("suit-colours ") + std::to_string(kVersion) + " " + info.key + " " + picksKey;
  for (const uint32_t id : jobs) {
    stamp += " " + FileName(id) + "@" + std::to_string(disc.Signature(id));
  }
  stamp += "\n";
  std::error_code ec;
  const auto cached = [&]() {
    std::vector<PortMods::LooseResource> found;
    for (const uint32_t id : jobs) {
      const fs::path file = dir / FileName(id);
      const uintmax_t size = fs::file_size(file, ec);
      if (ec || size == 0) {
        continue;  // unchanged or could not be made: logged when it was tried
      }
      found.push_back({kTXTR, id, PathString(file), size, "generated"});
    }
    return found;
  };
  {
    std::ifstream in(dir / kStampName);
    if (in.is_open() && ReadText(in) == stamp) {
      std::fprintf(stderr, "suit colours: %s from the cache in %s\n", info.name, PathString(dir).c_str());
      return cached();
    }
  }

  fs::remove(dir / kStampName, ec);
  for (const uint32_t id : jobs) {
    fs::remove(dir / FileName(id), ec);
  }
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> outputs;
  for (const uint32_t id : jobs) {
    std::vector<uint8_t> txtr;
    Image image;
    if (!disc.Read(id, txtr) || !DecodeCmprTxtr(txtr, image)) {
      std::fprintf(stderr, "suit colours: %s: %s is missing or not CMPR; left as it is\n", info.name,
                   FileName(id).c_str());
      continue;
    }
    const size_t changed = Recolour(image, suit, picks, BodyTexture(suit, id));
    std::fprintf(stderr, "suit colours: %s: %s: %zu of %d texels recoloured\n", info.name, FileName(id).c_str(),
                 changed, image.width * image.height);
    if (changed == 0) {
      continue;  // nothing of the suit's colour in it: the retail texture stays
    }
    const uint32_t mips = (uint32_t(txtr[8]) << 24) | (uint32_t(txtr[9]) << 16) | (uint32_t(txtr[10]) << 8) | txtr[11];
    outputs.emplace_back(id, EncodeCmprTxtr(image, mips));
  }
  fs::create_directories(dir, ec);
  // Each file goes in whole (written aside, then renamed), and the stamp last, so
  // an interrupted write leaves no stamp and is made again.
  const auto put = [&](const std::string& name, const char* data, size_t size) {
    const fs::path file = dir / name;
    fs::path temporary = file;
    temporary += ".tmp";
    {
      std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
      out.write(data, std::streamsize(size));
      out.close();
      if (!out) {
        fs::remove(temporary, ec);
        std::fprintf(stderr, "suit colours: cannot write %s\n", PathString(file).c_str());
        return false;
      }
    }
    fs::rename(temporary, file, ec);
    if (ec) {
      fs::remove(temporary, ec);
      std::fprintf(stderr, "suit colours: cannot write %s\n", PathString(file).c_str());
      return false;
    }
    return true;
  };
  for (const auto& [id, data] : outputs) {
    if (!put(FileName(id), reinterpret_cast<const char*>(data.data()), data.size())) {
      return {};
    }
  }
  if (!put(kStampName, stamp.data(), stamp.size())) {
    return {};
  }
  std::fprintf(stderr, "suit colours: %s made in %s\n", info.name, PathString(dir).c_str());
  return cached();
}

}  // namespace PortSuitColours
