#pragma once
#include <atomic>
#include <algorithm>
#include <esp_system.h>
#include <time.h>
#include "VoiceReplyQueue.h"

// UI owns capture; the API task owns the immutable READY buffer until acknowledgement.
namespace VoiceReply {
std::atomic<bool> released{false};
void release(void*, void*, size_t) { released.store(true); }
bool audioBusy() { int s = state.load(); return s == PLAYBACK || s == RECORDING; }
void put16(size_t p, uint16_t v) { wav[p] = v; wav[p + 1] = v >> 8; }
void put32(size_t p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
bool prepare(int selectedSlot) {
  if (state.load() != IDLE) { Serial.println("[REPLY] pending; next recording skipped"); return false; }
  if (!wav) wav = static_cast<uint8_t*>(ps_malloc(bytes));
  if (!wav) { Serial.println("[REPLY] allocation failed"); return false; }
  memcpy(wav, "RIFF", 4); put32(4, bytes - 8); memcpy(wav + 8, "WAVEfmt ", 8);
  put32(16, 16); put16(20, 1); put16(22, 1); put32(24, 16000);
  put32(28, 32000); put16(32, 2); put16(34, 16);
  memcpy(wav + 36, "data", 4); put32(40, samples * 2);
  char nonce[33]; uint32_t r[4]; esp_fill_random(r, sizeof(r));
  snprintf(nonce, sizeof(nonce), "%08lx%08lx%08lx%08lx", (unsigned long)r[0],
           (unsigned long)r[1], (unsigned long)r[2], (unsigned long)r[3]);
  eventId = String(Config::deviceId) + "-" + nonce;
  slot = selectedSlot; occurredAt = time(nullptr); detectedAt = millis();
  state.store(PLAYBACK);
  return true;
}
void restore(Storage& storage) {
  StorageGuard guard(storage);
  if (!storage.mounted || !LittleFS.exists("/voice-reply.bin")) return;
  File f = LittleFS.open("/voice-reply.bin", FILE_READ);
  JsonDocument d;
  String header = f.readStringUntil('\n');
  if (deserializeJson(d, header) || !d["id"].is<String>() ||
      !d["slot"].is<int>() || d["slot"].as<int>() < 0 || d["slot"].as<int>() > 1 ||
      f.size() - f.position() != bytes) {
    Serial.println("[REPLY] invalid saved reply; retained for inspection"); return;
  }
  wav = static_cast<uint8_t*>(ps_malloc(bytes));
  if (!wav || f.read(wav, bytes) != bytes) { free(wav); wav = nullptr; return; }
  eventId = d["id"].as<String>(); slot = d["slot"].as<int>();
  occurredAt = d["time"].as<time_t>(); detectedAt = millis();
  state.store(READY); Serial.println("[REPLY] restored pending WAV");
}
void update(bool playbackFinished, Storage& storage) {
  if (state.load() == PLAYBACK && playbackFinished) {
    M5.Speaker.end();
    released.store(false);
    M5.Mic.setBufferReleaseCallback(nullptr, release);
    if (!M5.Mic.begin() || !M5.Mic.record(reinterpret_cast<int16_t*>(wav + 44), samples, 16000, false)) {
      M5.Mic.end(); M5.Speaker.begin(); state.store(IDLE);
      Serial.println("[REPLY] microphone start failed"); return;
    }
    startedAt = millis(); state.store(RECORDING);
    Serial.println("[REPLY] recording 3 seconds: speak now");
  }
  if (state.load() != RECORDING) return;
  bool done = released.load();
  if (!done && uint32_t(millis() - startedAt) < 6000) return;
  M5.Mic.end(); M5.Speaker.begin();
  if (done) {
    int peak = 0;
    auto pcm = reinterpret_cast<int16_t*>(wav + 44);
    for (size_t i = 0; i < samples; ++i) peak = std::max(peak, abs(int(pcm[i])));
    Serial.printf("[REPLY] capture peak=%d\n", peak);
    StorageGuard guard(storage);
    JsonDocument d; d["id"] = eventId; d["slot"] = slot; d["time"] = occurredAt;
    File f = storage.mounted ? LittleFS.open("/voice-reply.tmp", FILE_WRITE) : File();
    bool saved = false;
    if (f) {
      size_t header = serializeJson(d, f);
      saved = header > 0 && f.write('\n') == 1 && f.write(wav, bytes) == bytes;
      f.flush(); f.close();
      saved = saved && LittleFS.rename("/voice-reply.tmp", "/voice-reply.bin");
    }
    Serial.printf("[REPLY] persisted=%d (RAM retained until upload)\n", saved);
  }
  state.store(done ? READY : IDLE);
  Serial.println(done ? "[REPLY] WAV ready bytes=96044" : "[REPLY] capture timeout; discarded");
}
}
