#pragma once
// M5Stack ENV II / ENV III (SHT30) temperature and humidity on PORT A.
// PORT A is the internal I2C bus (GPIO21/22), so M5Unified's In_I2C is used.
// Included after the sketch's shared state declarations.
constexpr uint8_t ENV3_ADDR = 0x44;
constexpr uint32_t ENV3_FREQ = 100000;
constexpr uint32_t ENV3_POLL_MS = 2000;
constexpr uint32_t ENV3_MEASURE_MS = 20;  // high repeatability, clock stretching disabled
constexpr uint32_t ENV3_LOG_MS = 30000;

float envTemp = NAN, envHum = NAN;
bool envOK = false, envSeen = false;
uint32_t envAt = 0, envLogAt = 0;

uint8_t envCrc(const uint8_t* data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc & 0x80) ? uint8_t(crc << 1) ^ 0x31 : uint8_t(crc << 1);
  }
  return crc;
}

// SHT30 single shot: 0x2C 0x06 = high repeatability, no clock stretching.
bool readEnv3() {
  if (!M5.In_I2C.start(ENV3_ADDR, false, ENV3_FREQ) ||
      !M5.In_I2C.write(uint8_t(0x2C)) || !M5.In_I2C.write(uint8_t(0x06)) ||
      !M5.In_I2C.stop()) return false;
  delay(ENV3_MEASURE_MS);
  uint8_t raw[6];
  if (!M5.In_I2C.start(ENV3_ADDR, true, ENV3_FREQ) ||
      !M5.In_I2C.read(raw, sizeof(raw), true) || !M5.In_I2C.stop()) return false;
  if (envCrc(raw, 2) != raw[2] || envCrc(raw + 3, 2) != raw[5]) return false;
  envTemp = -45.0f + 175.0f * float(uint16_t(raw[0]) << 8 | raw[1]) / 65535.0f;
  envHum = 100.0f * float(uint16_t(raw[3]) << 8 | raw[4]) / 65535.0f;
  return true;
}

// Polls every ENV3_POLL_MS and keeps the last good reading on screen.
void pollEnv3(uint32_t now) {
  if (envAt && uint32_t(now - envAt) < ENV3_POLL_MS) return;
  envAt = now ? now : 1;
  bool ok = readEnv3();
  if (ok) envSeen = true;
  if (ok != envOK) {
    envOK = ok;
    envLogAt = now;
    Serial.printf("[%lu ms] ENV III %s\n", now, ok ? "ready" : "lost (check PORT A)");
  }
  if (envOK && uint32_t(now - envLogAt) >= ENV3_LOG_MS) {
    envLogAt = now;
    Serial.printf("[%lu ms] ENV III: %.1f C, %.1f %%RH\n", now, envTemp, envHum);
  }
}
