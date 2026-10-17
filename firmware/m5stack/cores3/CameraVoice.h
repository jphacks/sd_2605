#pragma once
#include <LittleFS.h>
#include "Config.h"
#include "WavValidation.h"

// Keep both buffers alive for the asynchronous speaker task.
uint8_t* cameraVoiceData[2] = {};
size_t cameraVoiceSize[2] = {};
void cameraVoiceBegin() {
  if (!LittleFS.begin(false)) {
    Serial.println("[VOICE] LittleFS mount failed (no format)");
    return;
  }
  const char* paths[] = {"/arrive0.wav", "/leave0.wav"};
  for (int i = 0; i < 2; ++i) {
    File f = LittleFS.open(paths[i], FILE_READ);
    if (!f || !validWav(f)) {
      Serial.printf("[VOICE] invalid/missing %s\n", paths[i]);
      continue;
    }
    size_t n = f.size();
    uint8_t* data = static_cast<uint8_t*>(ps_malloc(n));
    f.seek(0);
    if (!data || f.read(data, n) != n) {
      free(data);
      Serial.printf("[VOICE] load failed %s\n", paths[i]);
      continue;
    }
    cameraVoiceData[i] = data;
    cameraVoiceSize[i] = n;
    Serial.printf("[VOICE] loaded %s bytes=%u\n", paths[i], unsigned(n));
  }
}
void cameraPlayVoice(PassageAction action) {
  int slot = action == PassageAction::ARRIVE ? 0 : 1;
  bool started = cameraSpeakerOK && cameraVoiceData[slot] && !M5.Speaker.isPlaying() &&
                 M5.Speaker.playWav(cameraVoiceData[slot], cameraVoiceSize[slot], 1, 0);
  Serial.printf("[VOICE] %s started=%d\n", actionName(action), started);
}
