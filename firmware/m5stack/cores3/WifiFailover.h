#pragma once
#include <stdint.h>

// Keep a working connection. After a disconnect, start with the preferred network.
class WifiFailover {
  bool wasConnected = false, attempted = false;
  uint8_t next = 0;
  uint32_t attemptedAt = 0;

 public:
  static constexpr uint32_t retryMs = 15000;
  int poll(uint32_t now, bool connected, bool primary, bool secondary) {
    if (connected) { wasConnected = true; return -1; }
    if (wasConnected) { wasConnected = false; attempted = false; next = 0; }
    if ((!primary && !secondary) || (attempted && uint32_t(now - attemptedAt) < retryMs))
      return -1;
    int selected = primary && secondary ? next : primary ? 0 : 1;
    next = 1 - selected;
    attemptedAt = now;
    attempted = true;
    return selected;
  }
};
