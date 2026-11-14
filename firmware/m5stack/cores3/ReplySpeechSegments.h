#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Frame decisions come from ESP-SR. This class only selects and compacts samples.
class ReplySpeechSegments {
 public:
  static constexpr size_t sampleRate = 16000, frameMs = 20;
  static constexpr size_t frameSamples = sampleRate * frameMs / 1000;
  static constexpr size_t maxFrames = 10000 / frameMs;
  static constexpr size_t minSpeechFrames = 400 / frameMs;
  static constexpr size_t preFrames = 200 / frameMs, postFrames = 300 / frameMs;

 private:
  bool keep[maxFrames] = {};
  size_t frames = 0, runStart = 0, runLength = 0, speechCount = 0, longestRun = 0;
  void closeRun() {
    if (runLength >= minSpeechFrames) {
      size_t first = runStart > preFrames ? runStart - preFrames : 0;
      size_t last = std::min(maxFrames, runStart + runLength + postFrames);
      for (size_t i = first; i < last; ++i) keep[i] = true;
    }
    runLength = 0;
  }

 public:
  size_t segments = 0;
  void feed(bool speech) {
    if (frames >= maxFrames) return;
    if (speech) {
      if (!runLength) runStart = frames;
      ++runLength; ++speechCount;
      longestRun = std::max(longestRun, runLength);
    } else closeRun();
    ++frames;
  }
  size_t speechFrames() const { return speechCount; }
  size_t longestSpeechFrames() const { return longestRun; }
  size_t compact(int16_t* pcm, size_t sampleCount) {
    closeRun();
    size_t out = 0;
    bool previous = false;
    segments = 0;
    for (size_t i = 0; i < frames && i * frameSamples < sampleCount; ++i) {
      if (keep[i]) {
        if (!previous) ++segments;
        size_t count = std::min(frameSamples, sampleCount - i * frameSamples);
        std::memmove(pcm + out, pcm + i * frameSamples, count * sizeof(int16_t));
        out += count;
      }
      previous = keep[i];
    }
    return out;
  }
};
