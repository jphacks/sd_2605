#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ReplyWav {
inline uint16_t get16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t get32(const uint8_t* p) { return uint32_t(get16(p)) | (uint32_t(get16(p + 2)) << 16); }
inline void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
inline void put32(uint8_t* p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
inline void header(uint8_t* wav, size_t samples) {
  std::memcpy(wav, "RIFF", 4); put32(wav + 4, 36 + samples * 2);
  std::memcpy(wav + 8, "WAVEfmt ", 8); put32(wav + 16, 16);
  put16(wav + 20, 1); put16(wav + 22, 1); put32(wav + 24, 16000);
  put32(wav + 28, 32000); put16(wav + 32, 2); put16(wav + 34, 16);
  std::memcpy(wav + 36, "data", 4); put32(wav + 40, samples * 2);
}
// Pending files are produced locally with this canonical 44-byte header.
inline bool valid(const uint8_t* wav, size_t bytes, size_t maximum) {
  return wav && bytes > 44 && bytes <= maximum && (bytes - 44) % 2 == 0 &&
      std::memcmp(wav, "RIFF", 4) == 0 && get32(wav + 4) == bytes - 8 &&
      std::memcmp(wav + 8, "WAVEfmt ", 8) == 0 && get32(wav + 16) == 16 &&
      get16(wav + 20) == 1 && get16(wav + 22) == 1 && get32(wav + 24) == 16000 &&
      get32(wav + 28) == 32000 && get16(wav + 32) == 2 && get16(wav + 34) == 16 &&
      std::memcmp(wav + 36, "data", 4) == 0 && get32(wav + 40) == bytes - 44;
}
}
