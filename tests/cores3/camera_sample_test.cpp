#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/CameraFrameSample.h"
int main() {
  uint8_t image[4 * 4 * 2] = {};
  assert(cameraBlockLuma(image, 4, 0, 0) == 0);
  image[20] = image[21] = 255; // A single noisy pixel cannot exceed the motion pixel threshold (20).
  assert(cameraBlockLuma(image, 4, 0, 0) == 15);
  for (auto& b : image) b = 255;
  assert(cameraBlockLuma(image, 4, 0, 0) == 255); // Real block-wide changes remain visible.
  for (int i = 0; i < 16; ++i) { image[i * 2] = 0xf8; image[i * 2 + 1] = 0; }
  assert(cameraBlockLuma(image, 4, 0, 0) == 76); // RGB565 byte order and luma weights.
  puts("Camera sampling: black/white, isolated pixel noise averaging and RGB565 red passed");
}
