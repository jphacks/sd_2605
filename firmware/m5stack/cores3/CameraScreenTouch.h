#pragma once
#include "TouchInput.h"

// Defer taps until release so a screen-switch hold cannot activate a control.
class CameraScreenTouch {
  bool down = false, held = false;
  uint32_t started = 0;
  int startX = 0, startY = 0;
 public:
  static constexpr int toggleScreen = 13;
  static constexpr int nextFace = 14;
  static constexpr int volumeDown = 15, volumeUp = 16, volumeWeb = 17;
  int poll(uint32_t now, bool pressed, int x, int y, bool debug) {
    if (pressed) {
      if (!down) {
        if (x < 0 || x >= 320 || y < 0 || y >= 240) return -1;
        down = true; held = false; started = now; startX = x; startY = y;
      }
      if (!held && uint32_t(now - started) >= 1000) {
        held = true;
        return toggleScreen;
      }
      return -1;
    }
    if (!down) return -1;
    down = false;
    if (held) return -1;
    if (!debug) return nextFace;
    if (startY >= 166) {
      int index = ((startY - 166) / 25) * 3 + startX / 107;
      return index < 6 ? 7 + index : volumeDown + index - 6;
    }
    TouchInput tap;
    return tap.poll(now, true, startX, startY, true);
  }
};
