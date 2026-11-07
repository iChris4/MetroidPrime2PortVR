// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
namespace PortCannonRepair {
struct Resource {
  uint32_t type;
  std::vector<uint8_t> data;
};
using DiscReader = std::function<bool(uint32_t, std::vector<uint8_t>&)>;
constexpr uint32_t kModel = 0x19ae626c, kSkin = 0x7281cdba, kSkeleton = 0x46ed31e0;
struct Result {
  Resource model, skin;
  size_t triangles = 0, seamVertices = 0;
};
// Built from the user's disc: clip the good -X side, reflect it, weld the seam and remap bones.
// Fingerprints exclude replacement models/skins/skeletons. No game geometry is bundled.
bool Build(const std::vector< uint8_t >& model, const std::vector< uint8_t >& skin,
           const std::vector< uint8_t >& skeleton, Result& result);
// Rigid suit arms retain their original animated elbow-locator attachment.
bool BuildArm(uint32_t id, const std::vector< uint8_t >& model, Result& result);
bool IsResource(uint32_t id);
const Resource* Find(uint32_t id, const DiscReader& read);
// Read and repair the currently mounted Echoes PAK resources when VR is enabled.
const Resource* FindForGame(uint32_t id);
void Reset();
} // namespace PortCannonRepair
