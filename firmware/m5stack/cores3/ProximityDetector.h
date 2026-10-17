#pragma once
#include "LTR5XX.h"
#include <algorithm>
class ProximityDetector {
  LTR5XX sensor;
  uint32_t at = 0, lastTrigger = 0, lastSample = 0;
  uint16_t calibration[32] = {}, history[3] = {};
  uint8_t historyCount = 0, historyIndex = 0;
  bool haveSample = false;
  uint16_t nearOffset = Config::nearDelta, releaseOffset = Config::releaseDelta;

  uint8_t samples = 0, nearCount = 0, farCount = 0;
  bool armed = false, triggered = false, calFailed = false;

 public:
  bool ok = false, calibrated = false;
  uint16_t raw = 0, filtered = 0, baseline = 0, high = 0, low = 0, noise = 0, peak = 0;
  uint32_t detections = 0, invalidSamples = 0;
  uint16_t offset() const { return nearOffset; }
  uint8_t calibrationProgress() const { return samples; }
  // Runtime changes force a fresh clear interval; never trigger merely from a new threshold.
  bool setOffset(uint16_t value) {
    if (value < 5 || value > 1000) return false;
    uint16_t release = uint32_t(value) * Config::releaseDelta / Config::nearDelta;
    if (calibrated && uint32_t(baseline) + value >= 2047) return false;
    nearOffset = value;
    releaseOffset = std::max(uint16_t(1), release);
    if (calibrated) {
      high = baseline + nearOffset;
      low = baseline + releaseOffset;
    }
    armed = false;
    nearCount = farCount = 0;
    return true;
  }
  void resetStats() {
    detections = invalidSamples = 0;
    peak = raw;
  }
  const char* state = "START";
  void begin() {
    Ltr5xx_Init_Basic_Para p = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
    p.ps_led_pulse_freq = LTR5XX_LED_PULSE_FREQ_40KHZ;
    p.ps_measurement_rate = LTR5XX_PS_MEASUREMENT_RATE_50MS;
    uint8_t id = 0;
    ok = M5.In_I2C.readRegister(0x23, 0x87, &id, 1, 100000) && id == 5 && sensor.begin(&p) &&
         sensor.setPsMode(LTR5XX_PS_ACTIVE_MODE);
    state = ok ? "CALIBRATING" : "SENSOR ERROR";
  }
  void calibrate() {
    calFailed = false;
    calibrated = false;
    samples = 0;
    historyCount = historyIndex = 0;
    haveSample = false;
    armed = false;
    nearCount = farCount = 0;
    state = "CALIBRATING";
  }
  bool update(uint32_t now, bool busy) {
    // Playback may be shorter than a sensor interval: suppress rearming on every loop.
    if (busy) {
      armed = false;
      nearCount = farCount = 0;
    }
    if (!ok || uint32_t(now - at) < Config::sampleMs) return false;
    at = now;
    // A delayed loop cannot turn widely separated measurements into a continuous approach.
    if (haveSample && uint32_t(now - lastSample) > Config::sampleMs * 3) {
      nearCount = farCount = 0;
      historyCount = historyIndex = 0;
      if (!calibrated && !calFailed) samples = 0;
    }
    // Official getPsValue ignores I2C failure: check status/data explicitly before accepting a
    // sample.
    // ALS_PS_STATUS bit 0 = PS NEW DATA; bit 1 is INTERRUPT, not data-ready.
    uint8_t b[2], s = 0;
    if (!M5.In_I2C.readRegister(0x23, 0x8c, &s, 1, 100000) || !(s & 0x01) ||
        !M5.In_I2C.readRegister(0x23, 0x8d, b, 2, 100000) || (b[1] & 0x80)) {
      nearCount = farCount = 0;
      historyCount = historyIndex = 0;
      if (!calibrated && !calFailed) samples = 0;
      ++invalidSamples;
      state = "NO SAMPLE";
      return false;
    }
    raw = b[0] | ((b[1] & 7) << 8);
    lastSample = now;
    haveSample = true;
    peak = std::max(peak, raw);
    history[historyIndex] = raw;
    historyIndex = (historyIndex + 1) % 3;
    if (historyCount < 3) ++historyCount;
    uint16_t sorted[3] = {history[0], history[1], history[2]};
    std::sort(sorted, sorted + historyCount);
    filtered = sorted[historyCount / 2];
    if (calFailed) {
      state = "CAL ERROR: clear + CAL";
      return false;
    }
    if (!calibrated) {
      calibration[samples++] = raw;
      if (samples >= 32) {
        std::sort(calibration, calibration + 32);
        baseline = (uint32_t(calibration[15]) + calibration[16]) / 2;
        noise = calibration[29] - calibration[2];  // Ignore two outliers at each end.
        if (noise > releaseOffset || uint32_t(baseline) + nearOffset >= 2047) {
          calFailed = true;
          state = "CAL ERROR: clear + CAL";
          return false;
        }
        high = baseline + nearOffset;
        low = baseline + releaseOffset;
        calibrated = true;
        armed = false;
        state = "WAIT CLEAR";
      }
      return false;
    }
    if (historyCount < 3) {
      state = "FILTER WARMUP";
      return false;
    }
    if (busy) {
      armed = false;
      nearCount = farCount = 0;
      state = "PLAYING";
      return false;
    }
    if (!armed) {
      nearCount = 0;
      farCount = raw <= low && filtered <= low ? std::min(5, int(farCount) + 1) : 0;
      if (farCount >= 5 && (!triggered || uint32_t(now - lastTrigger) >= Config::cooldownMs)) {
        armed = true;
        farCount = 0;
      }
      state = armed ? "READY" : "WAIT CLEAR/COOLDOWN";
      return false;
    }
    nearCount = raw >= high && filtered >= high ? nearCount + 1 : 0;
    state = nearCount ? "NEAR" : "READY";
    if (nearCount >= 3) {
      armed = false;
      nearCount = farCount = 0;
      triggered = true;
      lastTrigger = now;
      ++detections;
      state = "DETECTED";
      return true;
    }
    return false;
  }
};
