
#include <M5Unified.h>
#include "esp_camera.h"

bool cameraReady = false;
uint32_t lastCapture = 0;
uint32_t frames = 0;

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.external_display_value = 0;
  cfg.external_speaker.atomic_echo = true;
  M5.begin(cfg);
  Serial.begin(115200);
  delay(1000);
  M5.Speaker.setVolume(100);
  const bool speakerReady = M5.Speaker.begin();
  Serial.printf("Echo Base speaker: %s\n", speakerReady ? "ready" : "FAILED");
  if (speakerReady) {
    Serial.println("Test melody start");
    for (const int frequency : {523, 659, 784}) {
      const bool accepted = M5.Speaker.tone(frequency, 220);
      Serial.printf("Tone %d Hz: %s\n", frequency, accepted ? "OK" : "FAILED");
      delay(300);
    }
    while (M5.Speaker.isPlaying()) { delay(5); }
    M5.Speaker.end();
    Serial.println("Test melody end");
  }
  // Release unused buses before esp_camera installs its SCCB driver.
  M5.In_I2C.release();
  M5.Ex_I2C.release();
  Serial.begin(115200);
  delay(2000);
  Serial.printf("ATOMS3R-CAM test; PSRAM=%u\n", ESP.getPsramSize());
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  delay(500);
  camera_config_t camera = {};
  camera.pin_pwdn = -1;
  camera.pin_reset = -1;
  camera.pin_xclk = 21;
  camera.pin_sccb_sda = 12;
  camera.pin_sccb_scl = 9;
  camera.pin_d7 = 13;
  camera.pin_d6 = 11;
  camera.pin_d5 = 17;
  camera.pin_d4 = 4;
  camera.pin_d3 = 48;
  camera.pin_d2 = 46;
  camera.pin_d1 = 42;
  camera.pin_d0 = 3;
  camera.pin_vsync = 10;
  camera.pin_href = 14;
  camera.pin_pclk = 40;
  camera.xclk_freq_hz = 20000000;
  camera.ledc_timer = LEDC_TIMER_0;
  camera.ledc_channel = LEDC_CHANNEL_0;
  camera.pixel_format = PIXFORMAT_RGB565;
  camera.frame_size = FRAMESIZE_QVGA;
  camera.jpeg_quality = 12;
  camera.fb_count = 2;
  camera.fb_location = CAMERA_FB_IN_PSRAM;
  camera.grab_mode = CAMERA_GRAB_LATEST;
  camera.sccb_i2c_port = 1;  // M5Unified uses port 0 for the internal bus.
  esp_err_t err = esp_camera_init(&camera);
  cameraReady = err == ESP_OK;
  Serial.printf("Camera init: %s (0x%x)\n", esp_err_to_name(err), err);
}

void loop() {
  M5.update();
  if (millis() - lastCapture < 2000) { delay(5); return; }
  lastCapture = millis();
  if (!cameraReady) { Serial.println("Camera unavailable"); return; }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Capture FAILED"); return; }
  Serial.printf("Capture OK #%lu: %ux%u, %u bytes, heap=%u\n",
                (unsigned long)++frames, (unsigned)fb->width,
                (unsigned)fb->height, (unsigned)fb->len, ESP.getFreeHeap());
  esp_camera_fb_return(fb);
}
