#pragma once
#include <cstddef>
#include <cstdint>
// Average all 16 pixels of each 4x4 block, rather than sampling one noisy pixel.
inline uint8_t cameraBlockLuma(const uint8_t* rgb565, size_t width, int x, int y) {
  unsigned total = 0;
  for (int dy = 0; dy < 4; ++dy) {
    for (int dx = 0; dx < 4; ++dx) {
      size_t pos = ((y + dy) * width + x + dx) * 2;
      uint16_t rgb = (uint16_t(rgb565[pos]) << 8) | rgb565[pos + 1];
      int r = ((rgb >> 11) & 31) * 255 / 31;
      int g = ((rgb >> 5) & 63) * 255 / 63;
      int b = (rgb & 31) * 255 / 31;
      total += (77 * r + 150 * g + 29 * b) >> 8;
    }
  }
  return total / 16;
}
