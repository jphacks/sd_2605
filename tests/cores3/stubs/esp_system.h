#pragma once
#include <cstddef>
#include <cstdint>
inline void esp_fill_random(void* out, size_t length) {
  static uint32_t nonce = 1;
  auto p = static_cast<uint8_t*>(out);
  for (size_t i = 0; i < length; i++) p[i] = uint8_t((nonce + i) & 255);
  ++nonce;
}
