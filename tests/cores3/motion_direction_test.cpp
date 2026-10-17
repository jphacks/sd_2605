#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../firmware/m5stack/cores3/MotionRegion.h"
#include "../../firmware/m5stack/cores3/MotionDirectionTracker.h"
#include "../../firmware/m5stack/cores3/CameraMotionGate.h"
#include "../../firmware/m5stack/cores3/motion_score.h"
#include "../../firmware/m5stack/cores3/MotionBackground.h"
#include "../../firmware/m5stack/cores3/HorizontalMotion.h"

static constexpr unsigned finishFrames = (MotionDirectionConfig::endQuietMs + 99) / 100;
static DirectionObservation point(float x, bool ambiguous = false) { return {true, true, true, ambiguous, x}; }
static MotionDirectionEvent trajectory(std::initializer_list<float> positions, bool ambiguous = false) {
  MotionDirectionTracker tracker;
  uint32_t now = 0;
  for (float x : positions) { assert(!tracker.update(now, point(x, ambiguous))); now += 100; }
  assert(tracker.state() == MotionTrackState::TRACKING);
  const auto live = tracker.liveDirection();
  assert(tracker.liveDirection() == live); // Reading the label cannot mutate/publish a track.
  for (unsigned i = 0; i < finishFrames; ++i) {
    bool event = tracker.update(now, {}); now += 100;
    assert(event == (i + 1 == finishFrames));
  }
  return tracker.event();
}
static void rectangle(uint8_t* image, int x, int y, int width, int height, uint8_t value) {
  for (int yy = std::max(0, y); yy < std::min(30, y + height); ++yy)
    for (int xx = std::max(0, x); xx < std::min(40, x + width); ++xx) image[yy * 40 + xx] = value;
}

// Exercise actual frame difference -> region -> centroid -> state machine in both directions.
static void imagePass(bool reverse, unsigned lag = 1, uint32_t frameMs = 100) {
  uint8_t previous[2][1200], current[1200], background[1200];
  std::fill_n(previous[0], 1200, 80); std::fill_n(previous[1], 1200, 80); std::fill_n(background, 1200, 80);
  MotionRegionExtractor extractor;
  MotionDirectionTracker tracker;
  CameraMotionGate legacy;
  unsigned directionEvents = 0, legacyEvents = 0;
  uint32_t now = 0;
  unsigned history = 0;
  const float threshold = lag == 2 ? 3.0f : 5.0f;
  auto frame = [&](int x, bool draw) {
    std::fill_n(current, 1200, 80);
    if (draw) rectangle(current, x, 4, 8, 22, 180);
    auto score = compareFrames(current, previous[history], 1200, MotionDirectionConfig::pixelDelta);
    auto region = extractor.extract(current, previous[history], score.lightShift);
    auto bg = backgroundDifference(current, background, 1200, MotionDirectionConfig::pixelDelta);
    bool event = tracker.update(now, {true, score.percent >= threshold, region.usable, region.ambiguous, region.x, bg < MotionDirectionConfig::backgroundClearPercent});
    if (legacy.update(now, score.percent, threshold, true)) ++legacyEvents;
    if (event) {
      ++directionEvents;
      assert(tracker.event().direction == (reverse ? MotionDirection::RIGHT_TO_LEFT : MotionDirection::LEFT_TO_RIGHT));
      assert(std::fabs(tracker.event().dx) > MotionDirectionConfig::minDisplacement);
    }
    memcpy(previous[history], current, sizeof(current)); history = (history + 1) % lag; now += frameMs;
  };
  for (int i = 0; i < 5; ++i) frame(0, false);
  for (int step = -8; step <= 43; step += 3) frame(reverse ? 32 - step : step, true);
  for (int i = 0; i < 100; ++i) frame(0, false);
  assert(directionEvents == 1 && legacyEvents == 1);
  assert(tracker.state() == MotionTrackState::IDLE);
  std::printf("Image pass %s lag=%u period=%lums: one motion, one direction event\n", reverse ? "RTL" : "LTR", lag, (unsigned long)frameMs);
}

int main() {
  uint8_t flowPrevious[1200], flowCurrent[1200];
  for (int sign : {-1, 1}) {
    for (int y = 0; y < 30; ++y) for (int x = 0; x < 40; ++x) {
      flowPrevious[y*40+x] = 40 + ((x*31 + y*47 + x*y*13) % 170);
    }
    for (int y = 0; y < 30; ++y) for (int x = 0; x < 40; ++x)
      flowCurrent[y*40+x] = flowPrevious[y*40+std::max(0,std::min(39,x-sign*2))];
    auto flow = estimateHorizontalMotion(flowCurrent, flowPrevious, 0);
    assert(flow.reliable && std::fabs(flow.dx - sign*4.f) < .01f);
    assert(!estimateHorizontalMotion(flowPrevious, flowPrevious, 0).reliable);
    std::fill_n(flowPrevious,1200,80); std::fill_n(flowCurrent,1200,80);
    for (int y = 3; y < 27; ++y) for (int x = 0; x < 16; ++x) {
      uint8_t texture = 40 + ((x*31 + y*47 + x*y*13) % 170);
      flowPrevious[y*40+10+x] = texture;
      flowCurrent[y*40+10+x+sign*2] = texture;
    }
    flow = estimateHorizontalMotion(flowCurrent,flowPrevious,0);
    assert(flow.reliable && std::fabs(flow.dx-sign*4.f) < 1);
    MotionDirectionTracker optical;
    uint32_t t = 0;
    // Broad objects can leave the difference centroid near the center throughout.
    for (int i = 0; i < 14; ++i) {
      assert(!optical.update(t, {true,true,true,false,80.f+(i%3),false,flow.reliable,flow.dx})); t += 50;
    }
    assert(optical.liveDirection() == (sign > 0 ? MotionDirection::LEFT_TO_RIGHT : MotionDirection::RIGHT_TO_LEFT));
    unsigned events = 0;
    for (int i = 0; i < 80; ++i) { if (optical.update(t, {})) ++events; t += 50; }
    assert(events == 1 && optical.event().direction == (sign > 0 ? MotionDirection::LEFT_TO_RIGHT : MotionDirection::RIGHT_TO_LEFT));
    assert(std::fabs(optical.event().dx) < 5 && std::fabs(optical.event().flowDx) > 32);
  }
  std::fill_n(flowCurrent,1200,80); std::fill_n(flowPrevious,1200,40);
  assert(!estimateHorizontalMotion(flowCurrent,flowPrevious,40).reliable);
  MotionDirectionTracker flowReversal, flowMultiple;
  for (uint32_t t = 0; t < 1200; t += 50) {
    flowReversal.update(t, {true,true,true,false,80,false,true,t < 600 ? 5.f : -5.f});
    flowMultiple.update(t, {true,true,true,true,80,false,true,5});
  }
  assert(flowReversal.liveDirection() == MotionDirection::UNKNOWN);
  assert(flowMultiple.liveDirection() == MotionDirection::UNKNOWN);
  for (uint32_t t = 1200; t < 2000; t += 50) { flowReversal.update(t, {}); flowMultiple.update(t, {}); }
  assert(flowReversal.event().direction == MotionDirection::UNKNOWN);
  assert(flowMultiple.event().direction == MotionDirection::UNKNOWN);
  auto ltr = trajectory({20,32,44,56,68,80,92,104,116,128,140});
  assert(ltr.direction == MotionDirection::LEFT_TO_RIGHT && ltr.dx > 32);
  auto rtl = trajectory({140,128,116,104,92,80,68,56,44,32,20});
  assert(rtl.direction == MotionDirection::RIGHT_TO_LEFT && rtl.dx < -32);
  MotionDirectionTracker display;
  assert(display.liveDirection() == MotionDirection::UNKNOWN);
  uint32_t displayAt = 0;
  for (float x : {20.f,40.f,60.f,80.f,100.f,120.f}) {
    assert(!display.update(displayAt, point(x))); displayAt += 100;
  }
  assert(display.liveDirection() == MotionDirection::LEFT_TO_RIGHT);
  assert(display.event().direction == MotionDirection::UNKNOWN); // Live candidate is not yet an event.
  for (unsigned i = 0; i < finishFrames; ++i) { display.update(displayAt, {}); displayAt += 100; }
  assert(display.liveDirection() == MotionDirection::UNKNOWN);
  assert(display.event().direction == MotionDirection::LEFT_TO_RIGHT);
  assert(trajectory({80,81,80,82,83,81}).direction == MotionDirection::UNKNOWN);
  assert(trajectory({20,40,60,80,100,80,60,80,100}).direction == MotionDirection::UNKNOWN);
  assert(trajectory({20,40,110,130,140}).direction == MotionDirection::UNKNOWN); // Association jump.
  assert(trajectory({20,40,60,80,100}, true).direction == MotionDirection::UNKNOWN);
  assert(trajectory({20,40}).direction == MotionDirection::UNKNOWN); // Too few observations.
  // Mirroring reverses the screen direction, not just the leave/arrive mapping.
  assert(trajectory({140,120,100,80,60,40,20}).direction == MotionDirection::RIGHT_TO_LEFT);
  assert(passageAction(MotionDirection::LEFT_TO_RIGHT, false) == PassageAction::LEAVE);
  assert(passageAction(MotionDirection::LEFT_TO_RIGHT, true) == PassageAction::ARRIVE);
  assert(passageAction(MotionDirection::RIGHT_TO_LEFT, false) == PassageAction::ARRIVE);
  assert(passageAction(MotionDirection::RIGHT_TO_LEFT, true) == PassageAction::LEAVE);
  assert(passageAction(MotionDirection::UNKNOWN, true) == PassageAction::NONE);

  MotionDirectionTracker still;
  for (uint32_t t = 0; t < 5000; t += 100) assert(!still.update(t, {}));
  assert(still.state() == MotionTrackState::IDLE);
  assert(!still.update(5000, point(20)));
  assert(!still.update(5100, {})); // Isolated moving frame never begins a track.
  assert(still.state() == MotionTrackState::IDLE);

  MotionDirectionTracker cooldown;
  uint32_t now = 0;
  for (float x : {20.f,40.f,60.f,80.f,100.f,120.f}) { assert(!cooldown.update(now, point(x))); now += 100; }
  unsigned settledEvents = 0;
  for (int i = 0; i < 30; ++i) {
    if (cooldown.update(now, {true,false,false,false,-1,false})) ++settledEvents;
    now += 100;
  }
  assert(settledEvents == 1); // Exposure/background mismatch must not block finalization/rearm.
  assert(cooldown.state() == MotionTrackState::IDLE);
  assert(cooldown.event().direction == MotionDirection::LEFT_TO_RIGHT);
  for (int i = 0; i < 25; ++i) { assert(!cooldown.update(now, {})); now += 100; }
  assert(cooldown.state() == MotionTrackState::IDLE);
  for (float x : {140.f,120.f,100.f,80.f,60.f,40.f}) { assert(!cooldown.update(now, point(x))); now += 100; }
  for (unsigned i = 0; i < finishFrames; ++i) { bool event = cooldown.update(now, {}); now += 100; assert(event == (i + 1 == finishFrames)); }
  assert(cooldown.event().direction == MotionDirection::RIGHT_TO_LEFT);
  MotionDirectionTracker dropout;
  uint32_t dropoutAt = 0;
  for (float x : {20.f,32.f,44.f,56.f,68.f,80.f,92.f,104.f,116.f,128.f,140.f}) {
    assert(!dropout.update(dropoutAt, point(x, x == 68.f)));
    dropoutAt += 50;
    if (x == 80.f) {
      assert(!dropout.update(dropoutAt, {true,true,false,false,-1,false}));
      dropoutAt += 50;
    }
  }
  unsigned dropoutEvents = 0;
  for (int i = 0; i < 100; ++i) {
    if (dropout.update(dropoutAt, {true,false,false,false,-1,false})) ++dropoutEvents;
    dropoutAt += 50;
  }
  assert(dropoutEvents == 1 && dropout.event().direction == MotionDirection::LEFT_TO_RIGHT);
  assert(dropout.state() == MotionTrackState::IDLE);
  cooldown.restart(); // Explicit PAUSE/RUN recovers even if the old background never returns.
  assert(cooldown.state() == MotionTrackState::IDLE);
  assert(cooldown.xCurrent() == -1);
  assert(!cooldown.update(now + 1000, {}));

  MotionDirectionTracker interrupted;
  assert(!interrupted.update(0, point(20)));
  assert(!interrupted.update(100, point(40)));
  assert(interrupted.update(800, point(100))); // A missing interval must never infer a direction.
  assert(interrupted.event().direction == MotionDirection::UNKNOWN && interrupted.event().interrupted);
  MotionDirectionTracker paused;
  paused.update(0, point(20)); paused.update(100, point(40));
  assert(paused.update(200, {false,false,false,false,-1}));
  assert(paused.event().direction == MotionDirection::UNKNOWN);
  MotionDirectionTracker timeout;
  unsigned events = 0;
  for (uint32_t t = 0; t < 10000; t += 100) if (timeout.update(t, point(80))) ++events;
  assert(events == 1 && timeout.event().timedOut && timeout.event().direction == MotionDirection::UNKNOWN);
  MotionDirectionTracker wrapped;
  uint32_t tick = 0xffffff00u;
  for (float x : {20.f,40.f,60.f,80.f,100.f,120.f}) { assert(!wrapped.update(tick, point(x))); tick += 100; }
  for (unsigned i = 0; i < finishFrames; ++i) { bool event = wrapped.update(tick, {}); tick += 100; assert(event == (i + 1 == finishFrames)); }
  assert(wrapped.event().direction == MotionDirection::LEFT_TO_RIGHT);

  uint8_t previous[1200], current[1200], mirrored[1200];
  std::fill_n(previous, 1200, 80); std::fill_n(current, 1200, 80);
  MotionRegionExtractor extractor;
  auto empty = extractor.extract(current, previous, 0);
  assert(!empty.usable && empty.pixels == 0);
  for (int i = 0; i < 1200; i += 80) current[i] = 180;
  assert(!extractor.extract(current, previous, 0).usable); // Spatially isolated camera noise.
  std::fill_n(current, 1200, 80); rectangle(current, 2, 8, 4, 10, 180);
  auto left = extractor.extract(current, previous, 0);
  assert(left.usable && !left.ambiguous && left.x > 0 && left.x < 30);
  for (int y = 0; y < 30; ++y) for (int x = 0; x < 40; ++x) mirrored[y*40+x] = current[y*40+39-x];
  auto right = extractor.extract(mirrored, previous, 0);
  assert(std::fabs(left.x + right.x - 160) < .01f);
  rectangle(current, 32, 8, 4, 10, 180);
  assert(extractor.extract(current, previous, 0).ambiguous); // Two widely separated objects.
  std::fill_n(current, 1200, 80);
  rectangle(current, 10, 2, 4, 5, 180); rectangle(current, 10, 22, 4, 5, 180);
  assert(extractor.extract(current, previous, 0).ambiguous); // Vertically separated objects.
  std::fill_n(current, 1200, 130);
  auto lighting = compareFrames(current, previous, 1200, 20);
  assert(!extractor.extract(current, previous, lighting.lightShift).usable);
  for (int i = 0; i < 1200; ++i) { previous[i] = 30 + i % 120; current[i] = previous[i] * 3 / 2 + 10; }
  assert(backgroundDifference(current, previous, 1200, 20) == 0); // Exposure gain and offset after a pass.
  rectangle(current, 8, 4, 10, 20, 0);
  assert(backgroundDifference(current, previous, 1200, 20) > 5); // A stopped object cannot be learned away.
  imagePass(false); imagePass(true);
  imagePass(false, 2, 33); imagePass(true, 2, 33);
  MotionDirectionTracker fast;
  uint32_t fastAt = 0;
  for (float x : {20.f,32.f,44.f,56.f,68.f,80.f,92.f,104.f,116.f,128.f,140.f}) {
    assert(!fast.update(fastAt, point(x))); fastAt += 33;
  }
  for (int i = 0; i < 3; ++i) { assert(!fast.update(fastAt, {})); fastAt += 33; }
  assert(fast.state() == MotionTrackState::TRACKING); // Three fast frames must not end a track too soon.
  bool fastEvent = false;
  for (int i = 0; i < 12; ++i) { fastEvent = fast.update(fastAt, {}) || fastEvent; fastAt += 33; }
  assert(fastEvent && fast.event().direction == MotionDirection::LEFT_TO_RIGHT);
  puts("Direction: both directions, jitter, reversal, ambiguity, mirror, mapping, stationary scene, occupancy/cooldown, gaps, pause, timeout and rollover passed");
}
