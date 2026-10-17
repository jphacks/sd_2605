#include <M5Unified.h>
#include <LittleFS.h>
#include <atomic>
#include <esp_camera.h>
#include <esp_heap_caps.h>

// Set true for standalone internal proximity detection (camera is not initialized).
#ifndef CORES3_USE_PROXIMITY
#define CORES3_USE_PROXIMITY 0
#endif
constexpr bool USE_PROXIMITY = CORES3_USE_PROXIMITY != 0;
constexpr uint32_t COOLDOWN_MS = 10000, WARMUP_MS = 3000;
constexpr uint32_t FRAME_INTERVAL_MS = 150;
constexpr uint8_t PIXEL_DELTA = 20, REQUIRED_FRAMES = 2;
constexpr float MOTION_THRESHOLD = 8.0f; // changed pixels, percent (0..100)
constexpr int LIGHT_SHIFT_LIMIT = 30;
constexpr uint16_t PROX_THRESHOLD = 300, PROX_RELEASE = 200;
constexpr bool SHOW_PREVIEW = true;
constexpr char WELCOME_PATH[] = "/welcome.wav";
int16_t buffers[2][512];
std::atomic<bool> busy[2] = {{false}, {false}};
File wav;
uint32_t remaining = 0, cooldownAt = 0, detectedAt = 0, count = 0;
uint32_t playCount = 0, sensorStartedAt = 0, screenAt = 0;
size_t bufferIndex = 0;
bool mounted = false, missingAudio = false, muted = false;
bool cooling = false, pending = false, speakerOK = false, playbackCounted = false;
uint8_t volume = 80; // 0..255.
enum AudioMode { IDLE, WAV, CHIME1, CHIME2, BOOT, DRAIN };
AudioMode audio = IDLE;
const char* errorText = "";
String lastState;
bool detectionEnabled = true, cameraOK = false, proximityOK = false;
bool sampleOK = false, baselineReady = false, proxArmed = false;
uint16_t proximityRaw = 0;
uint8_t streak = 0;
float motionScore = 0;
uint32_t frameAt = 0;
uint8_t* previous = nullptr;
const char* cameraStatus = "NOT STARTED";

// Shared state must precede these headers (speaker callback runs on another task).
#include "storage.h"
#include "audio_player.h"
#include "motion_detector.h"

void displayState() {
  uint32_t now = millis();
  const char* state = pending ? "MOTION" : audio != IDLE ? "PLAYING" : "READY";
  if (lastState != state) { Serial.printf("[STATUS] %s\n", state); lastState = state; }
  if (uint32_t(now - screenAt) < 200) return;
  screenAt = now;
  auto& g = M5.Display;
  g.fillRect(0, 0, 320, 60, TFT_BLACK);
  g.fillRect(160, 60, 160, 120, TFT_BLACK);
  g.setTextSize(1); g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setCursor(4, 4); g.printf("Camera: %s", cameraStatus);
  g.setCursor(4, 20); g.printf("%s  Detection: %s  %s", state, detectionEnabled ? "ON" : "OFF",
                            USE_PROXIMITY ? "PROX" : "CAMERA");
  g.setCursor(4, 38); g.printf("Score: %.1f%% / %.1f%%  %s", motionScore, MOTION_THRESHOLD,
                            sampleOK ? "" : "NO SAMPLE");
  uint32_t elapsed = now - cooldownAt;
  unsigned seconds = cooling && elapsed < COOLDOWN_MS ? (COOLDOWN_MS - elapsed + 999) / 1000 : 0;
  g.setCursor(164, 64); g.printf("Count: %lu", count);
  g.setCursor(164, 82); g.printf("Cooldown: %u s", seconds);
  g.setCursor(164, 100); g.printf("PS: %u (%s)", proximityRaw, proximityOK ? "OK" : "OFF/ERR");
  g.setCursor(164, 118); g.printf("PS threshold: %u", PROX_THRESHOLD);
  g.setCursor(164, 136); g.print(uint32_t(now - sensorStartedAt) < WARMUP_MS ? "WARMUP" : "");
  g.fillRect(0, 180, 320, 20, TFT_BLACK);
  g.setCursor(4, 180); g.print(*errorText ? errorText : missingAudio ? "No WAV: chime" : "LittleFS /welcome.wav");
  const char* labels[] = {"TEST PLAY", "DETECT ON/OFF"};
  for (int i = 0; i < 2; ++i) {
    g.fillRoundRect(i * 160 + 2, 202, 156, 36, 5, TFT_DARKGREY);
    g.setTextColor(TFT_WHITE, TFT_DARKGREY);
    g.drawCenterString(labels[i], i * 160 + 80, 215);
  }
}
void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.println("[BOOT] CoreS3 initialized");
  Serial.printf("[MEMORY] Flash=%u PSRAM=%u bytes\n", ESP.getFlashChipSize(), ESP.getPsramSize());
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);

  mounted = LittleFS.begin(false); // Never erase existing voice memos on mount failure.
  if (mounted) {
    Serial.printf("[LittleFS] Initialized total=%u used=%u free=%u bytes\n",
                  unsigned(LittleFS.totalBytes()), unsigned(LittleFS.usedBytes()),
                  unsigned(LittleFS.totalBytes() - LittleFS.usedBytes()));
    if (openWelcome()) { wav.close(); remaining = 0; }
  } else storageError("mount failed; provision LittleFS image");
  M5.Mic.end();
  // Finish SCCB/I2C setup before the speaker task can access the amplifier.
  initDetection();
  speakerOK = M5.Speaker.begin();
  M5.Speaker.setVolume(volume);
  M5.Speaker.setBufferReleaseCallback(nullptr, released);
  if (speakerOK) {
    Serial.println("[SPEAKER] Initialized");
    if (M5.Speaker.tone(1000, 200, 0)) {
      audio = BOOT;
      Serial.println("[AUDIO] Boot chime started");
    }
    else storageError("boot chime failed");
  } else storageError("speaker initialization failed");
  displayState();
}
void loop() {
  M5.update();
  updateAudio();
  uint32_t now = millis();
  updateDetection(now);
  if (pending && uint32_t(now - detectedAt) >= 250) {
    pending = false;
    startAudio();
  }
  auto touch = M5.Touch.getDetail();
  if (touch.wasPressed() && touch.y >= 202 && touch.y < 240 && touch.x >= 0 && touch.x < 320) {
    if (touch.x < 160 && !pending) startAudio();
    if (touch.x >= 160) {
      detectionEnabled = !detectionEnabled;
      pending = false; streak = 0; baselineReady = false; proxArmed = false;
      Serial.printf("[TOUCH] detection=%d\n", detectionEnabled);
    }
  }
  displayState();
  yield();
}
