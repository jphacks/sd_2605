#pragma once
#include <LittleFS.h>
#include "Config.h"
#include "WavValidation.h"
#include <atomic>

// Keep both buffers alive for the asynchronous speaker task.
uint8_t* cameraVoiceData[2] = {};
size_t cameraVoiceSize[2] = {};
std::atomic<bool> cameraVoiceReleased{true};
void cameraVoiceRelease(void*, const void*, uint8_t) { cameraVoiceReleased.store(true); }
int64_t cameraVoiceVersion[2] = {-2, -2};
// Called only from the UI task while playback is idle. A failed reload retains the old buffer.
void cameraVoiceRefresh(uint32_t now) {
  static uint32_t checkedAt = 0;
  static int64_t missingLogged[2] = {-2, -2};
  if (now && uint32_t(now - checkedAt) < 1000) return;
  if (M5.Speaker.isPlaying() || !cameraVoiceReleased.load()) return;
  checkedAt = now;
  StorageGuard guard(cameraStorage);
  if (!cameraStorage.mounted) return;
  for (int i = 0; i < 2; ++i) {
    int64_t version = cameraStorage.records[i].version;
    if (cameraVoiceData[i] && cameraVoiceVersion[i] == version) continue;
    String path = cameraStorage.path(i, cameraStorage.records[i].bank);
    File f = LittleFS.open(path, FILE_READ);
    if (!f || !validWav(f)) {
      if (missingLogged[i] != version)
        Serial.printf("[VOICE] invalid/missing %s\n", path.c_str());
      missingLogged[i] = version;
      continue;
    }
    size_t n = f.size();
    uint8_t* data = static_cast<uint8_t*>(ps_malloc(n));
    f.seek(0);
    if (!data || f.read(data, n) != n) {
      free(data);
      Serial.printf("[VOICE] load failed %s\n", path.c_str());
      continue;
    }
    free(cameraVoiceData[i]);
    cameraVoiceData[i] = data;
    cameraVoiceSize[i] = n;
    cameraVoiceVersion[i] = version;
    Serial.printf("[VOICE] loaded %s bytes=%u version=%lld\n", path.c_str(), unsigned(n),
                  (long long)version);
  }
}
void cameraVoiceBegin() {
  M5.Speaker.setBufferReleaseCallback(nullptr, cameraVoiceRelease);
  cameraVoiceRefresh(0);
}
void cameraPlayVoice(PassageAction action) {
  // Playback uses the saved buffers/settings, independently of Wi-Fi, API and NTP.
  if (!cameraStorageOK) { Serial.println("[VOICE] storage unavailable"); return; }
  if (M5.Speaker.isPlaying() || !cameraVoiceReleased.load()) {
    Serial.println("[VOICE] skipped: speaker busy");
    return;
  }
  int slot = action == PassageAction::ARRIVE ? 0 : 1;
  bool enabled;
  uint8_t volume;
  {
    StorageGuard guard(cameraStorage);
    enabled = slot ? cameraStorage.settings.leave : cameraStorage.settings.arrive;
    volume = cameraStorage.effectiveVolume();
  }
  M5.Speaker.setVolume((volume * 255 + 50) / 100);
  if (!enabled || !cameraSpeakerOK || !cameraVoiceData[slot]) {
    Serial.printf("[VOICE] %s skipped: enabled=%d speaker=%d savedAudio=%d volume=%u\n",
                  actionName(action), enabled, cameraSpeakerOK, cameraVoiceData[slot] != nullptr, volume);
    return;
  }
  cameraVoiceReleased.store(false);
  bool started = enabled && cameraSpeakerOK && cameraVoiceData[slot] && !M5.Speaker.isPlaying() &&
                 M5.Speaker.playWav(cameraVoiceData[slot], cameraVoiceSize[slot], 1, 0);
  if (!started) cameraVoiceReleased.store(true);
  Serial.printf("[VOICE] %s started=%d\n", actionName(action), started);
}
