#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

// Brightness-compensated consecutive-frame difference, independent of hardware.
struct FrameScore { float percent; int lightShift; };
inline FrameScore compareFrames(const uint8_t* current, const uint8_t* previous,
                                size_t length, uint8_t delta) {
  if (!length) return {0, 0};
  int32_t sum = 0;
  for (size_t i = 0; i < length; ++i) sum += int(current[i]) - previous[i];
  int shift = sum / int(length);
  size_t changed = 0;
  for (size_t i = 0; i < length; ++i)
    if (abs(int(current[i]) - previous[i] - shift) >= delta) ++changed;
  return {100.0f * changed / length, shift};
}
