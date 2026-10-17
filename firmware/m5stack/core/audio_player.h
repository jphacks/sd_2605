#pragma once
// Non-blocking playback. Included after storage.h and sketch state declarations.
void released(void*, const void* data, uint8_t) {
  for (size_t i = 0; i < 2; ++i) if (data == buffers[i]) busy[i].store(false);
}
bool buffersFree() { return !busy[0].load() && !busy[1].load(); }
void finishAudio() {
  wav.close();
  audio = IDLE;
  cooling = true;
  cooldownAt = millis();
  Serial.printf("[%lu ms] playback end; cooldown 10s\n", cooldownAt);
}
void startAudio() {
  if (audio != IDLE) return;
  if (muted) {
    Serial.printf("[%lu ms] playback skipped (muted)\n", millis());
    finishAudio();
  } else if (openWelcome()) {
    audio = WAV;
    Serial.printf("[%lu ms] playback start: %s\n", millis(), WELCOME_PATH);
  } else {
    audio = CHIME1;
    Serial.printf("[%lu ms] playback start: fallback chime\n", millis());
    if (!M5.Speaker.tone(1047, 180, 0)) finishAudio();
  }
}
// At most one 1024-byte SD read per loop; never overwrite queued PCM.
void updateAudio() {
  if (audio == IDLE) return;
  if (audio == WAV) {
    if (remaining && !busy[bufferIndex].load()) {
      size_t length = remaining < sizeof(buffers[0]) ? remaining : sizeof(buffers[0]);
      if (wav.read(reinterpret_cast<uint8_t*>(buffers[bufferIndex]), length) != length) {
        sdError("WAV read failed");
        mounted = false;
        wav.close();
        audio = DRAIN;
        return;
      }
      busy[bufferIndex].store(true);
      if (!M5.Speaker.playRaw(buffers[bufferIndex], length / 2, 16000, false, 1, 0)) {
        busy[bufferIndex].store(false);
        Serial.println("ERROR: speaker PCM queue failed");
        wav.close();
        audio = DRAIN;
        return;
      }
      remaining -= length;
      bufferIndex ^= 1;
    }
    if (!remaining && buffersFree() && !M5.Speaker.isPlaying()) finishAudio();
  } else if (!M5.Speaker.isPlaying() && buffersFree()) {
    if (audio == CHIME1 || audio == DRAIN) {
      audio = CHIME2;
      if (!M5.Speaker.tone(1568, 240, 0)) finishAudio();
    } else if (audio == BOOT) {
      audio = IDLE;
      Serial.printf("[%lu ms] startup beep end\n", millis());
    } else finishAudio();
  }
}

