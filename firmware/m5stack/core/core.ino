#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <Preferences.h>
#include <atomic>

constexpr uint8_t PIR_PIN = 36;  // PORT B, external PIR output (no pull-up)
constexpr uint32_t COOLDOWN_MS = 10000;
constexpr uint32_t DETECT_DISPLAY_MS = 150;
constexpr uint8_t VOLUME = 80;
constexpr char WELCOME_PATH[] = "/welcome.wav";
// Official Speaker_SD_wav_file pattern: two buffers and task-safe release flags.
int16_t buffers[2][512];
std::atomic<bool> busy[2] = {{false}, {false}};
File wav;
uint32_t remaining = 0, cooldownAt = 0, detectedAt = 0, count = 0, screenAt = 0;
size_t bufferIndex = 0;
bool mounted = false, pir = false, cooling = false, pending = false;
// Long B toggles mute; the display always shows only the face.
bool muted = false;
// Character: 0 = pink girl, 1 = yellow friend, 2 = blue boy. C cycles all three.
uint8_t faceColor = 0;
Preferences prefs;
const char* const COLOR_NAME[3] = {"pink", "yellow", "blue"};
// A records three seconds; B plays the latest recording; C cycles characters.
constexpr uint32_t HOLD_MS = 600;          // M5Unified default is 500 ms
constexpr uint32_t BUTTON_GUARD_MS = 3000; // ignore the first clicks after boot
uint32_t buttonReadyAt = 0;
// Idle, streaming, first chime, last chime, startup beep, draining after error.
enum AudioMode { IDLE, WAV, CHIME1, CHIME2, BOOT, DRAIN, RECORDING, REC_PLAY };
AudioMode audio = IDLE;
constexpr size_t RECORD_SAMPLES = 16000 * 3;
int16_t* recording = nullptr;
bool hasRecording = false;
std::atomic<bool> recordingDone{false};
const char* errorText = "";
String lastState;
M5Canvas ui(&M5.Display);
bool uiBuffered = false;

#include "storage.h"
#include "audio_player.h"
#include "audio_recorder.h"
#include "pir_sensor.h"
#include "env3_sensor.h"

// Original faces.ino drawing, kept as a header to avoid Arduino auto-concatenation.
#include "faces.h"

void displayState() {
  const uint32_t now = millis();
  const FaceMood mood = (audio != IDLE || pending) ? MOOD_SING :
                        pir ? MOOD_WAKE : cooling ? MOOD_SLEEP : MOOD_IDLE;
  const String key = String(int(mood)) + ":" + String(faceColor);
  if (key == lastState || uint32_t(now - screenAt) < 50) return;
  screenAt = now;
  lastState = key;
  lgfx::LGFXBase& g = uiBuffered ? static_cast<lgfx::LGFXBase&>(ui) :
                                 static_cast<lgfx::LGFXBase&>(M5.Display);
  const FaceCharacter& character = faceColor == 0 ? GIRL : faceColor == 1 ? FRIEND : BOY;
  drawFace(g, mood, 0, character);
  if (uiBuffered) ui.pushSprite(0, 0);
}

// Switch the character body colour and remember it, so a reboot keeps the choice.
// C cycles all three. Save only when the selection changes.
void setFaceColor(uint8_t index) {
  if (index > 2 || index == faceColor) return;
  faceColor = index;
  Serial.printf("[%lu ms] face colour: %s\n", millis(), COLOR_NAME[faceColor]);
  prefs.begin("greeter", false);
  prefs.putUChar("face", faceColor);
  prefs.end();
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  cfg.internal_mic = true;
  M5.begin(cfg);
  Serial.begin(115200);
  // Hold threshold suppresses the C long-press click.
  M5.BtnA.setHoldThresh(HOLD_MS);
  M5.BtnB.setHoldThresh(HOLD_MS);
  M5.BtnC.setHoldThresh(HOLD_MS);
  // Restore the character chosen with C.
  prefs.begin("greeter", true);
  faceColor = prefs.getUChar("face", 0) % 3;
  prefs.end();
  initPir();
  // HIGH at boot is not an edge: require LOW before first detection.
  pir = readPir();
  SPI.begin(18, 19, 23, 4);
  if (openWelcome()) wav.close();
  M5.Speaker.setVolume(VOLUME);
  M5.Speaker.setBufferReleaseCallback(nullptr, released);
  M5.Display.setRotation(1);
  recording = static_cast<int16_t*>(malloc(RECORD_SAMPLES * sizeof(int16_t)));
  Serial.printf("recording buffer: %s; mic pin=%d\n", recording ? "ready" : "unavailable", M5.Mic.config().pin_data_in);
  ui.setColorDepth(8);
  uiBuffered = ui.createSprite(320, 240) != nullptr;
  buttonReadyAt = millis() + BUTTON_GUARD_MS;
  Serial.printf("[%lu ms] boot complete; PIR=%s; colour=%s\n", millis(), pir ? "HIGH" : "LOW",
                COLOR_NAME[faceColor]);
  if (M5.Speaker.tone(1320, 120, 0)) {
    audio = BOOT;
    Serial.println("startup beep start");
  } else Serial.println("ERROR: startup beep failed");
  displayState();
}
void loop() {
  M5.update();
  uint32_t now = millis();
  updateRecording();
  // A/B act once on press, including a long press.
  // A release after a hold is not counted as a click by M5Unified, so the two never clash.
  // The guard window skips the noisy first seconds after power-on (see hardware.md).
  if (int32_t(now - buttonReadyAt) >= 0) {
    if (M5.BtnA.wasPressed()) startRecording();
    if (M5.BtnB.wasPressed()) playRecording();
    if (M5.BtnC.wasReleased() && !M5.BtnC.wasReleasedAfterHold()) setFaceColor((faceColor + 1) % 3);
  }
  bool high = readPir();
  if (high != pir) {
    pir = high;
    Serial.printf("[%lu ms] PIR -> %s\n", now, high ? "HIGH" : "LOW");
    if (high && !cooling && audio == IDLE && !pending) {
      ++count;
      detectedAt = now;
      pending = true;
      Serial.printf("[%lu ms] MOTION DETECTED #%lu\n", now, count);
    }
  }
  updateAudio();
  pollEnv3(now);
  // Read millis again: finishAudio may have set cooldownAt later than 'now'.
  if (cooling && uint32_t(millis() - cooldownAt) >= COOLDOWN_MS) cooling = false;
  if (pending && uint32_t(now - detectedAt) >= DETECT_DISPLAY_MS) {
    pending = false;
    startAudio();
  }
  displayState();
  yield();
}

