#pragma once
#include <atomic>
#include <algorithm>
#include <esp_system.h>
#include <time.h>
#include "VoiceReplyQueue.h"
#include "ReplyVad.h"
#include "ReplyWav.h"

// UI owns capture; the API task owns the immutable READY buffer until acknowledgement.
namespace VoiceReply {
std::atomic<bool> released{false};
ReplyVad replyVad;
void release(void*, void*, size_t) { released.store(true); }
bool audioBusy() { int s = state.load(); return s == PLAYBACK || s == RECORDING || s == ANALYZING; }
bool prepare(int selectedSlot) {
  if (state.load() != IDLE) { Serial.println("[REPLY] pending; next recording skipped"); return false; }
  if (!wav) wav = static_cast<uint8_t*>(ps_malloc(bytes));
  if (!wav) { Serial.println("[REPLY] allocation failed"); return false; }
  pendingBytes = bytes;
  ReplyWav::header(wav, samples);
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
      f.size() - f.position() <= 44 || f.size() - f.position() > bytes) {
    Serial.println("[REPLY] invalid saved reply; retained for inspection"); return;
  }
  wav = static_cast<uint8_t*>(ps_malloc(bytes));
  pendingBytes = f.size() - f.position();
  if (!wav || f.read(wav, pendingBytes) != pendingBytes || !ReplyWav::valid(wav, pendingBytes, bytes)) {
    free(wav); wav = nullptr;
    Serial.println("[REPLY] invalid saved WAV; retained for inspection"); return;
  }
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
    Serial.println("[REPLY] recording 10 seconds: speak now");
  }
  if (state.load() == RECORDING) {
    bool done = released.load();
    if (!done && uint32_t(millis() - startedAt) < durationMs + 5000) return;
    M5.Mic.end(); M5.Speaker.begin();
    if (!done) {
      state.store(IDLE); Serial.println("[REPLY] capture timeout; discarded"); return;
    }
    int peak = 0;
    auto pcm = reinterpret_cast<int16_t*>(wav + 44);
    for (size_t i = 0; i < samples; ++i) peak = std::max(peak, abs(int(pcm[i])));
    Serial.printf("[REPLY] capture peak=%d\n", peak);
    if (!replyVad.begin(pcm, samples)) {
      pendingBytes = 0; state.store(READY);
      Serial.println("[VAD] ESP-SR initialization failed; audio discarded, event only"); return;
    }
    state.store(ANALYZING);
    Serial.println("[VAD] ESP-SR WebRTC mode=2 frame=20ms minSpeech=400ms pre=200ms post=300ms");
  }
  if (state.load() != ANALYZING || !replyVad.step()) return;
  Serial.printf("[VAD] speechFrames=%u longestMs=%u segments=%u keptMs=%u\n", unsigned(replyVad.speechFrames()),
                unsigned(replyVad.longestSpeechMs()), unsigned(replyVad.segments()),
                unsigned(replyVad.keptSamples * 1000 / 16000));
  if (!replyVad.keptSamples) {
    pendingBytes = 0; state.store(READY);
    Serial.println("[VAD] no speech; audio not saved or uploaded, event only"); return;
  }
  pendingBytes = 44 + replyVad.keptSamples * 2;
  ReplyWav::header(wav, replyVad.keptSamples);
  {
    StorageGuard guard(storage);
    JsonDocument d; d["id"] = eventId; d["slot"] = slot; d["time"] = occurredAt;
    File f = storage.mounted ? LittleFS.open("/voice-reply.tmp", FILE_WRITE) : File();
    bool saved = false;
    if (f) {
      size_t header = serializeJson(d, f);
      saved = header > 0 && f.write('\n') == 1 && f.write(wav, pendingBytes) == pendingBytes;
      f.flush(); f.close();
      saved = saved && LittleFS.rename("/voice-reply.tmp", "/voice-reply.bin");
    }
    Serial.printf("[REPLY] persisted=%d (RAM retained until upload)\n", saved);
  }
  state.store(READY);
  Serial.printf("[REPLY] WAV ready bytes=%u\n", unsigned(pendingBytes));
}
}
