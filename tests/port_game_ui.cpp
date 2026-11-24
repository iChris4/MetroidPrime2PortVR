// The box construction kit's arithmetic (platform/include/port_game_ui_math.h): a box model's zones
// found from its own vertices, and where its vertices go for another size. Shapes from the game's
// frames (the message box's and hint box's frame lines, as dumped from the disc). No game or GPU.
#include "port_game_ui_math.h"
#include "port_test.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace PortGameUiMath;

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// A symmetric model from distinct |x| and z values: every combination (the zones only look at the
// values, so a grid stands in for the model).
std::vector< Point > Grid(const std::vector< float >& ax, const std::vector< float >& zs) {
  std::vector< Point > out;
  for (float x : ax) {
    for (float z : zs) {
      out.push_back({x, z});
      out.push_back({-x, z});
    }
  }
  return out;
}

// The message box's frame line (FRME_MsgScreen model_center): corners from 8.47 out to 9.21, the
// top decorations at 3.6 to 6.3, the button's notch inside 0.75.
const std::vector< float > kMsgX{0.f, 0.04f, 0.72f, 0.75f, 3.60f, 3.63f, 3.94f, 3.96f, 5.83f, 5.86f, 6.23f,
                                 6.26f, 8.47f, 8.5f, 8.58f, 8.6f, 8.65f, 8.67f, 8.7f, 8.74f, 8.86f, 8.88f,
                                 8.89f, 8.93f, 8.99f, 9.06f, 9.08f, 9.14f, 9.16f, 9.19f, 9.21f};
const std::vector< float > kMsgZ{-2.70f, -2.63f, -2.60f, -2.48f, -2.40f, -2.34f, -2.21f, -2.17f, -2.13f,
                                 -1.97f, -1.59f, -1.16f, 0.f, 1.55f, 1.56f, 1.97f, 2.36f, 2.47f, 2.53f,
                                 2.60f, 2.71f, 2.75f, 2.79f, 2.97f, 3.00f, 3.09f};

void TestClusters() {
  // From the outer edge in, until a gap of more than kClusterGap.
  CHECK(Near(ClusterEnd({9.21f, 9.0f, 8.6f, 8.47f, 6.26f, 0.f}), 8.47f));
  // The hint box's frame line (FRME_HudMessage model_messageframe): its corner, then the notch.
  CHECK(Near(ClusterEnd({5.54f, 5.42f, 5.23f, 5.16f, 5.13f, 4.97f, 0.77f}), 4.97f));
  CHECK(Near(ClusterEnd({3.f}), 3.f));
  CHECK(Near(ClusterEnd({}), 0.f));
}

void TestZones() {
  const Zones zn = ZonesOf(Grid(kMsgX, kMsgZ), 0.88f);
  REQUIRE(zn.valid);
  CHECK(Near(zn.outer, 9.21f));
  CHECK(Near(zn.cornerFrom, 8.47f)); // the corners, not the decorations at 6.26
  CHECK(Near(zn.top, 3.09f));
  CHECK(Near(zn.bottom, -2.70f));
  CHECK(zn.topFrom > 1.f && zn.topFrom < 2.f);    // the top band takes in the side detail at 1.55
  CHECK(zn.bottomTo < -1.f && zn.bottomTo > -2.f); // the bottom band, the button's edge, to -1.16

  // A flat strip (one cluster) has nothing to stretch.
  CHECK(!ZonesOf(Grid({0.1f, 0.2f}, {0.f, 0.1f}), 0.f).valid);
}

void TestReshape() {
  const Zones zn = ZonesOf(Grid(kMsgX, kMsgZ), 0.88f);
  // Narrower by 4.7 and 2 lower at the top: corners shift whole, the notch stays, between squeezes.
  const float gx = -4.7f, gz = -2.f;
  CHECK(Near(Reshape({9.21f, 0.f}, zn, gx, gz).x, 9.21f - 4.7f));
  CHECK(Near(Reshape({-8.47f, 0.f}, zn, gx, gz).x, -8.47f + 4.7f));
  CHECK(Near(Reshape({0.75f, 0.f}, zn, gx, gz).x, 0.75f)); // inside the notch
  const float mid = Reshape({6.26f, 0.f}, zn, gx, gz).x;
  CHECK(mid > 0.88f && mid < 8.47f - 4.7f); // squeezed, still between the notch and the corner
  // The corner keeps its shape: two of its points stay the same distance apart.
  CHECK(Near(Reshape({9.21f, 0.f}, zn, gx, gz).x - Reshape({8.47f, 0.f}, zn, gx, gz).x, 9.21f - 8.47f));
  // Top band down by 2.6, the bottom band (the button's edge) where it was.
  CHECK(Near(Reshape({0.f, 3.09f}, zn, gx, gz).z, 3.09f - 2.f));
  CHECK(Near(Reshape({0.f, -2.70f}, zn, gx, gz).z, -2.70f));
  // Order kept: no point crosses another, across or up.
  float lastX = -1.f;
  for (float x : kMsgX) {
    const float nx = Reshape({x, 0.f}, zn, gx, gz).x;
    CHECK(nx >= lastX - 1e-5f);
    lastX = nx;
  }
  float lastZ = -100.f;
  for (float z : kMsgZ) {
    const float nz = Reshape({0.f, z}, zn, gx, gz).z;
    CHECK(nz >= lastZ - 1e-5f);
    lastZ = nz;
  }
  // Wider works the same way.
  CHECK(Near(Reshape({9.21f, 0.f}, zn, 2.f, 0.f).x, 11.21f));
}

void TestLimits() {
  const Zones zn = ZonesOf(Grid(kMsgX, kMsgZ), 0.88f);
  // Asking for nothing at all: the corners stop just outside the notch, the top just above the
  // bottom band, and nothing folds.
  const Point corner = Reshape({8.47f, 3.09f}, zn, -100.f, -100.f);
  CHECK(corner.x >= zn.notch + 0.2f - 1e-4f);
  const Point topBand = Reshape({0.f, zn.topFrom}, zn, -100.f, -100.f);
  CHECK(topBand.z >= zn.bottomTo + 0.2f - 1e-4f);
  CHECK(Near(MinGrowX(zn), (0.88f + 0.2f) - 8.47f));
  // Further than the limit is held at it: the top band stops 0.2 above the bottom band.
  CHECK(Near(Reshape({0.f, 3.09f}, zn, 0.f, -2.6f).z, 3.09f + MinGrowZ(zn)));
}

void TestRepeat() {
  Repeat r;
  CHECK(!r.Step(0, 0.0));
  CHECK(r.Step(1, 1.0));    // the press steps at once
  CHECK(!r.Step(1, 1.2));   // then waits the delay
  CHECK(r.Step(1, 1.36));   // then repeats
  CHECK(!r.Step(1, 1.40));
  CHECK(r.Step(1, 1.48));   // every interval
  CHECK(r.Step(-1, 1.49));  // the other way is a new press
  CHECK(!r.Step(0, 1.5));
  CHECK(r.Step(-1, 1.51));  // released and pressed again
}

void TestLines() {
  // The save state list at 1080p: a 0.0204-high line every 0.0482, the first from 0.2907.
  const Lines lines{0.2907f, 0.0204f, 0.0482f};
  CHECK(Near(lines.Centre(0), 0.3009f));
  CHECK(Near(lines.Centre(7), 0.3009f + 7 * 0.0482f));
  CHECK(lines.RowAt(0.3009f, 8) == 0);
  CHECK(lines.RowAt(0.3009f + 0.024f, 8) == 0);   // just short of half a pitch down
  CHECK(lines.RowAt(0.3009f + 0.025f, 8) == 1);   // just past it
  CHECK(lines.RowAt(0.3009f - 0.025f, 8) == -1);  // above the first
  CHECK(lines.RowAt(lines.Centre(7) + 0.025f, 8) == -1); // below the last
  CHECK(Lines{}.RowAt(0.5f, 8) == -1);
  CHECK(Wrap(0, -1, 8) == 7);
  CHECK(Wrap(7, 1, 8) == 0);
  CHECK(Wrap(3, 1, 8) == 4);
}

} // namespace

int main() {
  TestRepeat();
  TestLines();
  TestClusters();
  TestZones();
  TestReshape();
  TestLimits();
  if (port_test::Failures() == 0) {
    std::printf("port_game_ui: ok\n");
  }
  return PORT_TEST_RESULT();
}
