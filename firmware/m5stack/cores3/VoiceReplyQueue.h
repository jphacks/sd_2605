#pragma once
#include <atomic>
#include <time.h>
namespace VoiceReply {
enum State { IDLE, PLAYBACK, RECORDING, ANALYZING, READY };
std::atomic<int> state{IDLE};
constexpr uint32_t durationMs = 10000;
constexpr size_t samples = 16000 * durationMs / 1000, bytes = 44 + samples * 2;
size_t pendingBytes = bytes;
uint8_t* wav = nullptr;
String eventId;
int slot = 0;
time_t occurredAt = 0;
uint32_t detectedAt = 0, startedAt = 0;
}
