// PortSaveLayout::SlotOffset (platform/include/port_save_layout.h): present slots are packed.
#include "port_save_layout.h"

#include <cstdio>

namespace {
int sFailures = 0;

void Expect(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++sFailures;
  }
}
} // namespace

int main() {
  using PortSaveLayout::kSlotsStart;
  using PortSaveLayout::kSlotStride;
  using PortSaveLayout::SlotOffset;
  uint8_t user[16] = {};

  // All three present: the fixed places.
  user[8] = user[9] = user[10] = 1;
  Expect(SlotOffset(user, 0) == kSlotsStart, "full card: slot 1");
  Expect(SlotOffset(user, 1) == kSlotsStart + kSlotStride, "full card: slot 2");
  Expect(SlotOffset(user, 2) == kSlotsStart + 2 * kSlotStride, "full card: slot 3");

  // Slot 2 empty: slot 3 follows slot 1.
  user[9] = 0;
  Expect(SlotOffset(user, 2) == kSlotsStart + kSlotStride, "slot 2 empty: slot 3 after slot 1");

  // Only slot 3: it is first.
  user[8] = 0;
  Expect(SlotOffset(user, 2) == kSlotsStart, "only slot 3: at the start");

  // Three slots fit in a copy's 0x1ff8 bytes.
  Expect(kSlotsStart + 3 * kSlotStride <= 0x1ff8, "three slots fit");

  if (sFailures == 0) {
    std::printf("port_save_layout: all passed\n");
  }
  return sFailures == 0 ? 0 : 1;
}
