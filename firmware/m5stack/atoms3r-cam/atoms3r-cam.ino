#include <M5Unified.h>
#include "Config.h"
#include "Storage.h"
// Shared state must precede application audio/API headers.
Storage storage;
#include "AudioManager.h"
#include "EventManager.h"
#include "ApiClient.h"
#include "LocalAudioTest.h"
AudioManager audio(storage);
EventManager events(storage);
ApiClient api(storage, events);
#include "UartEventReceiver.h"
UartEventReceiver receiver;
bool storageOK = false;
String serialLine;
bool lineOverflow = false;

void printStatus() {
  receiver.printStatus();
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
  if (line == "LEFT_TO_RIGHT" || line == "RIGHT_TO_LEFT" ||
      line == "PLAY_LEAVE" || line == "PLAY_ARRIVE" || line == "REPLAY") {
    using DoorProtocol::Action;
    Action a = line == "REPLAY" ? Action::Replay :
      (line == "LEFT_TO_RIGHT" || (line == "PLAY_LEAVE" && ATOM_LEFT_TO_RIGHT_IS_LEAVE) ||
       (line == "PLAY_ARRIVE" && !ATOM_LEFT_TO_RIGHT_IS_LEAVE)) ? Action::LeftToRight : Action::RightToLeft;
    Serial.printf("[QUEUE] accepted=%d\n", receiver.enqueue(a));
  } else if (line == "STATUS") printStatus();
  else if (line == "POLL") api.request();
  else if (line == "TEST_CACHE") Serial.printf("[TEST_CACHE] %s\n", createTestCache(storage) ? "OK (test tones installed)" : "FAILED");
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
  else Serial.println("Commands: PLAY_LEAVE, PLAY_ARRIVE, REPLAY, LEFT_TO_RIGHT, RIGHT_TO_LEFT, STATUS, POLL, CHIME, TEST_CACHE, INIT_FS, HELP (newline)");
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.external_imu = false;
  cfg.external_rtc = false;
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
  if (!receiver.begin()) Serial.println("[UART] initialization FAILED");
  if (!api.start()) Serial.println("[API] task creation failed");
  command("HELP");
}

void loop() {
  M5.update();
  if (!storageOK) { delay(20); return; }
  audio.update();
  DoorProtocol::Action action;
  if (!audio.busy() && receiver.next(action)) {
    using DoorProtocol::Action;
    if (action == Action::Replay) audio.replay();
    else {
      int slot = ((action == Action::LeftToRight) == bool(ATOM_LEFT_TO_RIGHT_IS_LEAVE)) ? 1 : 0;
      audio.play(slot);
      events.enqueue(slot);
    }
  }
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
