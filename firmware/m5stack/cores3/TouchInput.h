#pragma once
#include <cstdint>
// One action per physical press, including when M5.update retains the same touch state.
class TouchInput {
  bool down = false, accepted = false;
  uint32_t last = 0;

 public:
  int poll(uint32_t now, bool pressed, int x, int y, bool tuning) {
    if (!pressed) {
      down = false;
      return -1;
    }
    if (down) return -1;
    down = true;
    if (accepted && uint32_t(now - last) < 150) return -1;
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return -1;
    int action = -1;
    if (y < 24)
      action = 6;
    else if (y >= 166)
      action = (tuning ? 7 : 0) + (y >= 203 ? 3 : 0) + x / 107;
    if (action >= 0) {
      last = now;
      accepted = true;
    }
    return action;
  }
};
