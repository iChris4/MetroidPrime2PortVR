// G2ME <-> G2MP save conversion (platform/port_save_convert.cpp) on a real G2ME save
// (tests/data/01-G2ME-MetroidPrime2.gci, one slot, written by the port with the legacy signature).
//
// The round trip ME -> MP -> ME must give the same file apart from the signature and the CRC of
// each copy: the sample carries the legacy signature 37d0412e and the converter writes the
// console one (44bfc715), so those two words are the documented difference.
#include "port_save_convert.h"
#include "Kyoto/CCrc32.hpp"
#include "port_bytes.h"
#include "port_test.h"

#include <fstream>
#include <iterator>

using namespace PortSave::Convert;

#ifndef SAVE_FIXTURE
#error SAVE_FIXTURE must name the sample .gci
#endif

static bool InSigOrCrc(size_t i) {
  for (size_t copy : {size_t(0x2040), size_t(0x4040)}) {
    if (i >= copy && i < copy + 4) return true;         // CRC
    if (i >= copy + 8 && i < copy + 12) return true;    // signature, first word of the user data
  }
  return false;
}

int main() {
  std::ifstream f(SAVE_FIXTURE, std::ios::binary);
  std::vector<uint8_t> in((std::istreambuf_iterator<char>(f)), {});
  REQUIRE_MSG(in.size() == 24640, "fixture missing or wrong size");

  CHECK(CanConvert("G2ME", "G2MP"));
  CHECK(CanConvert("G2MP", "G2ME"));
  CHECK(!CanConvert("G2ME", "G2ME"));
  CHECK(!CanConvert("R3ME", "G2ME"));
  CHECK(!CanConvert("G2ME", "R3ME"));

  std::vector<uint8_t> mp, back;
  Report r1, r2;
  std::string err;
  REQUIRE_MSG(Gci(in, "G2MP", mp, r1, err), err);
  CHECK_EQ(r1.slots, 1);
  CHECK(!r1.Lossy());
  CHECK_EQ(mp.size(), in.size());
  CHECK(std::string(reinterpret_cast<const char*>(&mp[0]), 4) == "G2MP");
  CHECK(mp != in);

  REQUIRE_MSG(Gci(mp, "G2ME", back, r2, err), err);
  CHECK_EQ(r2.slots, 1);
  CHECK(!r2.Lossy());
  REQUIRE(back.size() == in.size());
  size_t stray = 0;
  for (size_t i = 0; i < in.size(); ++i)
    if (in[i] != back[i] && !InSigOrCrc(i)) ++stray;
  CHECK_EQ(stray, size_t(0));

  // Each release's console signature: G2MP01 seeds it with 'EURP', not 'USA>' (#95; d13a209f is a
  // real PAL console save's).
  CHECK_EQ(port::ReadBE32(&mp[0x2048]), 0xd13a209fu);
  CHECK_EQ(port::ReadBE32(&back[0x2048]), 0x44bfc715u);

  // A PAL save from a port build before #95 (USA seed, c13c33f1) still converts.
  std::vector<uint8_t> oldPal = mp;
  for (size_t copy : {size_t(0x2040), size_t(0x4040)}) {
    const bool big = port::ReadBE32(&oldPal[copy]) ==
                     CCRC32::Calculate(&oldPal[copy + 4], 0x2000 - 4, 0xFFFFFFFFu);
    port::WriteBE32(&oldPal[copy + 8], 0xc13c33f1u);
    const uint32_t crc = CCRC32::Calculate(&oldPal[copy + 4], 0x2000 - 4, 0xFFFFFFFFu);
    big ? port::WriteBE32(&oldPal[copy], crc) : port::WriteLE32(&oldPal[copy], crc);
  }
  std::vector<uint8_t> fromOld;
  Report r3;
  REQUIRE_MSG(Gci(oldPal, "G2ME", fromOld, r3, err), err);
  CHECK(fromOld == back);

  // The payload really changed in between: not just a relabelled copy.
  size_t changed = 0;
  for (size_t i = 0x2040 + 12; i < 0x4040; ++i) changed += in[i] != mp[i];
  CHECK(changed > 0);

  // Refused or damaged input fails with a reason instead of producing a file.
  std::vector<uint8_t> out;
  Report r;
  CHECK(!Gci(in, "R3ME", out, r, err));
  CHECK(!Gci(in, "G2ME", out, r, err));  // same release: nothing to convert
  std::vector<uint8_t> shortIn(in.begin(), in.begin() + 0x2040);
  CHECK(!Gci(shortIn, "G2MP", out, r, err));
  std::vector<uint8_t> bad = in;
  for (size_t copy : {size_t(0x2040), size_t(0x4040)}) bad[copy + 8 + 12] ^= 0xff;  // SAVH tag
  CHECK(!Gci(bad, "G2MP", out, r, err));
  CHECK(!err.empty());

  return PORT_TEST_RESULT();
}
