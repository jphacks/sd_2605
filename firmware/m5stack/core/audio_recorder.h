#pragma once
// One asynchronous capture buffer, retained until the next recording or reboot.
void recordingReleased(void*, void*, size_t) { recordingDone.store(true); }

void startRecording() {
  if (audio != IDLE || pending) return;
  if (!recording) recording = static_cast<int16_t*>(malloc(RECORD_SAMPLES * sizeof(int16_t)));
  if (!recording) {
    Serial.println("ERROR: recording buffer allocation failed");
    return;
  }
  M5.Speaker.end();
  recordingDone.store(false);
  M5.Mic.setBufferReleaseCallback(nullptr, recordingReleased);
  if (!M5.Mic.begin()) {
    M5.Mic.end();
    M5.Speaker.begin();
    Serial.println("ERROR: microphone start failed");
    return;
  }
  hasRecording = false;
  if (!M5.Mic.record(recording, RECORD_SAMPLES, 16000, false)) {
    M5.Mic.end();
    M5.Speaker.begin();
    Serial.println("ERROR: microphone capture failed");
    return;
  }
  audio = RECORDING;
  Serial.printf("[%lu ms] recording start (3 seconds)\n", millis());
}

void updateRecording() {
  if (audio != RECORDING || !recordingDone.load()) return;
  M5.Mic.end();
  M5.Speaker.begin();
  M5.Speaker.setVolume(VOLUME);
  hasRecording = true;
  audio = IDLE;
  cooling = true;
  cooldownAt = millis();
  Serial.printf("[%lu ms] recording ready; B plays latest recording\n", millis());
}

void playRecording() {
  if (audio != IDLE || pending) return;
  if (!hasRecording) {
    Serial.println("No recording yet; press A first");
    return;
  }
  M5.Speaker.setVolume(VOLUME);
  if (M5.Speaker.playRaw(recording, RECORD_SAMPLES, 16000, false, 1, 0)) {
    audio = REC_PLAY;
    Serial.printf("[%lu ms] latest recording playback start\n", millis());
  } else Serial.println("ERROR: recording playback failed");
}
