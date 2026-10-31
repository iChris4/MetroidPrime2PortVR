#pragma once

#include <cstddef>

// Dark/Light ammo above the retail 250, kept beside the card (#27, "Beam ammo per expansion").
//
// The save format is retail's: CPlayerState::PutTo writes each amount and capacity in the bit
// width of the retail maximum, so a value above 250 cannot be stored there and is written clamped.
// The real amounts go into a sidecar, `beam_ammo.sidecar` in the folder above the card (the
// file has no business inside Aurora's GCI folder). A record is keyed by the FNV-1a hash of the
// save's own bytes, so it applies to exactly the save that wrote it: a deleted slot's record, a
// slot another save has overwritten, and a copy made by another tool (which has other bytes only
// if it changed them; identical bytes are the same save) all either match or are ignored. A
// missing or unreadable sidecar is retail behaviour: the save loads at the clamped amounts.
namespace PortBeamAmmo {

struct Amounts {
  int darkAmount = 0;
  int darkCapacity = 0;
  int lightAmount = 0;
  int lightCapacity = 0;
};

// Keeps `amounts` for the save bytes `data`. `persist` writes the file (card saves and save states);
// without it the record lives in memory only (checkpoints, slot buffers the game rewrites often).
void Remember(const void* data, size_t size, const Amounts& amounts, bool persist);

// The amounts recorded for exactly these bytes, if any.
bool Find(const void* data, size_t size, Amounts& amounts);

} // namespace PortBeamAmmo
