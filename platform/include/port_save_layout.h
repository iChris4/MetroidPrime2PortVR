#pragma once

// Where a save slot's bytes are in a copy of the card's save data (after the copy's CRC and
// generation). The game writes only the present slots, one after another
// (CMemoryCardDriver::BuildSaveBuffer), and reads them back the same way (ReadFinished): an empty
// slot takes no room, so slot 2 of a card whose slot 1 is empty starts where slot 1 would have.
// Reading each slot at its own fixed place picks up the wrong bytes (or none) on such a card.
#include <cstddef>
#include <cstdint>

namespace PortSaveLayout {
// The header (signature, index, three present flags, SAVH), the system options, one game-options
// block per slot and the global options come first.
constexpr size_t kSlotsStart = 15 + 192 + 3 * 32 + 32;
constexpr size_t kSlotStride = 0xa38; // one SGameFileSlot
constexpr size_t kPresentFlags = 8;   // user[8 + slot] != 0: the slot has a save

inline bool Present(const uint8_t* user, int slot) { return user[kPresentFlags + slot] != 0; }

// The offset of `slot` (0..2) in `user`: past every present slot before it.
inline size_t SlotOffset(const uint8_t* user, int slot) {
  size_t at = kSlotsStart;
  for (int i = 0; i < slot; ++i) {
    if (Present(user, i)) {
      at += kSlotStride;
    }
  }
  return at;
}
} // namespace PortSaveLayout
