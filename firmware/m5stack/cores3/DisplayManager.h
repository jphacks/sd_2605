#pragma once
#include "TouchInput.h"
class DisplayManager {
  uint32_t at = 0;
  TouchInput input;
  M5Canvas canvas{&M5.Display};
  bool ready = false, buttonsReady = false, lastTuning = false;

 public:
  bool tuning = true, chime = true;
  bool begin() {
    // The changing region is drawn in RAM; the LCD never sees its clearing phase.
    canvas.setColorDepth(8);
    canvas.setPsram(true);
    ready = canvas.createSprite(320, 166) != nullptr;
    if (!ready) {
      canvas.setPsram(false);
      ready = canvas.createSprite(320, 166) != nullptr;
    }
    if (!ready) {
      Serial.println("[DISPLAY] sprite allocation failed");
      M5.Display.setCursor(3, 3);
      M5.Display.print("Display buffer unavailable");
    }
    return ready;
  }
  String feedback = "TEST SOUND checks speaker";
  void message(const String& text) {
    feedback = text;
    Serial.println("[UI] " + text);
    at = millis() - 250;
  }
  // 0 mode, 1 arrive play, 2 leave play, 3 heartbeat, 4 event, 5 calibration.
  int touch() {
    auto t = M5.Touch.getDetail();
    int action = input.poll(millis(), t.isPressed(), t.x, t.y, tuning);
    if (action >= 0) Serial.printf("[TOUCH] x=%d y=%d action=%d\n", t.x, t.y, action);
    return action;
  }
  void update(uint32_t now, Storage& s, ApiClient& api, AudioManager& audio,
              ProximityDetector& detector, EventManager& events) {
    if (!ready || now - at < 250) return;
    at = now;
    LocalSettings settings;
    AudioRecord records[2];
    bool mounted, leaveMode;
    {  // Do not hold the filesystem mutex during drawing or LCD transfers.
      StorageGuard guard(s);
      settings = s.settings;
      records[0] = s.records[0];
      records[1] = s.records[1];
      mounted = s.mounted;
      leaveMode = s.leaveMode;
    }
    auto& g = canvas;
    g.fillScreen(TFT_BLACK);
    g.setTextColor(TFT_WHITE, TFT_BLACK);
    g.setTextSize(1);
    g.setCursor(3, 3);
    if (tuning) {
      g.printf("SENSOR TUNING - tap top: API UI\n");
      g.printf("Raw:%u Filter:%u Peak:%u\n", detector.raw, detector.filtered, detector.peak);
      g.printf("Base:%u Noise:%u Offset:%u\n", detector.baseline, detector.noise,
               detector.offset());
      g.printf("Near:%u Clear:%u\n", detector.high, detector.low);
      g.printf("%s Cal:%u/32\nCount:%lu Invalid:%lu\n", detector.state,
               detector.calibrationProgress(), (unsigned long)detector.detections,
               (unsigned long)detector.invalidSamples);
      g.printf("Chime:%s Speaker:%s\nAudio:%s\n%s", chime ? "ON" : "OFF",
               audio.speakerOK ? "OK" : "ERROR", audio.status.c_str(), feedback.c_str());
    } else {
      g.printf("WiFi:%s API:%d FS:%s\nID: %s\nMode: %s  Volume: %u\n",
               WiFi.status() == WL_CONNECTED ? "ON" : "OFF", api.code(), mounted ? "OK" : "ERR",
               Config::deviceId, leaveMode ? "LEAVE" : "ARRIVE", settings.volume);
      g.printf("PS: %u  Base:%u\nNear:%u Clear:%u\n%s\n", detector.raw, detector.baseline,
               detector.high, detector.low, detector.state);
      g.printf("WAV A:%lld L:%lld\nAudio: %s\n%s\nClock:%s Auto events:%s",
               (long long)records[0].version, (long long)records[1].version, audio.status.c_str(),
               events.status.c_str(), time(nullptr) >= 1767225600 ? "UTC OK" : "WAIT NTP",
               ENABLE_AUTO_EVENTS ? "ON" : "OFF");
    }
    canvas.pushSprite(0, 0);
    // Button labels and backgrounds are static until the page changes.
    if (buttonsReady && lastTuning == tuning) return;
    buttonsReady = true;
    lastTuning = tuning;
    auto& panel = M5.Display;
    const char* labels[] = {"MODE",      "TEST ARRIVE", "TEST LEAVE",
                            "HEARTBEAT", "SEND EVENT",  "CALIBRATE"};
    const char* tuningLabels[] = {"SENS +",       "SENS -",     "CALIBRATE",
                                  "CHIME ON/OFF", "TEST SOUND", "API UI"};
    for (int i = 0; i < 6; i++) {
      int x = (i % 3) * 107, y = 166 + (i / 3) * 37;
      panel.fillRoundRect(x + 1, y + 1, 104, 34, 3, TFT_DARKGREY);
      panel.setTextColor(TFT_WHITE, TFT_DARKGREY);
      panel.drawCenterString(tuning ? tuningLabels[i] : labels[i], x + 53, y + 12);
    }
  }
};
