// SPDX-License-Identifier: GPL-3.0-or-later
#include "port_cannon_repair.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>

namespace PortCannonRepair {
namespace {
using Bytes = std::vector< uint8_t >;
using Weights = std::map< uint32_t, float >;
struct Vec {
  float x = 0, y = 0, z = 0;
  Vec operator+(Vec b) const { return {x + b.x, y + b.y, z + b.z}; }
  Vec operator-(Vec b) const { return {x - b.x, y - b.y, z - b.z}; }
  Vec operator*(float f) const { return {x * f, y * f, z * f}; }
};
float Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec Cross(Vec a, Vec b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec Normalize(Vec a) {
  const float len = std::sqrt(Dot(a, a));
  return len > 1.e-12f ? a * (1.f / len) : Vec{0, 0, 1};
}
uint32_t U32(const Bytes& d, size_t p) {
  if (p > d.size() || d.size() - p < 4)
    throw std::runtime_error("cannon resource bounds");
  return uint32_t(d[p]) << 24 | uint32_t(d[p + 1]) << 16 | uint32_t(d[p + 2]) << 8 | d[p + 3];
}
uint16_t U16(const Bytes& d, size_t p) {
  if (p > d.size() || d.size() - p < 2)
    throw std::runtime_error("cannon index bounds");
  return uint16_t(d[p]) << 8 | d[p + 1];
}
float F32(const Bytes& d, size_t p) {
  const auto w = U32(d, p);
  float f;
  std::memcpy(&f, &w, 4);
  return f;
}
void Put32(Bytes& d, size_t p, uint32_t v) {
  if (p > d.size() || d.size() - p < 4)
    throw std::runtime_error("cannon output bounds");
  for (unsigned i = 0; i < 4; ++i)
    d[p + i] = uint8_t(v >> (24 - i * 8));
}
void Add32(Bytes& d, uint32_t v) {
  const size_t p = d.size();
  d.resize(p + 4);
  Put32(d, p, v);
}
void Add16(Bytes& d, size_t v) {
  if (v >= 65536)
    throw std::runtime_error("cannon vertex limit");
  d.push_back(uint8_t(v >> 8));
  d.push_back(uint8_t(v));
}
void AddFloat(Bytes& d, float f) {
  uint32_t w;
  std::memcpy(&w, &f, 4);
  Add32(d, w);
}
void AddVec(Bytes& d, Vec v) {
  AddFloat(d, v.x);
  AddFloat(d, v.y);
  AddFloat(d, v.z);
}
void Pad(Bytes& d) { d.resize((d.size() + 31) & ~size_t(31), 0); }
uint64_t Hash(const Bytes& d) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (auto b : d)
    h = (h ^ b) * UINT64_C(1099511628211);
  return h;
}
uint32_t MirrorBone(uint32_t bone) {
  if (bone >= 15 && bone <= 18) return 15 + ((bone - 15) ^ 1);
  if (bone >= 23 && bone <= 32) return bone + 13;
  if (bone >= 36 && bone <= 45) return bone - 13;
  if (bone == 34 || bone == 35) return bone + 12;
  if (bone == 46 || bone == 47) return bone - 12;
  return bone;
}
Weights BlendWeights(const Weights& a, const Weights& b, float t) {
  Weights out;
  for (const auto& w : a)
    out[w.first] += w.second * (1 - t);
  for (const auto& w : b)
    out[w.first] += w.second * t;
  float sum = 0;
  for (auto it = out.begin(); it != out.end();) {
    if (it->second < 1.e-6f)
      it = out.erase(it);
    else {
      sum += it->second;
      ++it;
    }
  }
  if (out.empty() || out.size() > 4)
    throw std::runtime_error("cannon skin influences");
  for (auto& w : out)
    w.second /= sum;
  return out;
}
Weights ReflectWeights(const Weights& in) {
  Weights out;
  for (const auto& w : in)
    out[MirrorBone(w.first)] += w.second;
  return out;
}
int MovingSide(uint32_t bone) {
  if (bone == 16 || bone == 18 || (bone >= 23 && bone <= 32) || bone == 34 || bone == 35)
    return -1;
  if (bone == 15 || bone == 17 || (bone >= 36 && bone <= 45) || bone == 46 || bone == 47)
    return 1;
  return 0;
}
struct Vertex {
  Vec p, n;
  std::array< std::array< float, 2 >, 3 > uv{};
  Weights weights;
};
bool OriginalWing(const Weights& weights) {
  return weights.size() == 1 && weights.begin()->first >= 15 && weights.begin()->first <= 18;
}
void Seam(Vertex& v) {
  // Join fixed/centre pieces only. Opposite nozzle/panel bones must remain independent:
  // averaging their weights ties together the very flaps a beam-change animation opens.
  v.p.x = 0;
  if (v.weights == ReflectWeights(v.weights)) {
    v.n.x = 0;
    v.n = Normalize(v.n);
  }
}
Vertex Interpolate(const Vertex& a, const Vertex& b, float t) {
  Vertex v;
  v.p = a.p * (1 - t) + b.p * t;
  v.n = Normalize(a.n * (1 - t) + b.n * t);
  for (unsigned i = 0; i < 3; ++i)
    for (unsigned j = 0; j < 2; ++j)
      v.uv[i][j] = a.uv[i][j] * (1 - t) + b.uv[i][j] * t;
  v.weights = BlendWeights(a.weights, b.weights, t);
  Seam(v);
  return v;
}
struct Triangle {
  std::array< Vertex, 3 > v;
  size_t surface;
  uint8_t format;
};
using Key = std::vector< int64_t >;
Key WeightKey(const Weights& w) {
  Key k;
  for (const auto& a : w) {
    k.push_back(a.first);
    k.push_back(std::llround(a.second * 1000000));
  }
  return k;
}
Key VertexKey(const Vertex& v) {
  Key k = WeightKey(v.weights);
  k.push_back(-1);
  for (float f : {v.p.x, v.p.y, v.p.z, v.n.x, v.n.y, v.n.z}) {
    if (OriginalWing(v.weights)) {
      uint32_t bits;
      std::memcpy(&bits, &f, 4);
      k.push_back(bits);
    } else
      k.push_back(std::llround(f * 1000000));
  }
  return k;
}

bool Rebuild(const Bytes& model, const Bytes& skin, Result& result, bool arm = false) {
  const size_t count = U32(model, 36), matSets = U32(model, 40);
  size_t offset = (44 + count * 4 + 31) & ~size_t(31);
  std::vector< Bytes > parts;
  for (size_t i = 0; i < count; ++i) {
    const size_t len = U32(model, 44 + i * 4);
    if (offset + len > model.size())
      return false;
    parts.emplace_back(model.begin() + offset, model.begin() + offset + len);
    offset += len;
  }
  const auto flags = U32(model, 8);
  if ((!arm && (matSets != 5 || parts.size() != 24 || flags != 5)) ||
      (arm && (matSets != 3 || parts.size() != 10 || flags != 6)))
    return false;
  const size_t posPart = matSets, normPart = matSets + 1, uvPart = matSets + 3;
  const size_t si = matSets + 4 + ((flags & 4) != 0), ns = U32(parts[si], 0);
  if ((!arm && ns != 13) || (arm && (ns < 1 || ns > 2)))
    return false;
  std::vector< Weights > skinWeights;
  size_t sp = 4;
  for (size_t i = 0; !arm && i < U32(skin, 0); ++i) {
    const size_t n = U32(skin, sp);
    sp += 4;
    Weights w;
    for (size_t j = 0; j < n; ++j) {
      const auto b = U32(skin, sp);
      w[b] = F32(skin, sp + 4);
      sp += 8;
    }
    const size_t len = U32(skin, sp);
    sp += 4;
    skinWeights.insert(skinWeights.end(), len, w);
  }
  if (!arm && (skinWeights.size() != 1885 || parts[posPart].size() != ((1885 * 12 + 31) & ~size_t(31)) ||
               parts[normPart].size() != ((1885 * 12 + 31) & ~size_t(31))))
    return false;
  const auto& mats = parts[0];
  size_t mp = 4 + U32(mats, 0) * 4;
  const size_t nm = U32(mats, mp);
  mp += 4;
  const size_t ends = mp;
  mp += nm * 4;
  std::vector< uint32_t > descs;
  for (size_t i = 0; i < nm; ++i) {
    const size_t at = mp + (i ? U32(mats, ends + (i - 1) * 4) : 0);
    descs.push_back(U32(mats, at + 8 + U32(mats, at + 4) * 4));
  }
  // Preserve every beam/suit material set and verify its corner layout.
  for (size_t set = 1; set < matSets; ++set) {
    const auto& ms = parts[set];
    size_t at = 4 + U32(ms, 0) * 4;
    if (U32(ms, at) != nm)
      return false;
    at += 4;
    const size_t ep = at;
    at += nm * 4;
    for (size_t i = 0; i < nm; ++i) {
      const size_t m = at + (i ? U32(ms, ep + (i - 1) * 4) : 0);
      if (U32(ms, m + 8 + U32(ms, m + 4) * 4) != descs[i])
        return false;
    }
  }
  std::vector< Triangle > output;
  std::vector< size_t > uvCounts(ns);
  for (size_t surf = 0; surf < ns; ++surf) {
    const auto& data = parts[si + 1 + surf];
    const size_t mi = U32(data, 12);
    if (mi >= descs.size())
      return false;
    const auto desc = descs[mi];
    if ((arm && desc != 0xf0f && desc != 0x3f0f) ||
        (!arm && desc != 0x81000f0f && desc != 0x81003f0f))
      return false;
    const size_t nuv = (desc & 0xffff) == 0xf0f ? 2 : 3, stride = (arm ? 4 : 6) + nuv * 2;
    uvCounts[surf] = nuv;
    size_t at = (0x4f + U32(data, 28)) & ~size_t(31);
    const size_t end = at + (U32(data, 16) & 0x7fffffff);
    if (end > data.size())
      return false;
    auto load = [&](size_t corner) {
      Vertex v;
      if (!arm) corner += 2; // PNMTXIDX and TEX6MTXIDX precede the indexed attributes.
      const size_t pi = U16(data, corner), ni = U16(data, corner + 2);
      if (!arm && (pi >= skinWeights.size() || ni >= skinWeights.size() ||
                   skinWeights[pi] != skinWeights[ni]))
        throw std::runtime_error("cannon position/normal bone mismatch");
      const size_t p = pi * 12, n = ni * 12;
      v.p = {F32(parts[posPart], p), F32(parts[posPart], p + 4), F32(parts[posPart], p + 8)};
      if (arm) {
        // Retail rigid arms use signed 16-bit normals. Convert rebuilt normals to floats
        // and clear the short-normal flag below; UVs/materials retain their layouts.
        const size_t at = ni * 6;
        v.n = Normalize({float(int16_t(U16(parts[normPart], at))),
                         float(int16_t(U16(parts[normPart], at + 2))),
                         float(int16_t(U16(parts[normPart], at + 4)))});
        v.weights = {{0, 1.f}}; // Deduplication only; no CSKR is emitted for rigid arms.
      } else {
        v.n = {F32(parts[normPart], n), F32(parts[normPart], n + 4), F32(parts[normPart], n + 8)};
        v.weights = skinWeights[pi];
      }
      for (size_t j = 0; j < nuv; ++j) {
        const size_t uv = U16(data, corner + 4 + j * 2) * 8;
        v.uv[j] = {F32(parts[uvPart], uv), F32(parts[uvPart], uv + 4)};
      }
      if (std::abs(v.p.x) < 1.e-6f && v.weights == ReflectWeights(v.weights))
        Seam(v);
      return v;
    };
    auto emit = [&](size_t a, size_t b, size_t c, uint8_t format, size_t begin) {
      std::vector< Vertex > poly = {load(begin + a * stride), load(begin + b * stride),
                                    load(begin + c * stride)},
                            clipped;
      // The four nozzle wings are already complete in retail. Retain BOTH sides,
      // original corners, normals, UVs and bone assignments: no mirror or seam edit.
      if (OriginalWing(poly[0].weights)) {
        if (poly[1].weights != poly[0].weights || poly[2].weights != poly[0].weights)
          throw std::runtime_error("mixed original nozzle wing");
        const Vec cross = Cross(poly[1].p - poly[0].p, poly[2].p - poly[0].p);
        if (Dot(cross, cross) >= 1.e-18f)
          output.push_back({{poly[0], poly[1], poly[2]}, surf, format});
        return;
      }
      int movingSide = 0;
      for (const auto& v : poly)
        for (const auto& w : v.weights) {
          const int side = MovingSide(w.first);
          if (side != 0) {
            if (movingSide != 0 && movingSide != side)
              throw std::runtime_error("mixed cannon hinge sides");
            movingSide = side;
          }
        }
      // A moving panel is a complete component, not a half of the fixed body. Some
      // nozzle corners deliberately cross X=0. Cutting/welding them creates a web
      // between opening flaps; retaining the bad-side sliver duplicates its geometry.
      if (movingSide > 0)
        return;
      if (movingSide < 0)
        clipped = poly;
      // Clip crossing triangles instead of keeping whole triangles from the bad half.
      for (size_t i = 0; movingSide == 0 && i < poly.size(); ++i) {
        const auto& v = poly[i];
        const auto& prev = poly[(i + poly.size() - 1) % poly.size()];
        const bool inside = v.p.x <= 0, was = prev.p.x <= 0;
        if (inside != was)
          clipped.push_back(Interpolate(prev, v, -prev.p.x / (v.p.x - prev.p.x)));
        if (inside)
          clipped.push_back(v);
      }
      for (size_t i = 1; i + 1 < clipped.size(); ++i) {
        Triangle t{{clipped[0], clipped[i], clipped[i + 1]}, surf, format};
        const Vec cross = Cross(t.v[1].p - t.v[0].p, t.v[2].p - t.v[0].p);
        if (Dot(cross, cross) < 1.e-18f)
          continue;
        // A face wholly on the cut plane is an internal divider, not exterior skin.
        if (t.v[0].p.x == 0 && t.v[1].p.x == 0 && t.v[2].p.x == 0)
          continue;
        output.push_back(t);
        std::swap(t.v[1], t.v[2]);
        for (auto& v : t.v) {
          v.p.x = -v.p.x;
          v.n.x = -v.n.x;
          v.weights = ReflectWeights(v.weights);
        }
        output.push_back(t);
      }
    };
    while (at < end && data[at]) {
      const uint8_t op = data[at] & 0xf8;
      // The rebuilt rigid arms use float normals: VAT0, not the original VAT1 S16 layout.
      const uint8_t format = arm ? 0 : data[at] & 7;
      const size_t n = U16(data, at + 1);
      at += 3;
      if (n > (end - at) / stride)
        return false;
      if (op == 0x90 && n % 3 == 0) {
        for (size_t i = 0; i < n; i += 3)
          emit(i, i + 1, i + 2, format, at);
      } else if (op == 0x98) {
        for (size_t i = 0; i + 2 < n; ++i)
          emit(i + (i & 1), i + 1 - (i & 1), i + 2, format, at);
      } else if (op == 0xa0) {
        for (size_t i = 1; i + 1 < n; ++i)
          emit(0, i, i + 1, format, at);
      } else if (op == 0x80 && n % 4 == 0) {
        for (size_t i = 0; i < n; i += 4) {
          emit(i, i + 1, i + 2, format, at);
          emit(i, i + 2, i + 3, format, at);
        }
      } else
        return false;
      at += n * stride;
    }
  }
  // UVs are corner attributes: they do not split the welded position/normal/weight vertices.
  // Sort by influence set because CSKR stores consecutive virtual-bone runs.
  std::map< Key, Vertex > vertices;
  for (const auto& t : output)
    for (const auto& v : t.v)
      vertices.emplace(VertexKey(v), v);
  std::map< Key, size_t > indices;
  std::vector< Vertex > ordered;
  for (const auto& item : vertices) {
    indices[item.first] = ordered.size();
    ordered.push_back(item.second);
  }
  parts[posPart].clear();
  parts[normPart].clear();
  parts[uvPart].clear();
  Bytes newSkin;
  std::vector< std::pair< Weights, size_t > > groups;
  for (const auto& v : ordered) {
    AddVec(parts[posPart], v.p);
    AddVec(parts[normPart], OriginalWing(v.weights) ? v.n : Normalize(v.n));
    if (groups.empty() || WeightKey(groups.back().first) != WeightKey(v.weights))
      groups.push_back({v.weights, 0});
    ++groups.back().second;
    if (v.p.x == 0)
      ++result.seamVertices;
  }
  Add32(newSkin, uint32_t(groups.size()));
  for (const auto& g : groups) {
    Add32(newSkin, uint32_t(g.first.size()));
    for (const auto& w : g.first) {
      Add32(newSkin, w.first);
      AddFloat(newSkin, w.second);
    }
    Add32(newSkin, uint32_t(g.second));
  }
  Pad(parts[posPart]);
  Pad(parts[normPart]);
  std::map< std::array< uint32_t, 2 >, size_t > uvIndices;
  auto uvIndex = [&](const std::array< float, 2 >& uv) {
    std::array< uint32_t, 2 > key;
    std::memcpy(key.data(), uv.data(), 8);
    const auto found = uvIndices.find(key);
    if (found != uvIndices.end())
      return found->second;
    const size_t index = parts[uvPart].size() / 8;
    uvIndices[key] = index;
    AddFloat(parts[uvPart], uv[0]);
    AddFloat(parts[uvPart], uv[1]);
    return index;
  };
  // Echoes skins on the GPU. Each surface selects ten virtual bones and each
  // corner carries both position and texture matrix indices. Split a surface
  // when a mirrored set needs another bank, preserving its material and order.
  std::vector<Bytes> surfaces;
  std::vector<short> matrixIndices;
  std::map<Key, size_t> groupIndices;
  for (size_t i = 0; i < groups.size(); ++i)
    groupIndices.emplace(WeightKey(groups[i].first), i);
  for (size_t source = 0; source < ns; ++source) {
    std::vector<size_t> bank;
    Bytes list;
    auto flush = [&] {
      if (list.empty()) return;
      Bytes surface = parts[si + 1 + source];
      const size_t start = (0x4f + U32(surface, 28)) & ~size_t(31);
      surface.resize(start);
      const size_t bankId = matrixIndices.size() / 10;
      if (!arm) {
        surface[44] = uint8_t(bankId >> 8);
        surface[45] = uint8_t(bankId);
      }
      Pad(list);
      Put32(surface, 16, uint32_t(list.size()));
      surface.insert(surface.end(), list.begin(), list.end());
      surfaces.push_back(std::move(surface));
      for (size_t i = 0; i < 10; ++i)
        matrixIndices.push_back(i < bank.size() ? short(bank[i]) : -1);
      bank.clear();
      list.clear();
    };
    for (const auto& triangle : output) {
      if (triangle.surface != source) continue;
      auto needed = bank;
      for (const auto& vertex : triangle.v) {
        const size_t group = groupIndices.at(WeightKey(vertex.weights));
        if (std::find(needed.begin(), needed.end(), group) == needed.end())
          needed.push_back(group);
      }
      if (needed.size() > 10) {
        flush();
        needed.clear();
        for (const auto& vertex : triangle.v) {
          const size_t group = groupIndices.at(WeightKey(vertex.weights));
          if (std::find(needed.begin(), needed.end(), group) == needed.end())
            needed.push_back(group);
        }
      }
      bank = std::move(needed);
      list.push_back(0x90 | triangle.format);
      list.push_back(0);
      list.push_back(3);
      for (const auto& vertex : triangle.v) {
        const size_t group = groupIndices.at(WeightKey(vertex.weights));
        const auto slot = std::find(bank.begin(), bank.end(), group) - bank.begin();
        if (!arm) {
          list.push_back(uint8_t(slot * 3));
          list.push_back(uint8_t(slot * 3));
        }
        const size_t index = indices.at(VertexKey(vertex));
        Add16(list, index);
        Add16(list, index);
        for (size_t i = 0; i < uvCounts[source]; ++i)
          Add16(list, uvIndex(vertex.uv[i]));
      }
    }
    flush();
  }
  Add32(newSkin, uint32_t(matrixIndices.size()));
  for (short index : matrixIndices) {
    newSkin.push_back(uint8_t(uint16_t(index) >> 8));
    newSkin.push_back(uint8_t(index));
  }
  Add32(newSkin, uint32_t(ordered.size()));
  for (const auto& vertex : ordered) {
    const size_t group = groupIndices.at(WeightKey(vertex.weights));
    if (group > 255) return false;
    newSkin.push_back(uint8_t(group));
  }
  Pad(parts[uvPart]);
  Vec low = ordered.front().p, high = low;
  for (const auto& v : ordered) {
    low = {std::min(low.x, v.p.x), std::min(low.y, v.p.y), std::min(low.z, v.p.z)};
    high = {std::max(high.x, v.p.x), std::max(high.y, v.p.y), std::max(high.z, v.p.z)};
  }
  Bytes bounds;
  AddVec(bounds, low);
  AddVec(bounds, high);
  parts.resize(si + 1);
  parts[si].clear();
  Add32(parts[si], uint32_t(surfaces.size()));
  size_t cumulative = 0;
  for (auto& surface : surfaces) {
    cumulative += surface.size();
    Add32(parts[si], uint32_t(cumulative));
    parts.push_back(std::move(surface));
  }
  Pad(parts[si]);
  Bytes rebuilt(model.begin(), model.begin() + 44);
  Put32(rebuilt, 36, uint32_t(parts.size()));
  rebuilt.resize((44 + parts.size() * 4 + 31) & ~size_t(31), 0);
  if (arm)
    Put32(rebuilt, 8, flags & ~uint32_t(2));
  for (size_t j = 0; j < 6; ++j)
    Put32(rebuilt, 12 + j * 4, U32(bounds, j * 4));
  for (size_t i = 0; i < parts.size(); ++i) {
    Put32(rebuilt, 44 + i * 4, uint32_t(parts[i].size()));
    rebuilt.insert(rebuilt.end(), parts[i].begin(), parts[i].end());
  }
  result.model = {0x434d444c, std::move(rebuilt)};
  if (!arm)
    result.skin = {0x43534b52, std::move(newSkin)};
  result.triangles = output.size();
  return result.triangles > (arm ? 100 : 1800) && result.seamVertices > 0;
}
const std::map<uint32_t, uint64_t> armHashes = {
    {0x4ad9bbac, UINT64_C(0x3c25cfa36cd56e00)}, // Echoes Varia arm
};
std::mutex mutex;
bool attempted = false;
Result cached;
std::map< uint32_t, Result > arms;
} // namespace
bool Build(const Bytes& model, const Bytes& skin, const Bytes& skeleton, Result& result) {
  result = Result{};
  if (Hash(model) != UINT64_C(0x6947bbd39ab501cf) || Hash(skin) != UINT64_C(0x2fe58975338e5411) ||
      Hash(skeleton) != UINT64_C(0xbdbb58108f5963f6))
    return false;
  try {
    Result rebuilt;
    if (!Rebuild(model, skin, rebuilt))
      return false;
    result = std::move(rebuilt);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}
bool BuildArm(uint32_t id, const Bytes& model, Result& result) {
  result = Result{};
  const auto it = armHashes.find(id);
  if (it == armHashes.end() || Hash(model) != it->second)
    return false;
  try {
    Result rebuilt;
    if (!Rebuild(model, {}, rebuilt, true))
      return false;
    result = std::move(rebuilt);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}
bool IsResource(uint32_t id) { return id == kModel || id == kSkin || armHashes.count(id) != 0; }
const Resource* Find(uint32_t id, const DiscReader& read) {
  if (!IsResource(id))
    return nullptr;
  std::lock_guard< std::mutex > lock(mutex);
  if (armHashes.count(id)) {
    auto it = arms.find(id);
    if (it == arms.end()) {
      Result result;
      Bytes model;
      if (read(id, model))
        BuildArm(id, model, result);
      it = arms.emplace(id, std::move(result)).first;
    }
    return it->second.model.data.empty() ? nullptr : &it->second.model;
  }
  if (!attempted) {
    attempted = true;
    Bytes m, s, b;
    if (read(kModel, m) && read(kSkin, s) && read(kSkeleton, b))
      Build(m, s, b, cached);
  }
  if (cached.model.data.empty())
    return nullptr;
  return id == kModel ? &cached.model : &cached.skin;
}
void Reset() {
  std::lock_guard< std::mutex > lock(mutex);
  cached = Result{};
  attempted = false;
  arms.clear();
}
} // namespace PortCannonRepair
