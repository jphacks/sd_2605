#pragma once
#include "MotionDirectionTracker.h"
inline const char* cameraLiveStatus(int status, bool enabled, MotionTrackState state, bool sceneClear) {
  if (status == 0) return "Starting camera...";
  if (status == 2) return "Warming up...";
  if (status < 0) return "Camera/memory error: check Serial";
  if (!enabled) return "PAUSED: tap PAUSE/RUN";
  if (state == MotionTrackState::TRACKING) return "Tracking: move across view";
  if (state == MotionTrackState::DETECTED) return "Direction recorded";
  if (state == MotionTrackState::COOLDOWN)
    return "COOLDOWN: waiting for quiet";
  return "READY: move left/right across view";
}
inline bool cameraNoticeActive(uint32_t now, uint32_t shownAt) { return uint32_t(now - shownAt) < 2500; }
