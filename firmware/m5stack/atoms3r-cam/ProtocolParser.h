#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace DoorProtocol {
constexpr size_t maxLine = 64;
enum class Action : uint8_t { LeftToRight, RightToLeft, Replay };
struct Event { uint32_t boot, seq; Action action; };
inline bool parse(const char* line, Event& out) {
  size_t n = strlen(line);
  if (n > maxLine || n < 19 || strncmp(line, "EVT,1,", 6)) return false;
  uint32_t boot = 0;
  for (size_t i = 6; i < 14; ++i) {
    char c = line[i];
    unsigned v = c >= '0' && c <= '9' ? c - '0' :
                 c >= 'A' && c <= 'F' ? c - 'A' + 10 :
                 c >= 'a' && c <= 'f' ? c - 'a' + 10 : 16;
    if (v > 15) return false;
    boot = (boot << 4) | v;
  }
  if (line[14] != ',') return false;
  size_t i = 15, digits = 0;
  uint32_t seq = 0;
  while (line[i] >= '0' && line[i] <= '9') {
    unsigned v = line[i++] - '0';
    if (++digits > 10 || seq > (UINT32_MAX - v) / 10) return false;
    seq = seq * 10 + v;
  }
  if (!digits || line[i++] != ',') return false;
  Action action;
  if (!strcmp(line + i, "LEFT_TO_RIGHT")) action = Action::LeftToRight;
  else if (!strcmp(line + i, "RIGHT_TO_LEFT")) action = Action::RightToLeft;
  else if (!strcmp(line + i, "REPLAY")) action = Action::Replay;
  else return false;
  out = {boot, seq, action};
  return true;
}
// Eight sender boots, each with a 64-sequence sliding window. Never evict a boot.
// Old sequences outside the window are rejected (no ACK), never replayed.
struct Dedup {
  struct Boot { uint32_t id, high; uint64_t seen; };
  Boot boots[8]{};
  size_t count = 0;
  bool contains(const Event& e) const {
    for (size_t i = 0; i < count; ++i) {
      const auto& b = boots[i];
      if (b.id == e.boot) return e.seq <= b.high && b.high - e.seq < 64 &&
        (b.seen & (uint64_t(1) << (b.high - e.seq)));
    }
    return false;
  }
  bool canAdd(const Event& e) const {
    for (size_t i = 0; i < count; ++i)
      if (boots[i].id == e.boot) return e.seq > boots[i].high || boots[i].high - e.seq < 64;
    return count < 8;
  }
  void add(const Event& e) {
    for (size_t i = 0; i < count; ++i) {
      auto& b = boots[i];
      if (b.id != e.boot) continue;
      if (e.seq > b.high) {
        uint32_t delta = e.seq - b.high;
        b.seen = delta >= 64 ? 1 : (b.seen << delta) | 1;
        b.high = e.seq;
      } else if (b.high - e.seq < 64) b.seen |= uint64_t(1) << (b.high - e.seq);
      return;
    }
    if (count < 8) boots[count++] = {e.boot, e.seq, 1};
  }
};
}
