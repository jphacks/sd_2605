#pragma once
#include <cstdint>
#include "MotionDirectionConfig.h"
class CameraMotionGate {
  unsigned activeFrames = 0, quietFrames = 0;
  bool armed = true, triggered = false;
  uint32_t lastDetection = 0;
 public:
  void reset() { activeFrames = quietFrames = 0; armed = !triggered; }
  bool ready() const { return armed; }
  bool update(uint32_t now, float percent, float threshold, bool valid) {
    if (!valid) { reset(); return false; }
    if (percent < threshold) {
      activeFrames = 0;
      if (quietFrames < MotionDirectionConfig::endFrames) ++quietFrames;
      if (quietFrames >= MotionDirectionConfig::endFrames &&
          (!triggered || uint32_t(now - lastDetection) >= MotionDirectionConfig::cooldownMs)) armed = true;
    } else {
      quietFrames = 0;
      if (!armed || percent < threshold) { activeFrames = 0; return false; }
      if (++activeFrames >= MotionDirectionConfig::startFrames) {
        armed = false; triggered = true; lastDetection = now; activeFrames = 0;
        return true;
      }
    }
    return false;
  }
};
