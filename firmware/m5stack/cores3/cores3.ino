#define CORES3_DETECTION_ONLY 1
#if CORES3_DETECTION_ONLY
#include "CameraOnly.h"
#else
#include <M5Unified.h>
#include <esp_sntp.h>
#include "Config.h"
#include "Storage.h"
// Shared state precedes audio/storage-related application headers.
Storage storage;
#include "AudioManager.h"
#include "ProximityDetector.h"
#include "EventManager.h"
#include "ApiClient.h"
#include "DisplayManager.h"
AudioManager audio(storage);
ProximityDetector proximity;
EventManager events(storage);
ApiClient api(storage, events);
DisplayManager display;
bool storageOK = false;
void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  display.begin();
  Serial.printf("[BOOT] entrance CoreS3 flash=%u PSRAM=%u\n", ESP.getFlashChipSize(),
                ESP.getPsramSize());
  storageOK = storage.begin();
  if (!storageOK) {
    Serial.println("[FATAL] NVS/mutex unavailable");
    return;
  }
  // RTC is used only after this firmware has set it from trusted NTP and voltage is valid.
  setenv("TZ", "UTC0", 1);
  tzset();
  if (storage.prefs.getBool("rtcTrusted", false) && M5.Rtc.isEnabled() && !M5.Rtc.getVoltLow())
    M5.Rtc.setSystemTimeFromRtc();
  proximity.setOffset(storage.prefs.getUShort("proxOffset", Config::nearDelta));
  proximity.begin();
  audio.begin();
  if (!api.start()) Serial.println("[API] task creation failed");
}
void loop() {
  M5.update();
  if (!storageOK) {
    M5.Display.setCursor(0, 0);
    M5.Display.print("Storage initialization failed");
    delay(20);
    return;
  }
  audio.update();
  uint32_t now = millis();
  int mode;
  {
    StorageGuard g(storage);
    mode = storage.leaveMode ? 1 : 0;
  }
  if (proximity.update(now, audio.busy())) {
    if (display.tuning) {
      if (display.chime) audio.testChime();
    } else {
      audio.play(mode);
      if (ENABLE_AUTO_EVENTS) events.enqueue(mode);
    }
    Serial.printf("[DETECT] raw=%u filtered=%u base=%u count=%lu tuning=%d\n", proximity.raw,
                  proximity.filtered, proximity.baseline, (unsigned long)proximity.detections,
                  display.tuning);
  }
  int displayAction = display.touch();
  // A serial fallback separates a touch issue from a detector or speaker issue.
  if (Serial.available()) {
    char command = Serial.read();
    if (command == '+')
      displayAction = 7;
    else if (command == '-')
      displayAction = 8;
    else if (command == 't')
      displayAction = 11;
    else if (command == 'c')
      displayAction = 9;
    else if (command == 'r') {
      proximity.resetStats();
      display.message("Counters reset");
    }
  }
  switch (displayAction) {
    case 0: {
      StorageGuard g(storage);
      if (!storage.setMode(!storage.leaveMode)) Serial.println("[MODE] save failed");
      break;
    }
    case 1:
      audio.play(0);
      break;
    case 2:
      audio.play(1);
      break;
    case 3:
      api.request();
      break;
    case 4:
      events.enqueue(mode);
      break;
    case 5:
    case 9:
      proximity.calibrate();
      display.message("CAL requested: clear front");
      break;
    case 6:
    case 12:
      display.tuning = !display.tuning;
      proximity.calibrate();
      break;
    case 7:
    case 8: {
      // SENS + means a smaller required change from the background.
      int before = proximity.offset();
      int step = before <= 50 ? 5 : 25;
      int target = std::max(5, std::min(1000, before + (displayAction == 7 ? -step : step)));
      if (target == before) {
        display.message("Sensitivity at MIN/MAX");
      } else if (proximity.setOffset(target)) {
        bool saved;
        {
          StorageGuard g(storage);
          saved = storage.prefs.putUShort("proxOffset", proximity.offset()) == 2;
        }
        display.message(String("Offset ") + before + " -> " + proximity.offset() +
                        (saved ? " SAVED" : " SAVE FAILED"));
      } else {
        display.message("Rejected: baseline too high");
      }

      break;
    }
    case 10:
      display.chime = !display.chime;
      display.message(display.chime ? "Detection chime ON" : "Detection chime OFF");
      break;
    case 11:
      display.message(audio.testChime() ? "TEST SOUND started" : "TEST SOUND failed/busy");
      break;
  }
  static uint32_t logAt = 0;
  if (now - logAt >= 250) {
    logAt = now;
    Serial.printf(
        "[PROX] raw=%u filtered=%u base=%u noise=%u near=%u clear=%u state=%s count=%lu "
        "invalid=%lu\n",
        proximity.raw, proximity.filtered, proximity.baseline, proximity.noise, proximity.high,
        proximity.low, proximity.state, (unsigned long)proximity.detections,
        (unsigned long)proximity.invalidSamples);
  }
  // SNTP status distinguishes an actual NTP update from RTC-restored system time.
  static uint32_t rtcAt = 0;
  if (now - rtcAt > 60000 && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    rtcAt = now;
    time_t t = time(nullptr);
    tm utc;
    gmtime_r(&t, &utc);
    if (M5.Rtc.isEnabled()) {
      M5.Rtc.setDateTime(&utc);
      StorageGuard g(storage);
      if (!storage.prefs.getBool("rtcTrusted", false)) storage.prefs.putBool("rtcTrusted", true);
    }
  }
  display.update(now, storage, api, audio, proximity, events);
  delay(2);
}

#endif
