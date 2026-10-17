#pragma once
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <atomic>
#include "motion_score.h"
#include "CameraMotionGate.h"
#include "CameraFrameSample.h"
#include "MotionRegion.h"
#include "MotionDirectionTracker.h"
#include "MotionBackground.h"
#include "HorizontalMotion.h"

class CameraMotionDetector {
 public:
  struct Result {
    float score = 0;
    uint32_t frames = 0, count = 0, processingMs = 0, errors = 0;
    bool present = false, ready = false;
    float fps = 0, previewFps = 0;
    uint32_t captureMs = 0;
    int status = 0; // 0 starting, 1 running, 2 warming up, negative error.
    MotionTrackState trackState = MotionTrackState::IDLE;
    MotionDirectionEvent directionEvent;
    MotionDirection liveDirection = MotionDirection::UNKNOWN;
    PassageAction action = PassageAction::NONE;
    uint32_t directionCount = 0, directionAt = 0;
    float xStart = -1, xCurrent = -1;
    unsigned regions = 0;
    bool ambiguous = false;
    bool sceneClear = true;
  };
  std::atomic<int> thresholdPercent{3};
  std::atomic<bool> enabled{true}, cameraBusReady{false};
  std::atomic<bool> invertArrivalDeparture{MotionDirectionConfig::invertArrivalDeparture};
  std::atomic<bool> restartRequested{false};
 private:
  SemaphoreHandle_t mutex = nullptr;
  uint16_t* preview = nullptr;
  uint16_t* displayPreview = nullptr;
  std::atomic<unsigned> previewRate10{0};
  uint32_t lastDrawnFrame = 0, previewWindowAt = 0, previewWindowFrames = 0;
  Result result;
  CameraMotionGate gate;
  MotionRegionExtractor regionExtractor;
  MotionDirectionTracker directionTracker;
  uint8_t directionBackground[MotionDirectionConfig::samplePixels] = {};
  uint8_t comparisonFrames[MotionDirectionConfig::differenceLagFrames][MotionDirectionConfig::samplePixels] = {};
  bool haveDirectionBackground = false;
  void logDirectionTransition(MotionTrackState before, bool event) {
    if (before != directionTracker.state()) {
      if (directionTracker.state() == MotionTrackState::TRACKING) {
        Serial.println("[MOTION] Tracking started");
        Serial.printf("[MOTION] x_start=%.1f, x_current=%.1f\n", directionTracker.xStart(), directionTracker.xCurrent());
      }
      if (directionTracker.state() == MotionTrackState::COOLDOWN) Serial.println("[MOTION] Cooldown");
    }
    if (event) {
      const auto& e = directionTracker.event();
      Serial.printf("[MOTION] Direction: %s x_start=%.1f x_end=%.1f dx=%.1f frames=%u ambiguous=%d flags=%u interrupted=%d timeout=%d flowDx=%.1f flowFrames=%u\n",
                    directionName(e.direction), e.xStart, e.xEnd, e.dx, e.frames,
                    e.ambiguous, e.ambiguityFlags, e.interrupted, e.timedOut, e.flowDx, e.flowFrames);
    }
  }
  // Caller holds result mutex. The event sequence is consumed once by the UI/audio hook.
  void publishDirection(bool event) {
    result.trackState = directionTracker.state();
    result.liveDirection = directionTracker.liveDirection();
    result.xStart = directionTracker.xStart(); result.xCurrent = directionTracker.xCurrent();
    if (event) {
      result.directionEvent = directionTracker.event();
      result.directionAt = millis();
      result.action = passageAction(result.directionEvent.direction, invertArrivalDeparture.load());
      ++result.directionCount;
    }
  }
  static void taskEntry(void* arg) { static_cast<CameraMotionDetector*>(arg)->run(); vTaskDelete(nullptr); }
  void fail(int code) {
    xSemaphoreTake(mutex, portMAX_DELAY); result.status = code; xSemaphoreGive(mutex);
    cameraBusReady.store(true);
    Serial.printf("[MOTION] fatal=%d\n", code);
  }
  void run() {
    camera_config_t c = {};
    c.pin_pwdn = c.pin_reset = c.pin_xclk = -1;
    c.pin_sccb_sda = c.pin_sccb_scl = -1;
    c.sccb_i2c_port = M5.In_I2C.getPort();
    c.pin_d7 = 47; c.pin_d6 = 48; c.pin_d5 = 16; c.pin_d4 = 15;
    c.pin_d3 = 42; c.pin_d2 = 41; c.pin_d1 = 40; c.pin_d0 = 39;
    c.pin_vsync = 46; c.pin_href = 38; c.pin_pclk = 45;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0; c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_RGB565; c.frame_size = FRAMESIZE_QQVGA;
    c.fb_count = 2; c.fb_location = CAMERA_FB_IN_PSRAM; c.grab_mode = CAMERA_GRAB_LATEST;
    esp_err_t error = esp_camera_init(&c);
    cameraBusReady.store(true);
    Serial.printf("[CAMERA] init=0x%x freeHeap=%u freePSRAM=%u\n", error, ESP.getFreeHeap(), ESP.getFreePsram());
    if (error != ESP_OK) { fail(-2); return; }
    uint8_t current[MotionDirectionConfig::samplePixels] = {};
    bool havePrevious = false;
    unsigned historyIndex = 0, historyCount = 0;
    uint32_t warmupAt = millis(), lastFrame = 0;
    uint32_t fpsAt = millis(), fpsFrames = 0, trackingLogAt = 0;
    int previousThreshold = thresholdPercent.load();
    while (true) {
      uint32_t started = millis();
      if (restartRequested.exchange(false)) {
        directionTracker.restart(); gate.reset(); havePrevious = false;
        historyIndex = historyCount = 0;
        haveDirectionBackground = false; warmupAt = millis();
        Serial.println("[MOTION] RUN: relearning background for 1500ms");
      }
      camera_fb_t* frame = esp_camera_fb_get();
      uint32_t capturedAt = millis();
      if (!frame || frame->format != PIXFORMAT_RGB565 || frame->width != 160 || frame->height != 120 ||
          frame->len < 160 * 120 * 2) {
        if (frame) esp_camera_fb_return(frame);
        gate.reset(); havePrevious = false;
        historyIndex = historyCount = 0;
        MotionTrackState before = directionTracker.state();
        bool event = directionTracker.update(millis(), {false, false, false, false, -1});
        logDirectionTransition(before, event);
        xSemaphoreTake(mutex, portMAX_DELAY);
        ++result.errors; result.present = result.ready = false;
        publishDirection(event);
        xSemaphoreGive(mutex);
        vTaskDelay(pdMS_TO_TICKS(100)); continue;
      }
      for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 40; ++x) {
          int rawX = MotionDirectionConfig::mirrorImage ? 39 - x : x;
          current[y * 40 + x] = cameraBlockLuma(frame->buf, frame->width, rawX * 4, y * 4);
        }
      xSemaphoreTake(mutex, portMAX_DELAY);
      if (!MotionDirectionConfig::mirrorImage) memcpy(preview, frame->buf, 160 * 120 * 2);
      else {
        uint8_t* dst = reinterpret_cast<uint8_t*>(preview);
        for (int y = 0; y < 120; ++y)
          for (int x = 0; x < 160; ++x) {
            size_t to = (y * 160 + x) * 2, from = (y * 160 + 159 - x) * 2;
            dst[to] = frame->buf[from]; dst[to + 1] = frame->buf[from + 1];
          }
      }
      xSemaphoreGive(mutex);
      esp_camera_fb_return(frame);
      uint32_t now = millis();
      bool gap = havePrevious && uint32_t(now - lastFrame) > MotionDirectionConfig::maxFrameGapMs;
      if (gap) historyIndex = historyCount = 0;
      bool continuous = havePrevious && historyCount >= MotionDirectionConfig::differenceLagFrames && !gap;
      const uint8_t* previous = comparisonFrames[historyIndex];
      FrameScore score = continuous ? compareFrames(current, previous, 40 * 30, MotionDirectionConfig::pixelDelta) : FrameScore{0, 0};
      MotionRegion region = continuous ? regionExtractor.extract(current, previous, score.lightShift) : MotionRegion{};
      auto flow = continuous && enabled.load() && score.percent >= thresholdPercent.load() ?
                  estimateHorizontalMotion(current, previous, score.lightShift) : HorizontalMotion{};
      memcpy(comparisonFrames[historyIndex], current, sizeof(current));
      historyIndex = (historyIndex + 1) % MotionDirectionConfig::differenceLagFrames;
      if (historyCount < MotionDirectionConfig::differenceLagFrames) ++historyCount;
      havePrevious = true; lastFrame = now;
      int threshold = thresholdPercent.load();
      bool settingsChanged = threshold != previousThreshold;
      if (settingsChanged) { gate.reset(); previousThreshold = threshold; }
      bool warmedUp = uint32_t(now - warmupAt) >= 1500;
      // Uniform exposure shifts are already subtracted by compareFrames.
      // Do not discard large mean shifts: a close passing object can cause those too.
      bool valid = continuous && warmedUp && enabled.load() && !settingsChanged;
      bool detected = gate.update(now, score.percent, threshold, valid);
      MotionTrackState before = directionTracker.state();
      if (!haveDirectionBackground || !warmedUp ||
          (valid && before == MotionTrackState::IDLE && score.percent < threshold)) {
        memcpy(directionBackground, current, sizeof(directionBackground));
        haveDirectionBackground = true;
      }
      float backgroundPercent = backgroundDifference(current, directionBackground, 40 * 30, MotionDirectionConfig::pixelDelta);
      bool sceneClear = backgroundPercent < MotionDirectionConfig::backgroundClearPercent;
      bool directionEvent = directionTracker.update(now, {valid, score.percent >= threshold,
                                                         region.usable, region.ambiguous, region.x, sceneClear, flow.reliable, flow.dx});
      logDirectionTransition(before, directionEvent);
      if (directionTracker.state() == MotionTrackState::TRACKING && uint32_t(now - trackingLogAt) >= 150) {
        trackingLogAt = now;
        Serial.printf("[MOTION] x_start=%.1f, x_current=%.1f raw=%.1f regions=%u ambiguous=%d usable=%d flow=%.1f votes=%u reliable=%d\n",
                      directionTracker.xStart(), directionTracker.xCurrent(), region.x, region.components,
                      region.ambiguous, region.usable, flow.dx, flow.votes, flow.reliable);
      }
      xSemaphoreTake(mutex, portMAX_DELAY);
      result.score = score.percent / 100.0f; result.processingMs = millis() - capturedAt;
      result.captureMs = capturedAt - started;
      ++fpsFrames;
      if (uint32_t(now - fpsAt) >= 1000) {
        result.fps = fpsFrames * 1000.0f / uint32_t(now - fpsAt);
        fpsFrames = 0; fpsAt = now;
      }
      result.present = valid && score.percent >= threshold; result.ready = gate.ready();
      result.status = warmedUp ? 1 : 2; ++result.frames;
      if (detected) ++result.count;
      result.regions = region.components; result.ambiguous = region.ambiguous;
      result.sceneClear = sceneClear;
      publishDirection(directionEvent);
      Result copy = result;
      xSemaphoreGive(mutex);
      static uint32_t loggedAt = 0;
      if (detected || uint32_t(now - loggedAt) >= 500) {
        loggedAt = now;
        Serial.printf("[MOTION] change=%.1f%% threshold=%d shift=%d ready=%d count=%lu frame=%lu errors=%lu state=%s regions=%u ambiguous=%d bg=%.1f%% clear=%d fps=%.1f lcd=%.1f capture=%lums process=%lums\n",
                      score.percent, threshold, score.lightShift, copy.ready, (unsigned long)copy.count,
                      (unsigned long)copy.frames, (unsigned long)copy.errors, trackStateName(copy.trackState),
                      copy.regions, copy.ambiguous, backgroundPercent, sceneClear, copy.fps,
                      previewRate10.load() / 10.0f, (unsigned long)copy.captureMs, (unsigned long)copy.processingMs);
      }
      if (detected) Serial.println("[DETECT] CAMERA MOTION");
      uint32_t elapsed = millis() - started;
      vTaskDelay(pdMS_TO_TICKS(elapsed < MotionDirectionConfig::captureIntervalMs ?
                              MotionDirectionConfig::captureIntervalMs - elapsed : 1));
    }
  }
 public:
  bool begin() {
    mutex = xSemaphoreCreateMutex();
    preview = static_cast<uint16_t*>(heap_caps_calloc(160 * 120, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    displayPreview = static_cast<uint16_t*>(heap_caps_calloc(160 * 120, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!mutex || !preview || !displayPreview || xTaskCreatePinnedToCore(taskEntry, "camera-motion", 8192, this, 1, nullptr, 0) != pdPASS) {
      result.status = -3; cameraBusReady.store(true); return false;
    }
    return true;
  }
  Result snapshot(bool drawPreview) {
    if (!mutex) return result;
    if (xSemaphoreTake(mutex, drawPreview ? pdMS_TO_TICKS(2) : portMAX_DELAY) != pdTRUE) return Result{};
    Result copy = result;
    bool draw = drawPreview && preview && displayPreview && copy.frames && copy.frames != lastDrawnFrame;
    if (draw) memcpy(displayPreview, preview, 160 * 120 * 2);
    xSemaphoreGive(mutex);
    // LCD transfer uses a private copy, and never holds the camera mutex.
    if (draw) {
      M5.Display.pushImage(0, 28, 160, 120, displayPreview);
      M5.Display.drawRect(0, 28, 160, 120, TFT_YELLOW);
      M5.Display.drawFastVLine(53, 28, 120, TFT_DARKGREY);
      M5.Display.drawFastVLine(107, 28, 120, TFT_DARKGREY);
      if (copy.trackState == MotionTrackState::TRACKING && copy.xCurrent >= 0)
        M5.Display.fillCircle(int(copy.xCurrent), 88, 3, TFT_RED);
      lastDrawnFrame = copy.frames;
      uint32_t now = millis();
      if (!previewWindowAt) previewWindowAt = now;
      ++previewWindowFrames;
      if (uint32_t(now - previewWindowAt) >= 1000) {
        previewRate10.store(previewWindowFrames * 10000 / uint32_t(now - previewWindowAt));
        previewWindowFrames = 0; previewWindowAt = now;
      }
    }
    copy.previewFps = previewRate10.load() / 10.0f;
    return copy;
  }
};
