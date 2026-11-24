#pragma once

// The construction kit's arithmetic (port_game_ui.cpp), with no game types so it can be tested on
// its own (tests/port_game_ui.cpp): how a box model's vertices move when the box takes another size.
//
// The game's boxes (the message box, the hint box, the quit screen) are frames of a few models: the
// frame line with its corners and decorations, and the fill behind it, in the frame's x/z plane,
// symmetric about x = 0, with the button (when there is one) on the bottom edge at x = 0. A box takes
// another size the way a 9-slice image does:
//   - the corners, the vertices in the cluster at the model's outer edge, shift in or out whole;
//   - the notch round the button, the vertices within its half width of x = 0, stays where it is;
//   - the rest stretches or squeezes between the two.
// Height the same way: the top band, the cluster at the model's top, moves up or down whole; the
// bottom band, with the button's edge, stays; between stretches. A cluster ends at the first gap of
// more than kClusterGap between the model's distinct values, so the zones come from each model's own
// shape rather than numbers measured for one frame.

#include <algorithm>
#include <cmath>
#include <vector>

namespace PortGameUiMath {

// The gap, in frame units, that ends a cluster of vertex positions. Inside a corner the game's
// points are at most about 0.4 apart; between a corner and the next detail at least about 1.
constexpr float kClusterGap = 0.5f;

struct Point {
  float x = 0.f, z = 0.f;
};

// How far a cluster reaches from the end of a sorted list of distinct values: the last value before a
// gap of more than kClusterGap, walking from values.front() (so pass them in the order to walk).
inline float ClusterEnd(const std::vector< float >& values) {
  if (values.empty()) {
    return 0.f;
  }
  float end = values.front();
  for (size_t i = 1; i < values.size(); ++i) {
    if (std::fabs(values[i] - values[i - 1]) > kClusterGap) {
      break;
    }
    end = values[i];
  }
  return end;
}

// A model's zones, in its own coordinates.
struct Zones {
  float outer = 0.f;      // the largest |x|
  float cornerFrom = 0.f; // |x| at and beyond this is a corner, moved whole
  float notch = 0.f;      // |x| at and within this stays (the button's notch); 0: no button
  float top = 0.f;        // the largest z
  float topFrom = 0.f;    // z at and above this is the top band, moved whole
  float bottom = 0.f;     // the smallest z
  float bottomTo = 0.f;   // z at and below this is the bottom band, which stays
  bool valid = false;
};

// The zones of a model from its vertices; `notch` is the button's half width (0 for none).
inline Zones ZonesOf(const std::vector< Point >& points, float notch) {
  Zones zn;
  if (points.empty()) {
    return zn;
  }
  std::vector< float > xs, zs;
  xs.reserve(points.size());
  zs.reserve(points.size());
  for (const Point& p : points) {
    xs.push_back(std::fabs(p.x));
    zs.push_back(p.z);
  }
  std::sort(xs.begin(), xs.end(), [](float a, float b) { return a > b; }); // outermost first
  std::sort(zs.begin(), zs.end(), [](float a, float b) { return a > b; }); // top first
  zn.outer = xs.front();
  zn.cornerFrom = ClusterEnd(xs);
  zn.notch = notch;
  zn.top = zs.front();
  zn.topFrom = ClusterEnd(zs);
  std::vector< float > up(zs.rbegin(), zs.rend()); // bottom first
  zn.bottom = up.front();
  zn.bottomTo = ClusterEnd(up);
  // A model all in one cluster (a flat strip) has no middle to stretch: nothing to do with it.
  zn.valid = zn.cornerFrom > zn.notch && zn.topFrom > zn.bottomTo;
  return zn;
}

// The most a box of these zones can narrow (a negative change in its half width) without its
// corners folding into the notch, with `margin` to spare between them. A frame's models all move by
// the same change (the fill's edge is inside the line's), so a frame takes the largest of these.
inline float MinGrowX(const Zones& zn, float margin = 0.2f) {
  return (zn.notch + margin) - zn.cornerFrom;
}

// The same for the height: the most the top can come down before the top band meets the bottom's.
inline float MinGrowZ(const Zones& zn, float margin = 0.2f) {
  return (zn.bottomTo + margin) - zn.topFrom;
}

// Where a point of the model goes when the box's half width changes by `growX` and its top by
// `growZ` (the bottom stays). Both are clamped to the minimums above.
inline Point Reshape(const Point& p, const Zones& zn, float growX, float growZ) {
  if (!zn.valid) {
    return p;
  }
  growX = std::max(growX, MinGrowX(zn));
  growZ = std::max(growZ, MinGrowZ(zn));
  Point out = p;
  // Across: a corner shifts by growX, the notch stays, between scales.
  const float ax = std::fabs(p.x);
  const float sx = p.x < 0.f ? -1.f : 1.f;
  float nx = ax;
  if (ax >= zn.cornerFrom) {
    nx = ax + growX;
  } else if (ax > zn.notch) {
    const float from = zn.cornerFrom - zn.notch;
    nx = zn.notch + (ax - zn.notch) * ((from + growX) / from);
  }
  out.x = sx * nx;
  // Up: the top band shifts by growZ, the bottom band stays, between scales.
  if (p.z >= zn.topFrom) {
    out.z = p.z + growZ;
  } else if (p.z > zn.bottomTo) {
    const float from = zn.topFrom - zn.bottomTo;
    out.z = zn.bottomTo + (p.z - zn.bottomTo) * ((from + growZ) / from);
  }
  return out;
}

// Pages (port_game_ui_page.h): a held direction steps once, then again after `delay` and every
// `interval` while held, as menus do. Step(direction, now) with direction 0 for none; true when this
// frame takes a step.
struct Repeat {
  float delay = 0.35f;
  float interval = 0.11f;
  int held = 0;
  double next = 0.0;
  bool Step(int direction, double now) {
    if (direction != held) {
      held = direction;
      next = now + delay;
      return direction != 0;
    }
    if (direction != 0 && now >= next) {
      next = now + interval;
      return true;
    }
    return false;
  }
};

// A list's lines, measured from the font itself: `top` the first line's ink top, `ink` one line's
// ink height (a one-line probe) and `pitch` from one line to the next (a two-line probe's ink less
// the one-line one), all in the same units.
struct Lines {
  float top = 0.f, ink = 0.f, pitch = 0.f;
  float Centre(int row) const { return top + 0.5f * ink + pitch * static_cast< float >(row); }
  // The row whose band (half a pitch either side of its centre) holds y; -1 outside rows 0..count-1.
  int RowAt(float y, int count) const {
    if (pitch <= 0.f) {
      return -1;
    }
    const int row = static_cast< int >(std::floor((y - Centre(0)) / pitch + 0.5f));
    return row >= 0 && row < count ? row : -1;
  }
};

// A step through `count` items, wrapping at both ends.
inline int Wrap(int index, int step, int count) {
  return count <= 0 ? 0 : ((index + step) % count + count) % count;
}

} // namespace PortGameUiMath
