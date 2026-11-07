// SPDX-License-Identifier: GPL-3.0-or-later
#include "port_cannon_repair.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

void Check(bool value, const char* what) {
  if (!value) {
    std::fprintf(stderr, "cannon mirror: %s\n", what);
    std::abort();
  }
}
std::vector< uint8_t > Read(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  return {(std::istreambuf_iterator< char >(f)), {}};
}
int main(int argc, char** argv) {
  using namespace PortCannonRepair;
  Result result;
  Check(!Build({}, {}, {}, result), "reject missing/replacement resources");
  Check(result.model.data.empty() && result.skin.data.empty(), "failure returns no partial pair");
  Check(IsResource(kModel) && IsResource(kSkin) && !IsResource(kSkeleton),
        "cannon pair intercepted without replacing skeleton");
  Check(IsResource(0x4ad9bbac), "rigid suit arm resources intercepted");
  Check(!BuildArm(0x4ad9bbac, {}, result), "reject missing/replacement arm mesh");
  if (argc < 2)
    return 0;
  const std::filesystem::path folder = argv[1];
  const auto m = Read(folder / "19AE626C.CMDL"), s = Read(folder / "7281CDBA.CSKR"),
             b = Read(folder / "46ED31E0.CINF");
  Check(Build(m, s, b, result), "rebuild canonical local retail assets");
  Check(result.triangles > 1800 && result.triangles < 2100, "full half replacement triangle count");
  Check(result.seamVertices > 10, "shared centreline vertices");
  // The original has one bone per vertex. Rebuilding must not average opposite
  // hinge bones into a synthetic blend that welds opening panels together.
  auto word = [&](size_t at) {
    const auto& d = result.skin.data;
    return uint32_t(d.at(at)) << 24 | uint32_t(d.at(at + 1)) << 16 | uint32_t(d.at(at + 2)) << 8 |
           d.at(at + 3);
  };
  size_t skinAt = 4;
  auto modelWord = [&](size_t at) {
    const auto& d = result.model.data;
    return uint32_t(d.at(at)) << 24 | uint32_t(d.at(at + 1)) << 16 | uint32_t(d.at(at + 2)) << 8 |
           d.at(at + 3);
  };
  size_t positionsAt = (44 + modelWord(36) * 4 + 31) & ~size_t(31);
  for (size_t i = 0; i < modelWord(40); ++i)
    positionsAt += modelWord(44 + i * 4);
  size_t firstVertex = 0;
  using WingPositions = std::map< uint32_t, std::set< std::array< uint32_t, 3 > > >;
  WingPositions rebuiltWings;
  bool leftNozzleCrosses = false, rightNozzleCrosses = false;
  for (size_t i = 0; i < word(0); ++i) {
    Check(word(skinAt) == 1, "independent hinge bone influences, no left/right averaging");
    Check(word(skinAt + 8) == 0x3f800000, "original full bone weight preserved");
    const auto bone = word(skinAt + 4), count = word(skinAt + 12);
    for (size_t j = 0; j < count; ++j) {
      const auto bits = modelWord(positionsAt + (firstVertex + j) * 12);
      float x;
      std::memcpy(&x, &bits, 4);
      if (bone >= 15 && bone <= 18)
        rebuiltWings[bone].insert({bits, modelWord(positionsAt + (firstVertex + j) * 12 + 4),
                                   modelWord(positionsAt + (firstVertex + j) * 12 + 8)});
      leftNozzleCrosses |= bone == 16 && x > .0015f;
      rightNozzleCrosses |= bone == 15 && x < -.0015f;
    }
    firstVertex += count;
    skinAt += 16;
  }
  // Validate the Echoes CSKR matrix banks against every display-list corner.
  const size_t matrixCount = word(skinAt);
  Check(matrixCount % 10 == 0, "complete ten-matrix banks");
  const size_t matrixData = skinAt + 4;
  const size_t vertexCountAt = matrixData + matrixCount * 2;
  Check(word(vertexCountAt) == firstVertex, "vertex-to-bone map covers rebuilt vertices");
  const size_t vertexMap = vertexCountAt + 4;
  Check(vertexMap + firstVertex == result.skin.data.size(), "skin has no Prime 1-format trailer");
  const auto& model = result.model.data;
  const size_t partCount = modelWord(36), materialSets = modelWord(40);
  const size_t surfaceTable = materialSets + 5;
  size_t partOffset = (44 + partCount * 4 + 31) & ~size_t(31);
  std::vector<size_t> offsets;
  for (size_t i = 0; i < partCount; ++i) {
    offsets.push_back(partOffset);
    partOffset += modelWord(44 + i * 4);
  }
  Check(partOffset == model.size(), "model section sizes cover output exactly");
  const size_t surfaceCount = modelWord(offsets[surfaceTable]);
  Check(surfaceTable + 1 + surfaceCount == partCount, "surface table matches rebuilt sections");
  auto half = [](const std::vector<uint8_t>& data, size_t at) {
    return uint16_t(uint16_t(data.at(at)) << 8 | data.at(at + 1));
  };
  size_t seenTriangles = 0;
  for (size_t surface = 0; surface < surfaceCount; ++surface) {
    const size_t start = offsets[surfaceTable + 1 + surface];
    const size_t bank = half(model, start + 44);
    Check(bank < matrixCount / 10, "surface matrix bank exists");
    size_t at = start + ((0x4f + modelWord(start + 28)) & ~size_t(31));
    const size_t end = at + modelWord(start + 16);
    const size_t material = modelWord(start + 12);
    const size_t materials = offsets[0];
    size_t materialAt = materials + 4 + modelWord(materials) * 4;
    const size_t materialCount = modelWord(materialAt);
    Check(material < materialCount, "surface keeps valid material");
    materialAt += 4;
    const size_t entry = materialAt + materialCount * 4 +
        (material == 0 ? 0 : modelWord(materialAt + (material - 1) * 4));
    const uint32_t desc = modelWord(entry + 8 + modelWord(entry + 4) * 4);
    const size_t uvCount = (desc & 0xffff) == 0xf0f ? 2 : 3;
    while (at < end && model.at(at) != 0) {
      Check((model.at(at) & 0xf8) == 0x90 && half(model, at + 1) == 3,
            "rebuilt display lists contain triangles");
      at += 3;
      for (size_t corner = 0; corner < 3; ++corner) {
        const size_t matrix = model.at(at);
        Check(matrix % 3 == 0 && matrix < 30 && model.at(at + 1) == matrix,
              "position and texture matrices select the same skin slot");
        const size_t vertex = half(model, at + 2);
        Check(vertex < firstVertex && half(model, at + 4) == vertex,
              "position and normal indices cover matching welded vertices");
        const size_t group = half(result.skin.data, matrixData + (bank * 10 + matrix / 3) * 2);
        Check(group == result.skin.data.at(vertexMap + vertex),
              "GPU matrix slot matches the vertex influence group");
        at += 6 + uvCount * 2;
      }
      ++seenTriangles;
    }
    Check(at <= end, "display list stays within its surface");
  }
  Check(seenTriangles == result.triangles, "all repaired triangles are reachable");
  Check(leftNozzleCrosses && rightNozzleCrosses,
        "opening nozzle components retain authored corners crossing the body mirror plane");
  auto sourceWord = [](const std::vector< uint8_t >& bytes, size_t at) {
    return uint32_t(bytes.at(at)) << 24 | uint32_t(bytes.at(at + 1)) << 16 |
           uint32_t(bytes.at(at + 2)) << 8 | bytes.at(at + 3);
  };
  size_t sourcePositions = (44 + sourceWord(m, 36) * 4 + 31) & ~size_t(31);
  for (size_t i = 0; i < sourceWord(m, 40); ++i)
    sourcePositions += sourceWord(m, 44 + i * 4);
  size_t sourceSkin = 4, sourceVertex = 0;
  WingPositions originalWings;
  for (size_t i = 0; i < sourceWord(s, 0); ++i) {
    Check(sourceWord(s, sourceSkin) == 1, "original wing fixture has one bone per vertex");
    const auto bone = sourceWord(s, sourceSkin + 4), count = sourceWord(s, sourceSkin + 12);
    if (bone >= 15 && bone <= 18)
      for (size_t j = 0; j < count; ++j) {
        const size_t at = sourcePositions + (sourceVertex + j) * 12;
        originalWings[bone].insert(
            {sourceWord(m, at), sourceWord(m, at + 4), sourceWord(m, at + 8)});
      }
    sourceVertex += count;
    sourceSkin += 16;
  }
  Check(originalWings == rebuiltWings,
        "all four original wing shapes preserved, including asymmetric corners");
  std::printf("Mirrored cannon: %zu triangles, %zu seam vertices\n", result.triangles,
              result.seamVertices);
  auto changed = s;
  changed[8] ^= 1;
  Check(!Build(m, changed, b, result), "reject custom skin, not just custom geometry");
  Check(result.model.data.empty(), "no mismatched mesh after skin rejection");
  Reset();
  int reads = 0;
  auto reader = [&](uint32_t id, std::vector< uint8_t >& out) {
    ++reads;
    out = id == kModel ? m : id == kSkin ? s : b;
    return true;
  };
  const auto* mesh = Find(kModel, reader);
  const auto* skin = Find(kSkin, reader);
  Check(mesh && skin && reads == 3, "mesh-first loads a coherent cached pair");
  Reset();
  reads = 0;
  skin = Find(kSkin, reader);
  mesh = Find(kModel, reader);
  Check(mesh && skin && reads == 3, "skin-first loads the same coherent pair");
  Reset();
  for (const auto id : {0x4ad9bbacu}) {
    char name[32];
    std::snprintf(name, sizeof(name), "%08X.CMDL", id);
    const auto input = Read(folder / name);
    Check(BuildArm(id, input, result), "rebuild canonical suit arm");
    Check(result.skin.data.empty(), "rigid arms need no replacement bones or skin");
    Check(result.triangles > 200 && result.triangles < 500 && result.seamVertices > 0,
          "full arm half replacement");
    std::printf("Arm %08X: %zu triangles, %zu seam vertices\n", id, result.triangles,
                result.seamVertices);
    auto custom = input;
    custom[12] ^= 1;
    Check(!BuildArm(id, custom, result), "custom/replacement arm stays unchanged");
    int armReads = 0;
    const auto* arm = Find(id, [&](uint32_t requested, std::vector< uint8_t >& out) {
      Check(requested == id, "arm does not request cannon or skeleton assets");
      ++armReads;
      out = input;
      return true;
    });
    Check(arm && armReads == 1, "independent arm cache");
    Check(Find(id,
               [&](uint32_t, std::vector< uint8_t >&) {
                 std::abort();
                 return false;
               }) == arm,
          "stable cached arm");
  }
  Reset();
}
