#pragma once
// Full RIFF walk, including chunks after data. No unbounded allocation.
inline uint16_t le16(const uint8_t* p) { return p[0] | uint16_t(p[1]) << 8; }
inline uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline bool validWav(File& f) {
  uint8_t h[16];
  size_t n = f.size();
  f.seek(0);
  if (n < 44 || n > Config::maxWav || f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) ||
      memcmp(h + 8, "WAVE", 4) || uint64_t(le32(h + 4)) + 8 != n)
    return false;
  bool fmt = false, data = false;
  while (f.position() < n) {
    if (n - f.position() < 8 || f.read(h, 8) != 8) return false;
    uint32_t len = le32(h + 4);
    uint64_t next = uint64_t(f.position()) + len + (len & 1);
    if (next > n) return false;
    if (!memcmp(h, "fmt ", 4)) {
      if (fmt || len < 16 || f.read(h, 16) != 16) return false;
      fmt = le16(h) == 1 && le16(h + 2) == 1 && le32(h + 4) == 16000 && le32(h + 8) == 32000 &&
            le16(h + 12) == 2 && le16(h + 14) == 16;
      if (!fmt) return false;
    } else if (!memcmp(h, "data", 4)) {
      if (!fmt || data || !len || len > 160000 || (len & 1)) return false;
      data = true;
    }
    if (!f.seek(next)) return false;
  }
  return fmt && data;
}
