// Host test of the actual WAV parser, including malformed chunk boundaries.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
struct File {
  std::vector<uint8_t> bytes;
  size_t pos = 0;
  bool valid = false;
  operator bool() const { return valid; }
  bool isDirectory() { return false; }
  void close() { valid = false; }
  size_t size() { return bytes.size(); }
  size_t position() { return pos; }
  bool seek(uint32_t p) {
    if (p > size()) return false;
    pos = p;
    return true;
  }
  size_t read(uint8_t* p, size_t n) {
    n = std::min(n, size() - pos);
    memcpy(p, bytes.data() + pos, n);
    pos += n;
    return n;
  }
};
namespace Config {
constexpr uint32_t maxWav = 200 * 1024;
}
#include "../../firmware/m5stack/cores3/WavValidation.h"
void put32(std::vector<uint8_t>& b, size_t at, uint32_t x) {
  for (int i = 0; i < 4; ++i) b[at + i] = (x >> (8 * i)) & 255;
}
std::vector<uint8_t> fixture() {
  std::vector<uint8_t> b(48, 0);
  memcpy(b.data(), "RIFF", 4);
  put32(b, 4, 40);
  memcpy(b.data() + 8, "WAVEfmt ", 8);
  put32(b, 16, 16);
  b[20] = 1;
  b[22] = 1;
  put32(b, 24, 16000);
  put32(b, 28, 32000);
  b[32] = 2;
  b[34] = 16;
  memcpy(b.data() + 36, "data", 4);
  put32(b, 40, 4);
  return b;
}
bool parse(std::vector<uint8_t> b) {
  File f;
  f.bytes = b;
  f.valid = true;
  return validWav(f);
}
int main() {
  auto b = fixture();
  assert(parse(b));
  b[22] = 2;
  assert(!parse(b));
  b = fixture();
  b.pop_back();
  assert(!parse(b));
  b = fixture();
  put32(b, 40, 0xfffffffe);
  assert(!parse(b));
  b = fixture();
  put32(b, 40, 3);
  assert(!parse(b));
  b = fixture();
  b[0] = 'X';
  assert(!parse(b));
  b = fixture();
  std::vector<uint8_t> junk = {'J', 'U', 'N', 'K', 1, 0, 0, 0, 7, 0};
  b.insert(b.begin() + 36, junk.begin(), junk.end());
  put32(b, 4, b.size() - 8);
  assert(parse(b));
  b = fixture();
  put32(b, 24, 44100);
  assert(!parse(b));
  b = fixture();
  b.resize(48 + 160002);
  put32(b, 4, b.size() - 8);
  put32(b, 40, 160006);
  assert(!parse(b));
  b = fixture();
  b.resize(44 + 160000);
  put32(b, 4, b.size() - 8);
  put32(b, 40, 160000);
  assert(parse(b));
  b = fixture();
  b.resize(44 + 160002);
  put32(b, 4, b.size() - 8);
  put32(b, 40, 160002);
  assert(!parse(b));
  b = fixture();
  b.push_back(0);
  put32(b, 4, b.size() - 8);
  assert(!parse(b));
  b = fixture();
  b.resize(200 * 1024 + 2);
  put32(b, 4, b.size() - 8);
  assert(!parse(b));
  b = fixture();
  b[20] = 3;
  assert(!parse(b));
  b = fixture();
  b[34] = 8;
  assert(!parse(b));
  b = fixture();
  put32(b, 28, 12345);
  assert(!parse(b));
  puts(
      "WAV: valid PCM, stereo rejection, truncation, oversized/odd chunks, RIFF, padded metadata, "
      "sample rate, PCM, depth, byte rate, duration, trailing corruption, file size passed");
}
