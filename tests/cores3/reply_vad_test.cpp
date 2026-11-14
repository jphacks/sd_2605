#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
#include "../../firmware/m5stack/cores3/ReplyVad.h"
#include "../../firmware/m5stack/cores3/ReplyWav.h"

std::vector<int16_t> audio() {
  std::vector<int16_t> pcm(160000);
  for (size_t i = 0; i < 500; ++i)
    std::fill_n(pcm.data() + i * 320, 320, int16_t(i + 1));
  return pcm;
}
void reset() { std::fill_n(mockSpeech, 500, false); mockProcessed = 0; }
void speech(size_t begin, size_t end) { std::fill(mockSpeech + begin, mockSpeech + end, true); }
void finish(ReplyVad& vad) {
  size_t steps = 0;
  while (!vad.step()) { ++steps; assert(steps < 64); }
  assert(steps == 62 && mockProcessed == 500);
}
int main() {
  ReplyVad vad;
  reset(); auto pcm = audio();
  assert(vad.begin(pcm.data(), pcm.size())); finish(vad);
  assert(vad.keptSamples == 0 && vad.segments() == 0);

  reset(); speech(100, 119); pcm = audio();
  assert(vad.begin(pcm.data(), pcm.size())); finish(vad);
  assert(vad.keptSamples == 0); // Short transients do not become recordings.

  reset(); speech(100, 120); speech(300, 320); pcm = audio();
  assert(vad.begin(pcm.data(), pcm.size())); finish(vad);
  assert(vad.keptSamples == 90 * 320 && vad.segments() == 2 && vad.speechFrames() == 40);
  assert(vad.longestSpeechMs() == 400);
  assert(pcm[0] == 91 && pcm[44 * 320] == 135 && pcm[45 * 320] == 291 && pcm[89 * 320] == 335);

  reset(); speech(100, 120); speech(130, 150); pcm = audio();
  assert(vad.begin(pcm.data(), pcm.size())); finish(vad);
  assert(vad.keptSamples == 75 * 320 && vad.segments() == 1); // Context overlaps are merged.

  reset(); speech(0, 20); speech(480, 500); pcm = audio();
  assert(vad.begin(pcm.data(), pcm.size())); finish(vad);
  assert(vad.keptSamples == 65 * 320 && vad.segments() == 2);
  assert(pcm[0] == 1 && pcm[vad.keptSamples - 1] == 500);

  reset(); speech(0, 500); pcm = audio();
  assert(vad.begin(pcm.data(), pcm.size())); finish(vad);
  assert(vad.keptSamples == pcm.size() && vad.segments() == 1);
  mockVadFails = true; assert(!vad.begin(pcm.data(), pcm.size())); mockVadFails = false;
  assert(!vad.begin(pcm.data(), pcm.size() - 1));

  std::vector<uint8_t> wav(320044);
  ReplyWav::header(wav.data(), 160000);
  assert(ReplyWav::valid(wav.data(), wav.size(), 320044));
  ReplyWav::header(wav.data(), 48000);
  assert(ReplyWav::valid(wav.data(), 96044, 320044));
  ReplyWav::header(wav.data(), 70 * 320);
  assert(ReplyWav::valid(wav.data(), 44 + 70 * 640, 320044));
  assert(!ReplyWav::valid(wav.data(), 44 + 70 * 640 - 1, 320044));
  wav[24] = 0; assert(!ReplyWav::valid(wav.data(), 44 + 70 * 640, 320044));
  puts("VAD pipeline (mock ESP-SR decisions): silence, transients, segments, context, boundaries, bounded processing, failures and WAV validation passed");
}
