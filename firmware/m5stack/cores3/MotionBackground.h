#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Fit a global gain and offset to avoid treating post-occlusion camera exposure
// adjustment as an object that never left. This does not update the reference.
inline float backgroundDifference(const uint8_t* current, const uint8_t* reference,
                                  size_t length, uint8_t delta) {
  if (!length) return 0;
  float sumC = 0, sumR = 0, sumRR = 0, sumRC = 0;
  for (size_t i = 0; i < length; ++i) {
    float r = reference[i], c = current[i];
    sumC += c; sumR += r; sumRR += r * r; sumRC += r * c;
  }
  float variance = sumRR - sumR * sumR / length;
  float gain = variance > 1 ? (sumRC - sumR * sumC / length) / variance : 1;
  gain = std::max(.5f, std::min(2.0f, gain));
  float offset = (sumC - gain * sumR) / length;
  unsigned changed = 0;
  for (size_t i = 0; i < length; ++i)
    if (std::fabs(current[i] - (gain * reference[i] + offset)) >= delta) ++changed;
  return 100.0f * changed / length;
}
