#include <M5Unified.h>
#include "Config.h"
#include "Storage.h"
// Shared state must precede application audio/API headers.
Storage storage;
#include "AudioManager.h"
#include "EventManager.h"
#include "ApiClient.h"
AudioManager audio(storage);
EventManager events(storage);
ApiClient api(storage, events);
bool storageOK = false;
String serialLine;
bool lineOverflow = false;

void printStatus() {
  StorageGuard guard(storage);
  Serial.printf("[STATUS] device=%s wifi=%d ip=%s HTTP=%d FS=%d PSRAM=%u\n",
                Config::deviceId, WiFi.status() == WL_CONNECTED,
                WiFi.localIP().toString().c_str(), api.code(), storage.mounted, ESP.getPsramSize());
  Serial.printf("[STATUS] leave=%d arrive=%d volume=%u audio=%s\n",
                storage.settings.leave, storage.settings.arrive, storage.settings.volume,
                audio.status.c_str());
  for (int slot = 0; slot < 2; ++slot) {
    File f = LittleFS.open(storage.path(slot, storage.records[slot].bank), FILE_READ);
    Serial.printf("[CACHE] %s version=%lld valid=%d\n", slot ? "leave" : "arrive",
                  (long long)storage.records[slot].version, f && validWav(f));
  }
}

void command(String line) {
  line.trim();
  line.toUpperCase();
  if (line.isEmpty()) return;
  if (line == "LEFT_TO_RIGHT" || line == "RIGHT_TO_LEFT") {
    const int slot = line == "LEFT_TO_RIGHT" ? 1 : 0;
    // Playback never depends on connectivity, NTP or successful event submission.
    bool played = audio.play(slot);
    Serial.printf("[DIRECTION] %s -> %s playback=%s status=%s\n", line.c_str(),
                  slot ? "left_home" : "arrived_home", played ? "started" : "skipped",
                  audio.status.c_str());
    bool queued = events.enqueue(slot);
    Serial.printf("[EVENT] queued=%d %s\n", queued, events.status.c_str());
  } else if (line == "STATUS") printStatus();
  else if (line == "POLL") api.request();
  else if (line == "CHIME") audio.testChime();
  else if (line == "INIT_FS") {
    StorageGuard guard(storage);
    if (storage.mounted) Serial.println("[FS] Already mounted; initialization refused");
    else {
      storage.mounted = LittleFS.format() && LittleFS.begin(false);
      Serial.printf("[FS] Explicit initialization: %s\n", storage.mounted ? "OK" : "FAILED");
      api.request();
    }
  }
  else Serial.println("Commands: LEFT_TO_RIGHT, RIGHT_TO_LEFT, STATUS, POLL, CHIME, INIT_FS, HELP (newline)");
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.external_display_value = 0;
  cfg.external_speaker.atomic_echo = true;
  M5.begin(cfg);
  Serial.begin(115200);
  delay(1000);
  Serial.printf("[BOOT] AtomS3R-CAM device=%s PSRAM=%u\n", Config::deviceId, ESP.getPsramSize());
  storageOK = storage.begin();
  if (!storageOK) {
    Serial.println("[FATAL] Storage/identity initialization failed; API and playback stopped");
    return;
  }
  if (!storage.mounted) Serial.println("[FS] unavailable; see README first-provisioning procedure");
  // Keep Echo Base speaker active for repeated WAV playback; no camera/PSoC initialization.
  audio.begin();
  if (!api.start()) Serial.println("[API] task creation failed");
  command("HELP");
}

void loop() {
  M5.update();
  if (!storageOK) { delay(20); return; }
  audio.update();
  // Bounded, nonblocking newline parser; discard the entire oversized line.
  for (int n = 0; n < 128 && Serial.available(); ++n) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (!lineOverflow) command(serialLine);
      else Serial.println("[SERIAL] line too long");
      serialLine = "";
      lineOverflow = false;
    } else if (!lineOverflow) {
      if (serialLine.length() < 64) serialLine += c;
      else lineOverflow = true;
    }
  }
  delay(5);
}
