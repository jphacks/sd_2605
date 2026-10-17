#pragma once
#include <algorithm>
#include <cstdlib>
#include "MotionDirectionConfig.h"

struct HorizontalMotion {
  bool reliable = false;
  float dx = 0;
  unsigned votes = 0;
};

// Sparse 5x5 horizontal block matching on the existing 40x30 luma image.
// Static/flat patches and matches with an equally plausible opposite direction
// do not vote. No heap allocation or additional image buffer is required.
inline HorizontalMotion estimateHorizontalMotion(const uint8_t* current, const uint8_t* previous, int lightShift) {
  using namespace MotionDirectionConfig;
  constexpr int radius = flowSearchRadius, patchRadius = 2;
  int positive = 0, negative = 0, positiveDx = 0, negativeDx = 0;
  for (int y = 3; y < sampleHeight - 3; y += 4) {
    for (int x = radius + patchRadius; x < sampleWidth - radius - patchRadius; x += 4) {
      int costs[radius * 2 + 1] = {};
      int low = 255, high = 0;
      for (int yy = -patchRadius; yy <= patchRadius; ++yy)
        for (int xx = -patchRadius; xx <= patchRadius; ++xx) {
          int value = current[(y + yy) * sampleWidth + x + xx];
          low = std::min(low, value); high = std::max(high, value);
          for (int dx = -radius; dx <= radius; ++dx)
            costs[dx + radius] += std::abs(value - previous[(y + yy) * sampleWidth + x + xx - dx] - lightShift);
        }
      if (high - low < 25 || costs[radius] < 25 * 8) continue;
      int best = 0;
      for (int i = 1; i <= radius * 2; ++i) if (costs[i] < costs[best]) best = i;
      int dx = best - radius;
      if (!dx || costs[best] * 100 > costs[radius] * 65 || costs[radius] - costs[best] < 25 * 3) continue;
      int opposite = costs[radius];
      for (int shift = 1; shift <= radius; ++shift)
        opposite = std::min(opposite, costs[radius + (dx > 0 ? -shift : shift)]);
      if (opposite - costs[best] < 25 * 2) continue;
      if (dx > 0) { ++positive; positiveDx += dx; }
      else { ++negative; negativeDx += dx; }
    }
  }
  HorizontalMotion result;
  result.votes = positive + negative;
  int dominant = std::max(positive, negative);
  result.reliable = dominant >= flowMinVotes && dominant >= result.votes * flowVoteAgreement;
  if (result.reliable)
    result.dx = float(positive >= negative ? positiveDx : negativeDx) / dominant *
                (imageWidth / sampleWidth) / differenceLagFrames;
  return result;
}
