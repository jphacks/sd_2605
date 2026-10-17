#pragma once
#include <M5Unified.h>
// The standalone detector deliberately has no network, filesystem or NVS dependencies.
namespace Config {
constexpr uint32_t sampleMs = 75, cooldownMs = 10000;
constexpr uint16_t nearDelta = 150, releaseDelta = 70;
}
#include "ProximityDetector.h"
#include "TouchInput.h"
ProximityDetector detector;
TouchInput detectorTouch;
bool detectorChime = true, detectorSpeakerOK = false;
String detectorMessage = "Clear front for calibration";

// Poll FT6336 directly through M5Unified's internal I2C. The CoreS3-specific
// M5GFX reader gates reads on the shared interrupt line; polling avoids that gate.
int detectorReadTouch(uint32_t now) {
  static uint32_t readAt = 0;
  if (now - readAt < 20) return -1;
  readAt = now;
  uint8_t data[5] = {};
  bool valid = M5.In_I2C.readRegister(0x38, 0x02, data, sizeof(data), 400000);
  bool pressed = valid && (data[0] & 0x0f) > 0 && (data[0] & 0x0f) <= 2 &&
                 (data[1] >> 6) != 1;
  m5gfx::touch_point_t point = {};
  if (pressed) {
    point.x = ((data[1] & 0x0f) << 8) | data[2];
    point.y = ((data[3] & 0x0f) << 8) | data[4];
    M5.Display.convertRawXY(&point, 1);
  }
  int action = detectorTouch.poll(now, pressed, point.x, point.y, true);
  static bool wasDown = false;
  if (pressed && !wasDown) {
    detectorMessage = String("Touch ") + point.x + "," + point.y + " action=" + action;
    Serial.println("[TOUCH] " + detectorMessage);
  }
  wasDown = pressed;
  return action;
}

void detectorSound() {
  bool started = detectorSpeakerOK && !M5.Speaker.isPlaying() && M5.Speaker.tone(1200, 300);
  Serial.printf("[SOUND] started=%d\n", started);
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Mic.end();
  detectorSpeakerOK = M5.Speaker.begin();
  M5.Speaker.setVolume(128);
  const char* labels[] = {"SENS +", "SENS -", "CALIBRATE", "CHIME", "TEST SOUND", "RESET"};
  M5.Display.setTextSize(1);
  for (int i = 0; i < 6; ++i) {
    int x = (i % 3) * 107, y = 166 + (i / 3) * 37;
    M5.Display.drawRect(x, y, 106, 37, TFT_DARKGREY);
    M5.Display.setCursor(x + 8, y + 13);
    M5.Display.print(labels[i]);
  }
  detector.begin();
  uint8_t touchId = 0;
  bool touchOK = M5.In_I2C.readRegister(0x38, 0xa8, &touchId, 1, 400000);
  Serial.printf("[TOUCH] polling init=%d vendor=0x%02x rotation=%u size=%dx%d\n",
                touchOK, touchId, M5.Display.getRotation(), M5.Display.width(), M5.Display.height());
  Serial.printf("[BOOT] CoreS3 DETECTION ONLY sensor=%d speaker=%d; clear front\n",
                detector.ok, detectorSpeakerOK);
  Serial.printf("[MEM] freeHeap=%u freePSRAM=%u framebuffer=0\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop() {
  M5.update();
  uint32_t now = millis();
  if (detector.update(now, M5.Speaker.isPlaying())) {
    Serial.printf("[DETECT] count=%lu raw=%u filtered=%u\n",
                  (unsigned long)detector.detections, detector.raw, detector.filtered);
    if (detectorChime) detectorSound();
  }
  int action = detectorReadTouch(now);
  if (Serial.available()) {
    switch (Serial.read()) {
      case '+': action = 7; break;
      case '-': action = 8; break;
      case 'c': action = 9; break;
      case 't': action = 11; break;
      case 'r': action = 12; break;
    }
  }
  if (action == 7 || action == 8) {
    int before = detector.offset(), step = before <= 50 ? 5 : 25;
    bool changed = detector.setOffset(std::max(5, std::min(1000, before + (action == 7 ? -step : step))));
    Serial.printf("[SENS] offset=%u accepted=%d\n", detector.offset(), changed);
    detectorMessage = String("Offset ") + before + " -> " + detector.offset();
  } else if (action == 9) detector.calibrate();
  else if (action == 10) detectorChime = !detectorChime;
  else if (action == 11) detectorSound();
  else if (action == 12) detector.resetStats();
  static uint32_t drawnAt = 0;
  if (now - drawnAt >= 250) {
    drawnAt = now;
    Serial.printf("[PROX] raw=%u filtered=%u base=%u near=%u state=%s cal=%u/32 count=%lu invalid=%lu\n",
                  detector.raw, detector.filtered, detector.baseline, detector.high, detector.state,
                  detector.calibrationProgress(), (unsigned long)detector.detections,
                  (unsigned long)detector.invalidSamples);
    // Paint only changed text rows; no framebuffer allocation or full-screen clear.
    String rows[] = {
      "DETECTION ONLY",
      String(detector.state),
      String("Raw:") + detector.raw + " Filter:" + detector.filtered,
      String("Base:") + detector.baseline + " Near:" + detector.high,
      String("Offset:") + detector.offset() + " Cal:" + detector.calibrationProgress() + "/32",
      String("Count:") + detector.detections + " Chime:" + (detectorChime ? "ON" : "OFF"),
      String("Speaker:") + (detectorSpeakerOK ? "OK" : "ERROR")
    };
    static String previous[8];
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextPadding(320);
    for (int i = 0; i < 7; ++i) {
      if (previous[i] != rows[i]) {
        M5.Display.drawString(rows[i], 0, i * 20);
        previous[i] = rows[i];
      }
    }
    if (previous[7] != detectorMessage) {
      M5.Display.setTextSize(1);
      M5.Display.drawString(detectorMessage, 0, 148);
      previous[7] = detectorMessage;
    }
    M5.Display.setTextPadding(0);
  }
  delay(2);
}
