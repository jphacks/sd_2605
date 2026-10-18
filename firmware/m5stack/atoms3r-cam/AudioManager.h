#pragma once
#include <atomic>
#include <esp_heap_caps.h>
#include "WavValidation.h"
class AudioManager {
  Storage& storage;
  uint8_t* buffer = nullptr;
  uint8_t* staging = nullptr;
  size_t lastSize = 0;
  uint8_t lastVolume = 0;
  std::atomic<bool> released{true};
  bool running = false;
  static void release(void* a, const void*, uint8_t) {
    static_cast<AudioManager*>(a)->released.store(true);
  }

 public:
  String status = "IDLE";
  bool speakerOK = false;
  AudioManager(Storage& s) : storage(s) {}
  void begin() {
    buffer = static_cast<uint8_t*>(
        heap_caps_malloc(Config::maxWav, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    staging = static_cast<uint8_t*>(
        heap_caps_malloc(Config::maxWav, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    M5.Mic.end();
    auto pins = M5.Speaker.config();
    Serial.printf("[SPEAKER] BCLK=%d WS=%d DATA=%d I2S=%d UART1 RX=2 TX=1\n",
                  pins.pin_bck, pins.pin_ws, pins.pin_data_out, int(pins.i2s_port));
    bool conflict = pins.pin_bck == 1 || pins.pin_bck == 2 || pins.pin_ws == 1 ||
      pins.pin_ws == 2 || pins.pin_data_out == 1 || pins.pin_data_out == 2;
    speakerOK = !conflict && M5.Speaker.begin();
    if (conflict) Serial.println("[SPEAKER] UART pin conflict; playback disabled");
    M5.Speaker.setBufferReleaseCallback(this, release);
    if (!buffer || !staging) status = "PSRAM allocation failed";
    if (!speakerOK) status = "Speaker init failed";
    Serial.printf("[SPEAKER] init=%d enabled=%d\n", speakerOK, M5.Speaker.isEnabled());
  }
  bool busy() { return !released.load() || M5.Speaker.isPlaying(); }
  void update() {
    if (running && !busy()) {
      running = false;
      status = "DONE";
      Serial.println("[AUDIO] DONE");
    }
  }
  bool testChime() {
    if (busy()) {
      Serial.println("[AUDIO] test rejected: BUSY");
      return false;
    }
    if (!speakerOK) {
      status = "Speaker init failed";
      Serial.println("[AUDIO] speaker unavailable");
      return false;
    }
    M5.Speaker.setVolume(128);  // Diagnostic volume, independent of Rails settings.
    if (!M5.Speaker.tone(1200, 300, 0)) {
      status = "CHIME FAILED";
      Serial.println("[AUDIO] CHIME FAILED");
      return false;
    }
    running = true;
    status = "TEST CHIME 300ms";
    Serial.println("[AUDIO] CHIME started 1200Hz 300ms volume=128");
    return true;
  }
  bool replay() {
    if (busy()) return false;
    if (!lastSize || !speakerOK) {
      Serial.println("[AUDIO] no previous playback");
      return false;
    }
    M5.Speaker.setVolume(lastVolume);
    released.store(false);
    if (!M5.Speaker.playWav(buffer, lastSize, 1, 0)) {
      released.store(true);
      return false;
    }
    running = true;
    status = "REPLAY";
    Serial.println("[AUDIO] REPLAY");
    return true;
  }
  bool play(int slot) {
    if (busy()) {
      status = "BUSY";
      return false;
    }
    StorageGuard guard(storage);
    if (!(slot ? storage.settings.leave : storage.settings.arrive)) {
      status = "DISABLED";
      return false;
    }
    if (!buffer || !staging || !speakerOK || !storage.mounted) {
      status = "Audio/FS unavailable";
      return false;
    }
    File f = LittleFS.open(storage.path(slot, storage.records[slot].bank), FILE_READ);
    if (!f || !validWav(f)) {
      status = "Missing/invalid WAV";
      Serial.println("[AUDIO] Missing/invalid WAV");
      return false;
    }
    size_t n = f.size();
    f.seek(0);
    if (f.read(staging, n) != n) {
      status = "WAV read failed";
      return false;
    }
    M5.Speaker.setVolume((storage.settings.volume * 255 + 50) / 100);
    released.store(false);
    if (!M5.Speaker.playWav(staging, n, 1, 0)) {
      released.store(true);
      status = "playWav failed";
      return false;
    }
    uint8_t* previous = buffer;
    buffer = staging;
    staging = previous;
    lastSize = n;
    lastVolume = (storage.settings.volume * 255 + 50) / 100;
    running = true;
    status = slot ? "PLAY LEAVE" : "PLAY ARRIVE";
    Serial.println("[AUDIO] " + status);
    return true;
  }
};
