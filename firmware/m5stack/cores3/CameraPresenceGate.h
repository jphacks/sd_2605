#pragma once
#include <cstdint>
// Require repeated positive frames, then absence plus cooldown before rearming.
class CameraPresenceGate {
  unsigned nearFrames = 0, clearFrames = 0;
  bool armed = true, triggered = false;
  uint32_t lastDetection = 0;
 public:
  bool present = false;
  void reset() { nearFrames = clearFrames = 0; present = false; armed = false; }
  bool update(uint32_t now, float score, float threshold, bool enabled) {
    if (!enabled) { reset(); return false; }
    if (score >= threshold) {
      clearFrames = 0;
      if (nearFrames < 2) ++nearFrames;
      if (nearFrames >= 2) {
        present = true;
        if (armed && (!triggered || uint32_t(now - lastDetection) >= 10000)) {
          armed = false; triggered = true; lastDetection = now;
          return true;
        }
      }
    } else {
      nearFrames = 0;
      if (score <= threshold - 0.2f) {
        if (clearFrames < 3) ++clearFrames;
        if (clearFrames >= 3) {
          present = false;
          if (!triggered || uint32_t(now - lastDetection) >= 10000) armed = true;
        }
      } else clearFrames = 0;
    }
    return false;
  }
};
