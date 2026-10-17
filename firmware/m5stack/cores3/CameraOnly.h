#pragma once
#include <M5Unified.h>
#include <algorithm>
#include "TouchInput.h"
#include "Config.h"
#include "Storage.h"
// Shared device state precedes camera/audio application headers.
bool cameraChime = true, cameraSpeakerOK = false;
Storage cameraStorage;
#include "EventManager.h"
#include "ApiClient.h"
EventManager cameraEvents(cameraStorage);
ApiClient cameraApi(cameraStorage, cameraEvents);
bool cameraStorageOK = false;
#include "CameraMotionDetector.h"
#include "CameraVoice.h"
#include "CameraStatusText.h"
#include "PirFusion.h"
PirFusion pirFusion;
CameraMotionDetector::Result fusionCandidate;
CameraMotionDetector vision;
TouchInput cameraTouch;
String cameraMessage = "Starting camera...";
uint32_t cameraMessageAt = 0;
void cameraNotify(const String& text) { cameraMessage = text; cameraMessageAt = millis(); }
String cameraNetworkStatus() {
  if (!cameraStorageOK) return "WiFi:OFF STORAGE ERR";
  if (!strlen(WIFI_SSID)) return "WiFi:NOT CONFIGURED";
  switch (WiFi.status()) {
    case WL_CONNECTED:
      if (!cameraApi.code()) return time(nullptr) < 1767225600 ? "WiFi:ON TIME WAIT" : "WiFi:ON API WAIT";
      return String("WiFi:ON API:") + cameraApi.code();
    case WL_NO_SSID_AVAIL: return "WiFi:NO SSID";
    case WL_CONNECT_FAILED: return "WiFi:CONNECT FAILED";
    case WL_CONNECTION_LOST: return "WiFi:LOST RETRYING";
    default: return "WiFi:CONNECTING";
  }
}
uint32_t cameraCountBase = 0, cameraLastCount = 0;
uint32_t cameraLastDirectionCount = 0, cameraDirectionBase = 0;

// Confirmed directions play their local voice; UNKNOWN maps to NONE.
void handleMotionDirection(const CameraMotionDetector::Result& event) {
  if (cameraChime && event.action != PassageAction::NONE) cameraPlayVoice(event.action);
  if (cameraStorageOK && ENABLE_AUTO_EVENTS && event.action != PassageAction::NONE)
    cameraEvents.enqueue(event.action == PassageAction::ARRIVE ? 0 : 1);
  Serial.printf("[DIRECTION EVENT] seq=%lu direction=%s action=%s\n", (unsigned long)event.directionCount,
                directionName(event.directionEvent.direction), actionName(event.action));
  cameraNotify(String(directionName(event.directionEvent.direction)) + " -> " + actionName(event.action));
}

void cameraSound() {
  bool started = cameraSpeakerOK && !M5.Speaker.isPlaying() && M5.Speaker.tone(1200, 300);
  Serial.printf("[SOUND] started=%d\n", started);
  if (started) Serial.println("[AUDIO] PLAY");
  cameraNotify(started ? "TEST SOUND started" : "SOUND failed/busy");
}
void fusionOutcome(PirFusion::Outcome outcome) {
  if (outcome == PirFusion::Outcome::CONFIRMED) {
    Serial.printf("[FUSION] CONFIRMED %s\n", directionName(fusionCandidate.directionEvent.direction));
    handleMotionDirection(fusionCandidate);

  } else if (outcome == PirFusion::Outcome::REJECTED) Serial.println("[FUSION] REJECTED");
}
int cameraReadTouch(uint32_t now) {
  static uint32_t readAt = 0;
  if (now - readAt < 20) return -1;
  readAt = now;
  uint8_t data[5] = {};
  bool valid = M5.In_I2C.readRegister(0x38, 0x02, data, sizeof(data), 400000);
  bool pressed = valid && (data[0] & 15) > 0 && (data[0] & 15) <= 2 && (data[1] >> 6) != 1;
  m5gfx::touch_point_t point = {};
  if (pressed) {
    point.x = ((data[1] & 15) << 8) | data[2];
    point.y = ((data[3] & 15) << 8) | data[4];
    M5.Display.convertRawXY(&point, 1);
  }
  int action = cameraTouch.poll(now, pressed, point.x, point.y, true);
  if (action >= 0) Serial.printf("[TOUCH] x=%d y=%d action=%d\n", point.x, point.y, action);
  return action;
}
void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  // Diagnostics must never throttle capture when the host stops reading USB.
  Serial.setTxTimeoutMs(0);
  Serial.setTxBufferSize(4096);
#endif
  pirFusion.begin(millis());
  if (PirConfig::PIR_ENABLED) pinMode(PirConfig::PIR_PIN, INPUT);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Mic.end();
  cameraSpeakerOK = M5.Speaker.begin();
  M5.Speaker.setVolume(128);
  cameraStorageOK = cameraStorage.begin();
  if (cameraStorageOK) {
    cameraVoiceBegin();
    if (!cameraApi.start()) Serial.println("[API] task creation failed");
  } else Serial.println("[FS] NVS/mutex initialization failed");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("CAMERA DIRECTION", 0, 2);
  const char* labels[] = {"SENS +", "SENS -", "PAUSE/RUN", "CHIME", "TEST SOUND", "RESET COUNT"};
  M5.Display.setTextSize(1);
  M5.Display.drawString("MAP", 286, 8);
  for (int i = 0; i < 6; ++i) {
    int x = (i % 3) * 107, y = 166 + (i / 3) * 37;
    M5.Display.drawRect(x, y, 106, 37, TFT_DARKGREY);
    M5.Display.drawString(labels[i], x + 8, y + 13);
  }
  Serial.printf("[BOOT] CAMERA MOTION; LTR553=UNUSED WiFi=ON speaker=%d\n", cameraSpeakerOK);
  Serial.printf("[DIRECTION] minDx=%.0f startFrames=%u minFrames=%u endFrames=%u cooldown=%lu mirror=%d invertMap=%d\n",
                MotionDirectionConfig::minDisplacement, MotionDirectionConfig::startFrames,
                MotionDirectionConfig::minTrackFrames, MotionDirectionConfig::endFrames,
                (unsigned long)MotionDirectionConfig::cooldownMs, MotionDirectionConfig::mirrorImage,
                vision.invertArrivalDeparture.load());
  if (!vision.begin()) cameraNotify("Vision task/memory failed");
}
void loop() {
  // Camera initialization configures the sensor on the shared I2C bus.
  // Let it finish before the UI starts polling that bus.
  if (vision.cameraBusReady.load()) M5.update();
  uint32_t now = millis();
  if (cameraStorageOK) cameraVoiceRefresh(now);
  if (PirConfig::PIR_ENABLED) {
    bool level = digitalRead(PirConfig::PIR_PIN) == HIGH;
    static bool first = true;
    if (first || level != pirFusion.high) {
      Serial.println(level ? "[PIR] HIGH" : "[PIR] LOW"); first = false;
    }
    pirFusion.sample(now, level);
    if (!vision.enabled.load()) pirFusion.cancel();
    else fusionOutcome(pirFusion.poll(now));
  }
  int action = vision.cameraBusReady.load() ? cameraReadTouch(now) : -1;
  if (Serial.available()) {
    switch (Serial.read()) {
      case '+': action = 7; break;
      case '-': action = 8; break;
      case 'p': action = 9; break;
      case 'm': action = 10; break;
      case 't': action = 11; break;
      case 'r': action = 12; break;
      case 'i': action = 6; break;
    }
  }
  if (action == 6) {
    vision.invertArrivalDeparture.store(!vision.invertArrivalDeparture.load());
    cameraNotify(vision.invertArrivalDeparture.load() ? "Map: LTR=ARRIVE RTL=LEAVE" : "Map: LTR=LEAVE RTL=ARRIVE");
  } else if (action == 7 || action == 8) {
    int threshold = vision.thresholdPercent.load() + (action == 7 ? -1 : 1);
    threshold = std::max(1, std::min(40, threshold));
    vision.thresholdPercent.store(threshold);
    cameraNotify(String("Threshold ") + threshold + "%");
  } else if (action == 9) {
    if (vision.enabled.load()) {
      vision.enabled.store(false);
      pirFusion.cancel();
      cameraNotify("PAUSED");
    } else {
      vision.restartRequested.store(true);
      vision.enabled.store(true);
      cameraNotify("RUN: learning background (clear view)");
    }
  } else if (action == 10) {
    cameraChime = !cameraChime;
    cameraNotify(cameraChime ? "Detection chime ON" : "Detection chime OFF");
  } else if (action == 11) cameraSound();
  else if (action == 12) {
    auto state = vision.snapshot(false);
    cameraCountBase = state.count; cameraDirectionBase = state.directionCount;
    cameraNotify("Count reset");
  }
  if (action >= 6) Serial.println("[UI] " + cameraMessage);
  // Dispatch audio independently of the 200ms text refresh, before LCD transfers.
  const auto eventState = vision.snapshot(false);
  if (eventState.directionCount != cameraLastDirectionCount) {
    cameraLastDirectionCount = eventState.directionCount;
    Serial.printf("[CAMERA] %s\n", directionName(eventState.directionEvent.direction));
    if (!PirConfig::PIR_ENABLED) handleMotionDirection(eventState);
    else if (vision.enabled.load() && eventState.directionEvent.direction != MotionDirection::UNKNOWN) {
      // Tracker cooldown exceeds the pending window: at most one candidate is live.
      fusionCandidate = eventState;
      auto outcome = pirFusion.camera(millis(), eventState.directionAt);
      if (outcome == PirFusion::Outcome::PENDING) Serial.println("[FUSION] PENDING");
      fusionOutcome(outcome);
    }
  }
  static uint32_t previewAt = 0;
  if (uint32_t(now - previewAt) >= MotionDirectionConfig::previewIntervalMs) {
    previewAt = now;
    vision.snapshot(true);
  }
  static uint32_t drawAt = 0;
  if (now - drawAt >= 200) {
    drawAt = now;
    auto state = vision.snapshot(false);
    if (state.count != cameraLastCount) {
      cameraLastCount = state.count;

      cameraNotify("MOTION DETECTED");
    }
    const char* status = state.status == 0 ? "STARTING" : state.status == 2 ? "WARMUP" :
                         state.status == -2 ? "CAMERA ERROR" : state.status == -3 ? "MEMORY ERROR" :
                         !vision.enabled.load() ? "PAUSED" : trackStateName(state.trackState);
    String rows[] = {String(status), String("Change:") + String(state.score * 100, 1) + " T:" + vision.thresholdPercent.load() + "%",
                     String("FPS:") + String(state.fps, 1) + " LCD:" + String(state.previewFps, 1),
                     String("Count:") + (state.count - cameraCountBase) + " Dir:" + (state.directionCount - cameraDirectionBase),
                     String(state.trackState == MotionTrackState::TRACKING ? "Now:" : "Last:") +
                         directionName(state.trackState == MotionTrackState::TRACKING ? state.liveDirection : state.directionEvent.direction),
                     String("x:") + int(state.xCurrent) + " dx:" + int(state.trackState == MotionTrackState::TRACKING ?
                         state.xCurrent - state.xStart : state.directionEvent.dx),
                     String("PIR:") + (!PirConfig::PIR_ENABLED ? "OFF" : !pirFusion.ready(now) ? "WARMUP" : pirFusion.high ? "DETECTED" : "IDLE") +
                         " F:" + (pirFusion.state == PirFusion::Outcome::PENDING ? "PENDING" : pirFusion.state == PirFusion::Outcome::CONFIRMED ? "CONFIRMED" : "NONE")};
    static String previous[8];
    M5.Display.setTextSize(1);
    M5.Display.setTextPadding(152);
    for (int i = 0; i < 7; ++i) {
      if (previous[i] != rows[i]) { M5.Display.drawString(rows[i], 168, 28 + i * 16); previous[i] = rows[i]; }
    }
    // Correlate the exact LCD label with the worker's finalized event sequence.
    static String lastDirectionLabel;
    static uint32_t lastDisplayedSequence = 0;
    if (rows[4] != lastDirectionLabel || state.directionCount != lastDisplayedSequence) {
      Serial.printf("[UI DIRECTION] seq=%lu state=%s label=%s final=%s\n",
                    (unsigned long)state.directionCount, trackStateName(state.trackState),
                    rows[4].c_str(), directionName(state.directionEvent.direction));
      lastDirectionLabel = rows[4]; lastDisplayedSequence = state.directionCount;
    }
    String footer = cameraNoticeActive(millis(), cameraMessageAt) ? cameraMessage :
                    cameraLiveStatus(state.status, vision.enabled.load(), state.trackState, state.sceneClear);
    if (footer != previous[7]) {
      M5.Display.setTextPadding(320);
      M5.Display.drawString(footer, 0, 151);
      Serial.println("[UI STATUS] " + footer);
      previous[7] = footer;
    }
    static String previousNetwork;
    String network = cameraNetworkStatus();
    if (network != previousNetwork) {
      M5.Display.setTextPadding(152);
      M5.Display.drawString(network, 168, 140);
      Serial.println("[UI WiFi] " + network);
      previousNetwork = network;
    }
    M5.Display.setTextPadding(0);
  }
  delay(2);
}
