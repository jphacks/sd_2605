#pragma once
#include <cstdint>
namespace PirConfig {
constexpr bool PIR_ENABLED = true;
constexpr int PIR_PIN = 8;
constexpr uint32_t PIR_WARMUP_MS = 30000;
constexpr uint32_t PIR_BEFORE_WINDOW_MS = 2000;
constexpr uint32_t PIR_AFTER_WINDOW_MS = 1000;
constexpr uint32_t EVENT_COOLDOWN_MS = 5000;
}
// Hardware-independent state machine. One LOW->HIGH episode can confirm only once.
class PirFusion {
 public:
  enum class Outcome { NONE, PENDING, CONFIRMED, REJECTED };
  bool high = false;
  uint32_t changedAt = 0;
  Outcome state = Outcome::NONE;
  void begin(uint32_t now) { *this = PirFusion(); bootAt = now; }
  bool ready(uint32_t now) const { return uint32_t(now - bootAt) >= PirConfig::PIR_WARMUP_MS; }
  void sample(uint32_t now, bool level) {
    if (level != high) { high = level; changedAt = now; }
    if (!ready(now)) return;
    // A HIGH spanning warmup is deliberately ignored until a LOW is observed.
    if (!high) armed = true;
    if (high && armed) { armed = false; episode = true; consumed = false; }
    if (high && episode) { lastHighAt = now; haveHigh = true; }
  }
  Outcome camera(uint32_t now, uint32_t detectedAt) {
    if (!ready(now) || pending || (confirmed && uint32_t(now-lastConfirmedAt) < PirConfig::EVENT_COOLDOWN_MS))
      return Outcome::REJECTED;
    cameraAt = detectedAt; pending = true; state = Outcome::PENDING;
    return poll(now);
  }
  Outcome poll(uint32_t now) {
    if (!pending) return Outcome::NONE;
    // Signed relative time supports millis wrap and samples after camera publication.
    int32_t relative = int32_t(lastHighAt - cameraAt);
    if (haveHigh && !consumed && relative >= -int32_t(PirConfig::PIR_BEFORE_WINDOW_MS) &&
        relative <= int32_t(PirConfig::PIR_AFTER_WINDOW_MS)) {
      pending = false; consumed = true; confirmed = true; lastConfirmedAt = now;
      return state = Outcome::CONFIRMED;
    }
    if (uint32_t(now-cameraAt) > PirConfig::PIR_AFTER_WINDOW_MS) {
      pending = false; return state = Outcome::REJECTED;
    }
    return Outcome::PENDING;
  }
  void cancel() { pending = false; state = Outcome::NONE; }
 private:
  uint32_t bootAt = 0, lastHighAt = 0, cameraAt = 0, lastConfirmedAt = 0;
  bool armed = false, episode = false, consumed = false, haveHigh = false;
  bool pending = false, confirmed = false;
};
