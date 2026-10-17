#pragma once
#include <cmath>
#include "MotionDirectionConfig.h"

enum class MotionDirection { UNKNOWN, LEFT_TO_RIGHT, RIGHT_TO_LEFT };
enum class MotionTrackState { IDLE, TRACKING, DETECTED, COOLDOWN };
enum class PassageAction { NONE, LEAVE, ARRIVE };
inline const char* directionName(MotionDirection d) {
  return d == MotionDirection::LEFT_TO_RIGHT ? "LEFT_TO_RIGHT" :
         d == MotionDirection::RIGHT_TO_LEFT ? "RIGHT_TO_LEFT" : "UNKNOWN";
}
inline const char* trackStateName(MotionTrackState s) {
  return s == MotionTrackState::TRACKING ? "TRACKING" : s == MotionTrackState::DETECTED ? "DETECTED" :
         s == MotionTrackState::COOLDOWN ? "COOLDOWN" : "IDLE";
}
inline PassageAction passageAction(MotionDirection d, bool invert) {
  if (d == MotionDirection::UNKNOWN) return PassageAction::NONE;
  bool leave = d == MotionDirection::LEFT_TO_RIGHT;
  return leave != invert ? PassageAction::LEAVE : PassageAction::ARRIVE;
}
inline const char* actionName(PassageAction action) {
  return action == PassageAction::LEAVE ? "LEAVE" : action == PassageAction::ARRIVE ? "ARRIVE" : "NONE";
}

struct DirectionObservation {
  bool valid = true, moving = false, usable = false, ambiguous = false;
  bool sceneClear = true;
  float x = -1;
  bool flowReliable = false;
  float flowDx = 0;
  DirectionObservation(bool validValue = true, bool movingValue = false, bool usableValue = false,
                       bool ambiguousValue = false, float position = -1, bool clearValue = true,
                       bool reliableFlow = false, float horizontalFlow = 0)
      : valid(validValue), moving(movingValue), usable(usableValue), ambiguous(ambiguousValue),
        sceneClear(clearValue), x(position), flowReliable(reliableFlow), flowDx(horizontalFlow) {}
};
struct MotionDirectionEvent {
  MotionDirection direction = MotionDirection::UNKNOWN;
  float xStart = -1, xEnd = -1, dx = 0;
  unsigned frames = 0;
  bool ambiguous = false, interrupted = false, timedOut = false;
  unsigned ambiguityFlags = 0; // 1 missing region, 2 multiple/broad regions, 4 association jump.
  float flowDx = 0;
  unsigned flowFrames = 0;
};

class MotionDirectionTracker {
  MotionTrackState stateValue = MotionTrackState::IDLE;
  MotionDirectionEvent last;
  MotionDirection previousDirection = MotionDirection::UNKNOWN;
  unsigned pending = 0, goodFrames = 0, quiet = 0, ambiguousFrames = 0;
  unsigned flowFrames = 0;
  float flowPositive = 0, flowNegative = 0;
  float startX = -1, smoothX = -1, lastRawX = -1, positiveTravel = 0, negativeTravel = 0;
  int firstZone = 0, lastZone = 0;
  bool uncertain = false;
  unsigned uncertaintyFlags = 0;
  uint32_t startedAt = 0, finalizedAt = 0, lastAt = 0, lastMovingAt = 0;
  bool haveTime = false;

  static int zone(float x) {
    return x <= MotionDirectionConfig::leftZoneEnd ? -1 :
           x >= MotionDirectionConfig::rightZoneStart ? 1 : 0;
  }
  void sample(const DirectionObservation& o) {
    using namespace MotionDirectionConfig;
    if (o.flowReliable) {
      ++flowFrames;
      if (o.flowDx > 0) flowPositive += o.flowDx;
      else flowNegative -= o.flowDx;
    }
    if (!o.usable) uncertaintyFlags |= 1;
    if (o.ambiguous) { uncertaintyFlags |= 2; ++ambiguousFrames; }
    if (!o.usable) return;
    if (lastRawX >= 0) {
      float step = o.x - lastRawX;
      if (std::fabs(step) > maxCentroidStep) { uncertain = true; uncertaintyFlags |= 4; }
    }
    lastRawX = o.x;
    if (smoothX < 0) startX = smoothX = o.x;
    else {
      float step = smoothingAlpha * (o.x - smoothX);
      smoothX += step;
      if (step > jitterPixels) positiveTravel += step;
      if (step < -jitterPixels) negativeTravel -= step;
    }
    ++goodFrames;
    int currentZone = zone(smoothX);
    if (currentZone) {
      if (!firstZone) firstZone = currentZone;
      lastZone = currentZone;
    }
  }
  MotionDirection candidate() const {
    using namespace MotionDirectionConfig;
    if (ambiguousFrames >= 3 && ambiguousFrames * 3 >= goodFrames) return MotionDirection::UNKNOWN;
    // Direct image displacement is independent of a difference-region centroid.
    // A conflicting flow trajectory must not be overridden by the centroid.
    if (flowFrames >= minTrackFrames) {
      float dx = flowPositive - flowNegative;
      float travel = flowPositive + flowNegative;
      if (dx > minDisplacement && dx >= travel * flowPathAgreement) return MotionDirection::LEFT_TO_RIGHT;
      if (dx < -minDisplacement && -dx >= travel * flowPathAgreement) return MotionDirection::RIGHT_TO_LEFT;
      return MotionDirection::UNKNOWN;
    }
    if (uncertain || goodFrames < minTrackFrames) return MotionDirection::UNKNOWN;
    float dx = smoothX - startX;
    if (dx > minDisplacement && negativeTravel <= maxReverseTravel &&
        !(firstZone == 1 && lastZone == -1)) return MotionDirection::LEFT_TO_RIGHT;
    if (dx < -minDisplacement && positiveTravel <= maxReverseTravel &&
        !(firstZone == -1 && lastZone == 1)) return MotionDirection::RIGHT_TO_LEFT;
    return MotionDirection::UNKNOWN;
  }
  bool finish(uint32_t now, bool interrupted, bool timedOut) {
    using namespace MotionDirectionConfig;
    last = MotionDirectionEvent{};
    last.xStart = startX; last.xEnd = smoothX; last.dx = smoothX >= 0 ? smoothX - startX : 0;
    // A single noisy/missing region must not poison an otherwise coherent passage.
    uncertain = uncertain || (ambiguousFrames >= 3 && ambiguousFrames * 3 >= goodFrames);
    last.frames = goodFrames; last.ambiguous = uncertain;
    last.ambiguityFlags = uncertaintyFlags;
    last.flowDx = flowPositive - flowNegative; last.flowFrames = flowFrames;
    last.interrupted = interrupted; last.timedOut = timedOut;
    if (!interrupted && !timedOut) last.direction = candidate();
    // Preserve passage state across cooldown and explicit tracking restarts.
    if (last.direction == MotionDirection::UNKNOWN) {
      if (previousDirection == MotionDirection::LEFT_TO_RIGHT)
        last.direction = MotionDirection::RIGHT_TO_LEFT;
      else if (previousDirection == MotionDirection::RIGHT_TO_LEFT)
        last.direction = MotionDirection::LEFT_TO_RIGHT;
    }
    if (last.direction != MotionDirection::UNKNOWN) previousDirection = last.direction;
    finalizedAt = now; lastMovingAt = now; stateValue = MotionTrackState::DETECTED; quiet = 0; pending = 0;
    return true;
  }
 public:
  // Explicit operator restart only; automatic cooldown never relearns an occupied scene.
  void restart() {
    stateValue = MotionTrackState::IDLE; pending = goodFrames = quiet = ambiguousFrames = 0;
    flowFrames = 0; flowPositive = flowNegative = 0;
    startX = smoothX = lastRawX = -1; positiveTravel = negativeTravel = 0;
    uncertain = false; uncertaintyFlags = 0; firstZone = lastZone = 0; haveTime = false;
  }
  MotionTrackState state() const { return stateValue; }
  float xStart() const { return startX; }
  float xCurrent() const { return smoothX; }
  // Display-only estimate. Never publishes an event or changes tracker state.
  MotionDirection liveDirection() const {
    return stateValue == MotionTrackState::TRACKING ? candidate() : MotionDirection::UNKNOWN;
  }
  const MotionDirectionEvent& event() const { return last; }
  bool update(uint32_t now, DirectionObservation o) {
    using namespace MotionDirectionConfig;
    if (haveTime && uint32_t(now - lastAt) > maxFrameGapMs) o.valid = false;
    lastAt = now; haveTime = true;
    if (stateValue == MotionTrackState::DETECTED || stateValue == MotionTrackState::COOLDOWN) {
      if (o.valid && !o.moving) { if (quiet < endFrames) ++quiet; }
      else { quiet = 0; lastMovingAt = now; }
      if (uint32_t(now - finalizedAt) >= detectedHoldMs) stateValue = MotionTrackState::COOLDOWN;
      if (stateValue == MotionTrackState::COOLDOWN && uint32_t(now - finalizedAt) >= cooldownMs &&
          uint32_t(now - lastMovingAt) >= rearmQuietMs && quiet >= endFrames) {
        stateValue = MotionTrackState::IDLE; pending = 0;
      }
      return false;
    }
    if (!o.valid) {
      if (stateValue == MotionTrackState::TRACKING) return finish(now, true, false);
      pending = 0;
      return false;
    }
    if (stateValue == MotionTrackState::IDLE) {
      if (!o.moving) { pending = 0; return false; }
      if (!pending) {
        startX = smoothX = lastRawX = -1; goodFrames = ambiguousFrames = 0;
        flowFrames = 0; flowPositive = flowNegative = 0;
        positiveTravel = negativeTravel = 0; uncertain = false; uncertaintyFlags = 0; firstZone = lastZone = 0;
        startedAt = now;
      }
      sample(o);
      lastMovingAt = now;
      if (++pending >= startFrames) { stateValue = MotionTrackState::TRACKING; quiet = 0; }
      return false;
    }
    if (o.moving) { quiet = 0; lastMovingAt = now; sample(o); }
    else ++quiet;
    if (quiet >= endFrames && uint32_t(now - lastMovingAt) >= endQuietMs)
      return finish(now, false, false);
    if (uint32_t(now - startedAt) >= maxTrackMs) return finish(now, false, true);
    return false;
  }
};
