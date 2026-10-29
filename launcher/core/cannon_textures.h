// SPDX-License-Identifier: MIT
//
// PrimedGun's arm-cannon texture slots on the port. A slot is applied by
// copying its files into a managed folder of the user texture pack whose name
// sorts first, because Aurora takes the first file it meets for each texture
// (case-insensitive path order), so the slot wins over any other pack file for
// the same texture, as PrimedGun's 000_PrimedGunCannon pack did in Dolphin.
//
// PrimedGun Echoes: the textures of Echoes' arm cannon, by Aurora's names (as
// MP2_DUMP_TEXTURES names them: no `_m` mipmap marker). The cannon is a model of
// its own per beam (CGunWeapon's solid model): the first nine textures are on
// every beam's cannon, the others on one beam's only. Left out: the suit arm's
// textures and the cannon's reflection map, tex1_64x64_2eb482271cd2bc4c_14,
// which world objects use too (docs/PRIMEDGUN.md "Cannon texture slots").

#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace PrimedGunLauncher::Cannon {

constexpr std::array<std::string_view, 23> kTextureNames = {
    "tex1_64x64_642e777b467c55b0_14",    // the band where the arm goes in
    "tex1_64x8_8d6f00b5d395f22f_14",     // the band's two glowing stripes
    "tex1_64x64_38ee0f92b65d44f7_14",
    "tex1_128x128_af8ceb8f74fa2048_14",
    "tex1_128x128_bec6d78ea7dd739e_14",  // Metroid Prime's "Cannon texture B" too
    "tex1_8x8_f31df448bb96f9e2_14",
    "tex1_8x8_3a038381a6d99bc9_0",
    "tex1_8x8_8ccaa740b6342439_0",
    "tex1_64x64_8491a3cd135c2670_14",    // the shine, over the body
    "tex1_256x256_5607ec62ec28d99d_14",
    "tex1_16x16_c29c6ac44a1eea38_14",
    "tex1_8x64_9fa941b46459adc5_14",
    "tex1_256x256_f25b513bf95b45f7_14",
    "tex1_32x256_0eb08aa0692d1837_14",
    "tex1_32x256_b4b7ccaa1146b62f_14",
    "tex1_32x32_f0d21841aa6e3a84_14",
    "tex1_64x64_960d6073b77400bd_14",
    "tex1_256x256_b4b337024b1924ab_14",
    "tex1_32x256_0ba7b3fc6769569a_14",
    "tex1_32x256_aab755b28bb2ff26_14",
    "tex1_256x256_8afc770aee15820f_14",
    "tex1_32x256_6070467fdcffbae4_14",
    "tex1_32x256_b4fbbc827929c285_14",
};
constexpr std::array<std::string_view, kTextureNames.size()> kTextureLabels = {
    "All beams: collar band",
    "All beams: collar stripes",
    "All beams: collar trim",
    "All beams: part A (128x128)",
    "All beams: part B (128x128)",
    "All beams: detail (8x8)",
    "All beams: ramp A (8x8)",
    "All beams: ramp B (8x8)",
    "Shine mask",
    "Power Beam: body",
    "Power Beam: detail (16x16)",
    "Power Beam: strip (8x64)",
    "Dark Beam: body",
    "Dark Beam: strip A",
    "Dark Beam: strip B",
    "Dark Beam: detail (32x32)",
    "Dark Beam: detail (64x64)",
    "Light Beam: body",
    "Light Beam: strip A",
    "Light Beam: strip B",
    "Annihilator Beam: body",
    "Annihilator Beam: strip A",
    "Annihilator Beam: strip B",
};
constexpr int kTextureCount = static_cast<int>(kTextureNames.size());
constexpr int kShineIndex = 8;
constexpr int kSlotCount = 6; // Default, Slot 1-4, Custom
constexpr int kCustomSlot = 5;
constexpr std::string_view kPackFolder = "000_primedgun_cannon";
// platform/port_textures.cpp kDeviceDirs.
constexpr std::array<std::string_view, 6> kDeviceFolders = {"xbox",     "playstation", "switch",
                                                            "gamecube", "standard",    "keyboard"};

std::string SlotName(int slot);

struct Folders {
  std::filesystem::path library;        // <user>/primedgun/cannon_textures
  std::filesystem::path shippedLibrary; // <exe>/primedgun/cannon_textures
  std::filesystem::path userTextures;   // <user>/user_textures, or MP_USER_TEXTURES
};

// Copies the shipped files the user library lacks; never overwrites.
void SeedLibrary(const Folders& folders);

std::filesystem::path SlotFolder(const Folders& folders, int slot);
// `<slot>/<texture>.dds` or `.png`, whichever the slot holds; empty when it
// has neither.
std::filesystem::path ResolveSource(const Folders& folders, int slot, int index);
std::filesystem::path DefaultPreview(const Folders& folders, int index);
std::filesystem::path RemoveShinePreset(const Folders& folders);
std::filesystem::path RestoreShinePreset(const Folders& folders, int slot);

// Where the managed folder goes: the pack root, or every device folder when the
// pack is split by device (only the active device's folder loads then).
std::vector<std::filesystem::path> PackFolders(const Folders& folders);

// Removes every managed folder; slot 0 (Default) is just this.
bool ClearPack(const Folders& folders, std::string& error);
// Clears the pack and copies the slot's files into it (the textures it has).
bool ApplySlot(const Folders& folders, int slot, std::string& error);
// Copies `source` into the slot as texture `index`, replacing what it had.
// Returns the file written.
std::filesystem::path ImportIntoSlot(const Folders& folders, int slot, int index,
                                     const std::filesystem::path& source, std::string& error);
// Keeps the slot's shine mask in presets/restore_shine/slot_N (once), then
// puts the remove-shine preset in its place. Returns the file written.
std::filesystem::path RemoveShine(const Folders& folders, int slot, std::string& error);
// Puts the kept shine mask back. Returns the file written.
std::filesystem::path RestoreShine(const Folders& folders, int slot, std::string& error);

} // namespace PrimedGunLauncher::Cannon
