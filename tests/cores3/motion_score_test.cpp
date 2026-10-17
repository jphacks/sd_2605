#include <cassert>
#include <algorithm>
#include "../../firmware/m5stack/cores3/motion_score.h"
int main() {
  uint8_t previous[1200], current[1200];
  std::fill_n(previous, 1200, 80);
  std::fill_n(current, 1200, 80);
  assert(compareFrames(current, previous, 1200, 20).percent == 0);
  // A whole-image exposure change is removed from the score.
  std::fill_n(current, 1200, 130);
  auto light = compareFrames(current, previous, 1200, 20);
  assert(light.percent == 0 && light.lightShift == 50);
  // A localized object covering 10% is still detected under a brightness shift.
  std::fill_n(current, 1200, 90);
  std::fill_n(current, 120, 170);
  auto object = compareFrames(current, previous, 1200, 20);
  assert(object.percent == 10 && object.lightShift == 18);
  // Low-amplitude sensor noise stays below the per-pixel threshold.
  for (int i = 0; i < 1200; ++i) current[i] = 80 + (i % 7) - 3;
  assert(compareFrames(current, previous, 1200, 20).percent == 0);
  assert(compareFrames(current, previous, 0, 20).percent == 0);
}
