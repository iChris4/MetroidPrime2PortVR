// PortPrime1Tweaks: the version 3.5 PAK lookup and the CTWK "Ball" layout check, on files built
// here (no game data): a PAK with names before and after Ball, a Ball body whose floats are their
// own index, and the refusals for a wrong version, a missing name and a body of the wrong size.
#include "port_prime1_tweaks.h"
#include "port_test.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

void Put32(std::vector< uint8_t >& out, uint32_t v) {
  out.push_back(uint8_t(v >> 24));
  out.push_back(uint8_t(v >> 16));
  out.push_back(uint8_t(v >> 8));
  out.push_back(uint8_t(v));
}

void PutFourCC(std::vector< uint8_t >& out, const char* type) { out.insert(out.end(), type, type + 4); }

void PutFloat(std::vector< uint8_t >& out, float f) {
  uint32_t bits;
  std::memcpy(&bits, &f, 4);
  Put32(out, bits);
}

struct Resource {
  const char* type;
  uint32_t id;
  const char* name; // null: unnamed
  std::vector< uint8_t > body;
};

// A version 3.5 PAK: header, name table, resource table, then the bodies (padded to 32 bytes).
std::vector< uint8_t > BuildPak(const std::vector< Resource >& resources, uint32_t version = 0x00030005u) {
  std::vector< uint8_t > pak;
  Put32(pak, version);
  Put32(pak, 0);
  uint32_t named = 0;
  for (const Resource& r : resources) {
    named += r.name != nullptr;
  }
  Put32(pak, named);
  for (const Resource& r : resources) {
    if (r.name != nullptr) {
      PutFourCC(pak, r.type);
      Put32(pak, r.id);
      Put32(pak, uint32_t(std::strlen(r.name)));
      pak.insert(pak.end(), r.name, r.name + std::strlen(r.name));
    }
  }
  Put32(pak, uint32_t(resources.size()));
  const size_t tableAt = pak.size();
  pak.resize(pak.size() + resources.size() * 20);
  while (pak.size() % 32 != 0) {
    pak.push_back(0);
  }
  for (size_t i = 0; i < resources.size(); ++i) {
    std::vector< uint8_t > body = resources[i].body;
    while (body.size() % 32 != 0) {
      body.push_back(0);
    }
    std::vector< uint8_t > entry;
    Put32(entry, 0);
    PutFourCC(entry, resources[i].type);
    Put32(entry, resources[i].id);
    Put32(entry, uint32_t(body.size()));
    Put32(entry, uint32_t(pak.size()));
    std::memcpy(pak.data() + tableAt + i * 20, entry.data(), 20);
    pak.insert(pak.end(), body.begin(), body.end());
  }
  return pak;
}

// `bad` (a float index) gets `badValue` instead of its own index.
std::vector< uint8_t > BallBody(size_t floats = PortPrime1Tweaks::kBallFloats, size_t bad = size_t(-1),
                                float badValue = 0.f) {
  std::vector< uint8_t > body;
  for (size_t i = 0; i < floats; ++i) {
    PutFloat(body, i == bad ? badValue : float(i));
  }
  return body;
}

void TestFindsBallAmongOthers() {
  const std::vector< Resource > resources = {
      {"CTWK", 0x11111111u, "Player", BallBody(10)},
      {"CTWK", 0x22222222u, "Ball", BallBody()},
      {"STRG", 0x22222222u, nullptr, BallBody(4)}, // same id, other type: not Ball
      {"CTWK", 0x33333333u, "Game", BallBody(3)},
  };
  const std::vector< uint8_t > pak = BuildPak(resources);
  PortPrime1Tweaks::PakEntry entry;
  std::string error;
  REQUIRE_MSG(PortPrime1Tweaks::FindPakResource(pak.data(), pak.size(), "CTWK", "Ball", entry, error),
              error);
  CHECK(!entry.compressed);
  CHECK_EQ(entry.size, 480u); // 456 padded to 32

  PortPrime1Tweaks::BoostBall ball;
  REQUIRE_MSG(PortPrime1Tweaks::ParseBall(pak.data() + entry.offset, entry.size, ball, error), error);
  CHECK_EQ(ball.drainTime, 105.f);
  CHECK_EQ(ball.minChargeTime, 106.f);
  CHECK_EQ(ball.chargeTime[0], 108.f);
  CHECK_EQ(ball.chargeTime[1], 109.f);
  CHECK_EQ(ball.maxChargeTime, 110.f);
  CHECK_EQ(ball.chargeTime[2], 110.f);
  CHECK_EQ(ball.incrementalSpeed[0], 111.f);
  CHECK_EQ(ball.incrementalSpeed[1], 112.f);
  CHECK_EQ(ball.incrementalSpeed[2], 113.f);
  CHECK_EQ(ball.cameraBoostSpringTardis, 97.f);
}

void TestRefusals() {
  PortPrime1Tweaks::PakEntry entry;
  PortPrime1Tweaks::BoostBall ball;
  std::string error;
  // An Echoes-era PAK (another version) is not read.
  const std::vector< uint8_t > other = BuildPak({{"CTWK", 1u, "Ball", BallBody()}}, 0x00000002u);
  CHECK(!PortPrime1Tweaks::FindPakResource(other.data(), other.size(), "CTWK", "Ball", entry, error));
  // No Ball.
  const std::vector< uint8_t > noBall = BuildPak({{"CTWK", 1u, "Player", BallBody()}});
  CHECK(!PortPrime1Tweaks::FindPakResource(noBall.data(), noBall.size(), "CTWK", "Ball", entry, error));
  // A truncated file.
  const std::vector< uint8_t > pak = BuildPak({{"CTWK", 1u, "Ball", BallBody()}});
  CHECK(!PortPrime1Tweaks::FindPakResource(pak.data(), 20, "CTWK", "Ball", entry, error));
  // A Ball of another layout (one float short, or a whole extra block) is refused, not misread.
  const std::vector< uint8_t > shortBody = BallBody(PortPrime1Tweaks::kBallFloats - 1);
  CHECK(!PortPrime1Tweaks::ParseBall(shortBody.data(), shortBody.size(), ball, error));
  const std::vector< uint8_t > longBody = BallBody(PortPrime1Tweaks::kBallFloats + 8);
  CHECK(!PortPrime1Tweaks::ParseBall(longBody.data(), longBody.size(), ball, error));
  // Values the ball would divide by or add to its velocity must be finite and not negative, the two divisors not zero.
  const float inf = std::numeric_limits< float >::infinity();
  const float nan = std::numeric_limits< float >::quiet_NaN();
  const struct {
    size_t index;
    float value;
  } bad[] = {{105, 0.f}, {110, 0.f}, {106, -1.f}, {108, nan}, {113, inf}, {97, nan}};
  for (const auto& b : bad) {
    const std::vector< uint8_t > body = BallBody(PortPrime1Tweaks::kBallFloats, b.index, b.value);
    CHECK(!PortPrime1Tweaks::ParseBall(body.data(), body.size(), ball, error));
  }
  // A float the Boost Ball does not use may be anything.
  const std::vector< uint8_t > unused = BallBody(PortPrime1Tweaks::kBallFloats, 0, nan);
  CHECK(PortPrime1Tweaks::ParseBall(unused.data(), unused.size(), ball, error));
}

} // namespace

int main() {
  TestFindsBallAmongOthers();
  TestRefusals();
  return PORT_TEST_RESULT();
}
