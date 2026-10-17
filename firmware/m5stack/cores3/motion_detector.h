#pragma once
#include "motion_score.h"
// Internal sensors only. No sensor register access from the speaker task.
constexpr size_t SAMPLE_W = 40, SAMPLE_H = 30, SAMPLE_COUNT = SAMPLE_W * SAMPLE_H;

void initDetection() {
  sensorStartedAt = millis();
  if (USE_PROXIMITY) {
    cameraStatus = "OFF (PROX mode)";
    uint8_t id = 0;
    // LTR-553 manufacturer ID and PS setup, using the existing M5 internal bus.
    proximityOK = M5.In_I2C.readRegister(0x23, 0x87, &id, 1, 100000) && id == 0x05 &&
      M5.In_I2C.writeRegister8(0x23, 0x82, 0x7F, 100000) && // 60kHz, 100mA, 100% duty
      M5.In_I2C.writeRegister8(0x23, 0x83, 0x04, 100000) && // four pulses
      M5.In_I2C.writeRegister8(0x23, 0x84, 0x00, 100000) && // 50ms
      M5.In_I2C.writeRegister8(0x23, 0x81, 0x02, 100000);   // PS active
    Serial.printf("[PROX] init=%d (reflection intensity, not distance)\n", proximityOK);
    return;
  }
  if (!psramFound()) { cameraStatus = "PSRAM MISSING"; return; }
  previous = static_cast<uint8_t*>(heap_caps_malloc(SAMPLE_COUNT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!previous) { cameraStatus = "NO MEMORY"; return; }
  camera_config_t c = {};
  c.pin_pwdn = -1; c.pin_reset = -1; c.pin_xclk = -1;
  c.pin_sccb_sda = -1; c.pin_sccb_scl = -1;
  c.pin_d7 = 47; c.pin_d6 = 48; c.pin_d5 = 16; c.pin_d4 = 15;
  c.pin_d3 = 42; c.pin_d2 = 41; c.pin_d1 = 40; c.pin_d0 = 39;
  c.pin_vsync = 46; c.pin_href = 38; c.pin_pclk = 45;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0; c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_RGB565; c.frame_size = FRAMESIZE_QQVGA;
  c.fb_count = 2; c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST; c.sccb_i2c_port = M5.In_I2C.getPort();
  // GPIO12/11 and the I2C master are already owned by M5Unified.
  // esp_camera supports borrowing that port when SCCB SDA is -1.
  // Keep touch, power and speaker on the same bus; never release/reinstall it.
  esp_err_t result = esp_camera_init(&c);
  cameraOK = result == ESP_OK;
  cameraStatus = cameraOK ? "OK 160x120 RGB565" : "INIT FAILED";
  if (!cameraOK) { heap_caps_free(previous); previous = nullptr; }
  Serial.printf("[CAMERA] init=0x%x PSRAM free=%u\n", result, ESP.getFreePsram());
  sensorStartedAt = millis();
}

void motionEvent(uint32_t now) {
  ++count; pending = true; detectedAt = now; streak = 0;
  cooling = true; cooldownAt = now;
  Serial.printf("[MOTION] t=%lu count=%lu mode=%s score=%.1f PS=%u\n",
                now, count, USE_PROXIMITY ? "PROX" : "CAMERA", motionScore, proximityRaw);
}

// Camera RGB565 bytes follow the official pushImage convention (big endian).
uint8_t sampleLuma(const camera_fb_t* fb, size_t x, size_t y) {
  size_t offset = ((y * fb->height / SAMPLE_H) * fb->width + x * fb->width / SAMPLE_W) * 2;
  uint16_t p = (uint16_t(fb->buf[offset]) << 8) | fb->buf[offset + 1];
  int r = ((p >> 11) & 31) * 255 / 31;
  int g = ((p >> 5) & 63) * 255 / 63;
  int b = (p & 31) * 255 / 31;
  return (77 * r + 150 * g + 29 * b) >> 8;
}

void updateDetection(uint32_t now) {
  if (cooling && uint32_t(now - cooldownAt) >= COOLDOWN_MS) cooling = false;
  if (uint32_t(now - frameAt) < FRAME_INTERVAL_MS) return;
  frameAt = now;
  bool allowed = detectionEnabled && audio == IDLE && !pending && !cooling &&
                 uint32_t(now - sensorStartedAt) >= WARMUP_MS;
  if (USE_PROXIMITY) {
    uint8_t data[2] = {}, status = 0;
    sampleOK = proximityOK && M5.In_I2C.readRegister(0x23, 0x8C, &status, 1, 100000) &&
               (status & 1) && M5.In_I2C.readRegister(0x23, 0x8D, data, 2, 100000) && !(data[1] & 0x80);
    if (sampleOK) {
      proximityRaw = data[0] | ((data[1] & 7) << 8);
      if (proximityRaw < PROX_RELEASE) proxArmed = true;
      if (allowed && proxArmed && proximityRaw >= PROX_THRESHOLD) {
        if (++streak >= REQUIRED_FRAMES) { motionEvent(now); proxArmed = false; }
      } else streak = 0;
    } else streak = 0;
    if (!allowed) { streak = 0; proxArmed = false; }
    Serial.printf("[SCORE] PS=%u valid=%d enabled=%d\n", proximityRaw, sampleOK, allowed);
    return;
  }
  if (!cameraOK) return;
  camera_fb_t* fb = esp_camera_fb_get();
  sampleOK = fb && fb->format == PIXFORMAT_RGB565 && fb->width == 160 && fb->height == 120 &&
             fb->len >= size_t(fb->width) * fb->height * 2;
  if (!sampleOK) {
    if (fb) esp_camera_fb_return(fb);
    baselineReady = false; streak = 0; motionScore = 0;
    Serial.println("[CAMERA] frame missing/invalid"); return;
  }
  uint8_t current[SAMPLE_COUNT];
  for (size_t y = 0; y < SAMPLE_H; ++y)
    for (size_t x = 0; x < SAMPLE_W; ++x)
      current[y * SAMPLE_W + x] = sampleLuma(fb, x, y);
  FrameScore result = baselineReady ? compareFrames(current, previous, SAMPLE_COUNT, PIXEL_DELTA) : FrameScore{0, 0};
  int shift = result.lightShift;
  motionScore = result.percent;
  memcpy(previous, current, SAMPLE_COUNT);
  // Reject sudden whole-image illumination shifts and require consecutive frames.
  bool candidate = baselineReady && abs(shift) <= LIGHT_SHIFT_LIMIT && motionScore >= MOTION_THRESHOLD;
  if (allowed && candidate) {
    if (++streak >= REQUIRED_FRAMES) motionEvent(now);
  } else streak = 0;
  baselineReady = true; // Keep adapting while disabled, playing, or cooling down.
  if (SHOW_PREVIEW) M5.Display.pushImage(0, 60, fb->width, fb->height, reinterpret_cast<uint16_t*>(fb->buf));
  esp_camera_fb_return(fb); // Every successfully acquired frame is returned, including error paths.
  Serial.printf("[SCORE] camera=%.1f threshold=%.1f shift=%d streak=%u allowed=%d\n",
                motionScore, MOTION_THRESHOLD, shift, streak, allowed);
}
