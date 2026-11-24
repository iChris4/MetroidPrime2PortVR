#pragma once

// MSB-first bit streams, as the game's CBitStreamReader / CBitStreamWriter, for the save
// converters (port_save_convert.cpp, port_save_trilogy_payload.cpp).

#include <cstddef>
#include <cstdint>
#include <vector>

namespace port {

// Reading past the end sets `bad` and yields zeros.
struct BitReader {
  const uint8_t* data;
  size_t bits;
  size_t pos = 0;
  bool bad = false;

  BitReader(const uint8_t* d, size_t byteCount) : data(d), bits(byteCount * 8) {}
  uint32_t Get(int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) {
      uint32_t bit = 0;
      if (pos >= bits) {
        bad = true;
      } else {
        bit = (data[pos >> 3] >> (7 - (pos & 7))) & 1;
      }
      v = (v << 1) | bit;
      ++pos;
    }
    return v;
  }
  void Align() { pos = (pos + 7) & ~size_t(7); }
};

struct BitWriter {
  std::vector< uint8_t > bytes;
  size_t pos = 0;

  void Put(uint32_t v, int n) {
    for (int i = n - 1; i >= 0; --i) {
      if ((pos >> 3) >= bytes.size()) {
        bytes.push_back(0);
      }
      if ((v >> i) & 1) {
        bytes[pos >> 3] |= uint8_t(0x80 >> (pos & 7));
      }
      ++pos;
    }
  }
  void Align() {
    pos = (pos + 7) & ~size_t(7);
    if ((pos >> 3) > bytes.size()) {
      bytes.resize(pos >> 3, 0);
    }
  }
};

} // namespace port
