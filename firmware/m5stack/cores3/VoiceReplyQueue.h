#pragma once
#include <atomic>
#include <time.h>
namespace VoiceReply {
enum State { IDLE, PLAYBACK, RECORDING, READY };
std::atomic<int> state{IDLE};
constexpr size_t samples = 16000 * 3, bytes = 44 + samples * 2;
uint8_t* wav = nullptr;
String eventId;
int slot = 0;
time_t occurredAt = 0;
uint32_t detectedAt = 0, startedAt = 0;
}
