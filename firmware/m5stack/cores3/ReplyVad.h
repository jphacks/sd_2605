#pragma once
#include <esp_vad.h>
#include "ReplySpeechSegments.h"

// ESP-SR's WebRTC VAD is bundled with the M5Stack ESP32-S3 core.
// Process at most 160 ms of recorded audio per UI loop, without another task or model partition.
class ReplyVad {
  vad_handle_t handle = nullptr;
  int16_t* pcm = nullptr;
  size_t count = 0, offset = 0;
  ReplySpeechSegments selection;

 public:
  size_t keptSamples = 0;
  bool begin(int16_t* data, size_t samples) {
    if (handle) { vad_destroy(handle); handle = nullptr; }
    selection = ReplySpeechSegments{};
    pcm = data; count = samples; offset = 0; keptSamples = 0;
    if (!data || !samples || samples % ReplySpeechSegments::frameSamples ||
        samples > ReplySpeechSegments::maxFrames * ReplySpeechSegments::frameSamples) return false;
    handle = vad_create(VAD_MODE_2);
    return handle != nullptr;
  }
  bool step() {
    if (!handle) return true;
    for (int batch = 0; batch < 8 && offset < count; ++batch) {
      vad_state_t decision = vad_process(handle, pcm + offset,
          ReplySpeechSegments::sampleRate, ReplySpeechSegments::frameMs);
      selection.feed(decision == VAD_SPEECH);
      offset += ReplySpeechSegments::frameSamples;
    }
    if (offset < count) return false;
    vad_destroy(handle); handle = nullptr;
    keptSamples = selection.compact(pcm, count);
    return true;
  }
  size_t speechFrames() const { return selection.speechFrames(); }
  size_t longestSpeechMs() const { return selection.longestSpeechFrames() * ReplySpeechSegments::frameMs; }
  size_t segments() const { return selection.segments; }
};
