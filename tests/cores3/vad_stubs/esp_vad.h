#pragma once
#include <cassert>
#include <cstdint>
enum vad_mode_t { VAD_MODE_2 = 2 };
enum vad_state_t { VAD_SILENCE, VAD_SPEECH };
struct FakeVad { size_t frame = 0; };
using vad_handle_t = FakeVad*;
inline bool mockVadFails = false;
inline bool mockSpeech[500] = {};
inline size_t mockProcessed = 0, mockDestroyed = 0;
inline vad_handle_t vad_create(vad_mode_t mode) {
  assert(mode == VAD_MODE_2);
  return mockVadFails ? nullptr : new FakeVad;
}
inline vad_state_t vad_process(vad_handle_t handle, int16_t*, int rate, int frameMs) {
  assert(rate == 16000 && frameMs == 20 && handle->frame < 500);
  ++mockProcessed;
  return mockSpeech[handle->frame++] ? VAD_SPEECH : VAD_SILENCE;
}
inline void vad_destroy(vad_handle_t handle) { ++mockDestroyed; delete handle; }
