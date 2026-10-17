#pragma once
// LittleFS-backed WAV storage. Included after sketch state declarations.
void storageError(const char* message) {
  errorText = message;
  Serial.printf("[%lu ms] [ERROR] %s\n", millis(), message);
}
uint16_t u16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// Only PCM mono 16-bit 16 kHz. Bounds-check chunks and skip metadata/padding.
bool openWelcome() {
  wav.close();
  if (!mounted) { storageError("mount failed"); return false; }
  errorText = "";
  wav = LittleFS.open(WELCOME_PATH, FILE_READ);
  if (!wav || wav.isDirectory()) {
    missingAudio = true;
    Serial.println("[AUDIO] welcome.wav not found"); wav.close(); return false;
  }
  missingAudio = false;
  uint8_t h[16];
  if (wav.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
    storageError("invalid RIFF/WAVE"); wav.close(); return false;
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
        Serial.printf("[AUDIO] Validated %s: PCM mono 16bit 16000Hz, %lu bytes\n", WELCOME_PATH, remaining);
        return true;
      }
      if (!wav.seek(uint32_t(next))) break;
    }
  }
  storageError("need PCM mono 16-bit 16000 Hz");
  wav.close();
  return false;
}
