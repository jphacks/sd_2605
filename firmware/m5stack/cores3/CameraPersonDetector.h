#pragma once
#include <Chirale_TensorFlowLite.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <atomic>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "PersonModel.h"
#include "PersonTestImages.h"
#include "CameraPresenceGate.h"

class CameraPersonDetector {
 public:
  struct Result {
    float score = 0;
    uint32_t frames = 0, count = 0, inferenceMs = 0, errors = 0;
    bool present = false;
    int status = 0; // 0 starting, 1 running, -1 model, -2 camera, -3 memory, -4 self test.
  };
  std::atomic<int> thresholdPercent{70};
  std::atomic<bool> enabled{true};
  std::atomic<bool> cameraBusReady{false};
 private:
  SemaphoreHandle_t mutex = nullptr;
  uint16_t* preview = nullptr;
  Result result;
  CameraPresenceGate gate;
  static void taskEntry(void* arg) { static_cast<CameraPersonDetector*>(arg)->run(); vTaskDelete(nullptr); }
  void fail(int code) {
    xSemaphoreTake(mutex, portMAX_DELAY); result.status = code; xSemaphoreGive(mutex);
    cameraBusReady.store(true);
    Serial.printf("[VISION] fatal=%d\n", code);
  }
  static uint8_t luma(const camera_fb_t* frame, int x, int y) {
    size_t pos = (size_t(y) * frame->width + x) * 2;
    uint16_t rgb = (uint16_t(frame->buf[pos]) << 8) | frame->buf[pos + 1];
    int r = ((rgb >> 11) & 31) * 255 / 31;
    int g = ((rgb >> 5) & 63) * 255 / 63;
    int b = (rgb & 31) * 255 / 31;
    return (77 * r + 150 * g + 29 * b) >> 8;
  }
  void run() {
    constexpr size_t arenaSize = 160 * 1024;
    uint8_t* arena = static_cast<uint8_t*>(heap_caps_malloc(arenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!arena) arena = static_cast<uint8_t*>(heap_caps_malloc(arenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!arena) { fail(-3); return; }
    const tflite::Model* model = tflite::GetModel(personModelData);
    if (model->version() != TFLITE_SCHEMA_VERSION) { heap_caps_free(arena); fail(-1); return; }
    tflite::MicroMutableOpResolver<5> ops;
    if (ops.AddAveragePool2D() != kTfLiteOk || ops.AddConv2D() != kTfLiteOk ||
        ops.AddDepthwiseConv2D() != kTfLiteOk || ops.AddReshape() != kTfLiteOk ||
        ops.AddSoftmax() != kTfLiteOk) { heap_caps_free(arena); fail(-1); return; }
    // Interpreter and arena stay alive for the lifetime of this task.
    tflite::MicroInterpreter interpreter(model, ops, arena, arenaSize);
    if (interpreter.AllocateTensors() != kTfLiteOk) { fail(-1); return; }
    TfLiteTensor* input = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0);
    if (!input || !output || input->type != kTfLiteInt8 || input->dims->size != 4 ||
        input->dims->data[0] != 1 || input->dims->data[1] != 96 || input->dims->data[2] != 96 ||
        input->dims->data[3] != 1 || input->bytes != 96 * 96 ||
        output->type != kTfLiteInt8 || output->bytes != 2) { fail(-1); return; }
    // Official reference images verify the actual on-device model and runtime.
    for (int i = 0; i < 2; ++i) {
      memcpy(input->data.int8, i == 0 ? personTestImage : noPersonTestImage, 96 * 96);
      uint32_t started = millis();
      bool pass = interpreter.Invoke() == kTfLiteOk;
      int person = output->data.int8[1], absent = output->data.int8[0];
      pass = pass && (i == 0 ? person > absent : absent > person);
      Serial.printf("[SELFTEST] %s person=%d absent=%d ms=%lu %s\n", i == 0 ? "person" : "empty",
                    person, absent, (unsigned long)(millis() - started), pass ? "PASS" : "FAIL");
      if (!pass) { fail(-4); return; }
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    camera_config_t c = {};
    c.pin_pwdn = c.pin_reset = c.pin_xclk = -1;
    // Borrow M5Unified's existing I2C master: keep touch and speaker working.
    c.pin_sccb_sda = c.pin_sccb_scl = -1;
    c.sccb_i2c_port = M5.In_I2C.getPort();
    c.pin_d7 = 47; c.pin_d6 = 48; c.pin_d5 = 16; c.pin_d4 = 15;
    c.pin_d3 = 42; c.pin_d2 = 41; c.pin_d1 = 40; c.pin_d0 = 39;
    c.pin_vsync = 46; c.pin_href = 38; c.pin_pclk = 45;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0; c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_RGB565; c.frame_size = FRAMESIZE_QQVGA;
    c.fb_count = 2; c.fb_location = CAMERA_FB_IN_PSRAM; c.grab_mode = CAMERA_GRAB_LATEST;
    esp_err_t error = esp_camera_init(&c);
    cameraBusReady.store(true);
    Serial.printf("[CAMERA] init=0x%x freeHeap=%u freePSRAM=%u arenaUsed=%u\n", error,
                  ESP.getFreeHeap(), ESP.getFreePsram(), unsigned(interpreter.arena_used_bytes()));
    if (error != ESP_OK) { fail(-2); return; }
    int previousThreshold = thresholdPercent.load();
    while (true) {
      camera_fb_t* frame = esp_camera_fb_get();
      if (!frame || frame->format != PIXFORMAT_RGB565 || frame->width != 160 || frame->height != 120 ||
          frame->len < 160 * 120 * 2) {
        if (frame) esp_camera_fb_return(frame);
        gate.reset();
        xSemaphoreTake(mutex, portMAX_DELAY); ++result.errors; result.present = false; xSemaphoreGive(mutex);
        vTaskDelay(pdMS_TO_TICKS(100)); continue;
      }
      // Center square crop, 120x120 -> 96x96, grayscale int8 (pixel - 128).
      for (int y = 0; y < 96; ++y)
        for (int x = 0; x < 96; ++x)
          input->data.int8[y * 96 + x] = int(luma(frame, 20 + x * 120 / 96, y * 120 / 96)) - 128;
      xSemaphoreTake(mutex, portMAX_DELAY);
      memcpy(preview, frame->buf, 160 * 120 * 2);
      xSemaphoreGive(mutex);
      esp_camera_fb_return(frame);
      uint32_t started = millis();
      bool success = interpreter.Invoke() == kTfLiteOk;
      uint32_t elapsed = millis() - started;
      if (!success) {
        gate.reset();
        xSemaphoreTake(mutex, portMAX_DELAY); ++result.errors; result.present = false; xSemaphoreGive(mutex);
        vTaskDelay(pdMS_TO_TICKS(50)); continue;
      }
      float score = (int(output->data.int8[1]) - output->params.zero_point) * output->params.scale;
      int threshold = thresholdPercent.load();
      if (threshold != previousThreshold) { gate.reset(); previousThreshold = threshold; }
      bool detected = gate.update(millis(), score, threshold / 100.0f, enabled.load());
      xSemaphoreTake(mutex, portMAX_DELAY);
      result.score = score; result.inferenceMs = elapsed; result.present = gate.present;
      result.status = 1; ++result.frames;
      if (detected) ++result.count;
      Result current = result;
      xSemaphoreGive(mutex);
      Serial.printf("[VISION] score=%.3f threshold=%d present=%d count=%lu ms=%lu frame=%lu errors=%lu\n",
                    score, thresholdPercent.load(), current.present, (unsigned long)current.count,
                    (unsigned long)elapsed, (unsigned long)current.frames, (unsigned long)current.errors);
      if (detected) Serial.println("[DETECT] CAMERA PERSON");
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
 public:
  bool begin() {
    mutex = xSemaphoreCreateMutex();
    preview = static_cast<uint16_t*>(heap_caps_calloc(160 * 120, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!mutex || !preview) { result.status = -3; cameraBusReady.store(true); return false; }
    if (xTaskCreatePinnedToCore(taskEntry, "person-detect", 16384, this, 1, nullptr, 0) != pdPASS) {
      result.status = -3;
      cameraBusReady.store(true);
      return false;
    }
    return true;
  }
  Result snapshot(bool drawPreview) {
    if (!mutex) return result;
    xSemaphoreTake(mutex, portMAX_DELAY);
    Result copy = result;
    if (drawPreview && preview && result.frames) {
      M5.Display.pushImage(0, 28, 160, 120, preview);
      M5.Display.drawRect(20, 28, 120, 120, TFT_YELLOW);
    }
    xSemaphoreGive(mutex);
    return copy;
  }
};
