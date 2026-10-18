#pragma once
// SD-backed WAV storage. Included after sketch state declarations.
void sdError(const char* message) {
  errorText = message;
  Serial.printf("[%lu ms] SD ERROR: %s\n", millis(), message);
}
uint16_t u16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// Cards differ in how fast they accept the SPI clock during initialisation, and a
// too-fast clock is a common cause of "card mount failed" on otherwise good cards.
// Try the fast speed first and fall back so a slow card still works.
constexpr uint8_t SD_CS_PIN = 4;
const uint32_t SD_SPEEDS[] = {25000000, 10000000, 4000000};

bool mountSd() {
  for (uint32_t hz : SD_SPEEDS) {
    SD.end();
    if (SD.begin(SD_CS_PIN, SPI, hz)) {
      Serial.printf("[%lu ms] SD mounted at %lu Hz; type=%u; %llu MB\n", millis(), hz,
                    SD.cardType(), SD.cardSize() / (1024ULL * 1024ULL));
      return true;
    }
    Serial.printf("[%lu ms] SD mount failed at %lu Hz\n", millis(), hz);
  }
  return false;
}

// Only PCM mono 16-bit 16 kHz. Bounds-check chunks and skip metadata/padding.
bool openWelcome() {
  wav.close();
  if (!mounted) mounted = mountSd();
  if (!mounted) { sdError("insert FAT32 SD card"); return false; }
  wav = SD.open(WELCOME_PATH, FILE_READ);
  if (!wav || wav.isDirectory()) {
    sdError("/welcome.wav missing"); wav.close(); return false;
  }
  uint8_t h[16];
  if (wav.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
    sdError("invalid RIFF/WAVE"); wav.close(); return false;
  }
  uint64_t end = uint64_t(u32(h + 4)) + 8;
  bool formatOK = false;
  if (end <= wav.size()) {
    while (uint64_t(wav.position()) + 8 <= end) {
      if (wav.read(h, 8) != 8) break;
      uint32_t length = u32(h + 4);
      uint64_t next = uint64_t(wav.position()) + length + (length & 1);
      if (next > end) break;
      if (!memcmp(h, "fmt ", 4)) {
        if (length < 16 || wav.read(h, 16) != 16) break;
        formatOK = u16(h) == 1 && u16(h + 2) == 1 && u32(h + 4) == 16000 &&
                   u32(h + 8) == 32000 && u16(h + 12) == 2 && u16(h + 14) == 16;
        if (!formatOK) break;
      } else if (!memcmp(h, "data", 4)) {
        if (!formatOK || !length || (length & 1)) break;
        remaining = length;
        bufferIndex = 0;
        errorText = "";
        return true;
      }
      if (!wav.seek(uint32_t(next))) break;
    }
  }
  sdError("need PCM mono 16-bit 16000 Hz");
  wav.close();
  return false;
}


