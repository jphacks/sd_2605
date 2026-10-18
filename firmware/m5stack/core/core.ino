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
// Long-press extras: B mutes the sound, C opens the test procedure page.
bool muted = false, showGuide = false;
// Character colour: 0 = pink (A), 1 = yellow (B), 2 = blue (C). Kept in NVS.
uint8_t faceColor = 0;
Preferences prefs;
const char* const COLOR_NAME[3] = {"pink", "yellow", "blue"};
// Short press picks a colour; a long press runs the older action of that button.
constexpr uint32_t HOLD_MS = 600;          // M5Unified default is 500 ms
constexpr uint32_t BUTTON_GUARD_MS = 3000; // ignore the first clicks after boot
uint32_t buttonReadyAt = 0;
// Idle, streaming, first chime, last chime, startup beep, draining after error.
enum AudioMode { IDLE, WAV, CHIME1, CHIME2, BOOT, DRAIN };
AudioMode audio = IDLE;
const char* errorText = "";
String lastState;
M5Canvas ui(&M5.Display);
bool uiBuffered = false;

#include "storage.h"
#include "audio_player.h"
#include "pir_sensor.h"
#include "env3_sensor.h"

// ---------------------------------------------------------------- face screen ----
// The main screen shows environment data on top, a cute face in the middle and the
// greeting under it while the motion sensor is active. The face is drawn with primitives
// only, so no image file or SD card is needed.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t FACE_EYE = rgb565(52, 38, 44);
constexpr uint16_t FACE_MOUTH = TFT_BLACK;  // small solid black mouth
constexpr uint16_t FACE_TONGUE = rgb565(246, 132, 146);
constexpr uint16_t FACE_INK = rgb565(86, 46, 58);        // dark text drawn on the face

// Body colours of the character. The A / B / C buttons pick one of these three.
struct FacePalette { uint16_t fill, edge, blush; };
constexpr FacePalette PALETTES[3] = {
  {rgb565(255, 214, 226), rgb565(212, 128, 156), rgb565(250, 138, 168)},  // 0 pink
  {rgb565(255, 240, 186), rgb565(212, 172, 88),  rgb565(250, 186, 112)},  // 1 yellow
  {rgb565(206, 232, 255), rgb565(116, 156, 204), rgb565(146, 190, 242)},  // 2 blue
};

// Geometry of the face. The head is wider than the panel and its centre sits below the
// bottom edge, so only the dome shows: the panel itself becomes the face, the four
// corners stay dark for the text, and the head bleeds off both sides.
constexpr int FACE_CX = 160, FACE_CY = 200, FACE_R = 155;  // dome apex at y = 45
constexpr int EYE_Y = 106, EYE_DX = 56, EYE_R = 25;
constexpr int BLUSH_Y = 141, BLUSH_DX = 100, BLUSH_RX = 22, BLUSH_RY = 11;
constexpr int MOUTH_Y = 156;
constexpr int GREET_Y = 194;      // greeting line, size 2
constexpr int BAR_Y = 232;        // cooldown bar, still on the face

// Mood of the character. WAKE is the "just spotted you" look, SING plays the sound.
enum FaceMood { MOOD_IDLE, MOOD_WAKE, MOOD_SING, MOOD_SLEEP };

// One eye. style: 0 = open, 1 = wide (surprised), 2 = happy ^^, 3 = sleepy.
void drawFaceEye(lgfx::LGFXBase& g, int x, int y, int style, int r) {
  switch (style) {
    case 1:  // wide, with a small pupil so it still reads as an eye
      g.fillCircle(x, y, r + 4, TFT_WHITE);
      g.fillCircle(x, y, r - 1, FACE_EYE);
      g.fillCircle(x - 3, y - 4, 2, TFT_WHITE);
      break;
    case 2:  // happy, drawn twice to make the caret 2 px thick
      for (int d = 0; d < 2; ++d) {
        g.drawLine(x - r, y + 5 + d, x, y - 4 + d, FACE_EYE);
        g.drawLine(x, y - 4 + d, x + r, y + 5 + d, FACE_EYE);
      }
      break;
    case 3:  // sleepy, closed
      g.fillRect(x - r, y - 2, r * 2, 4, FACE_EYE);
      break;
    default:  // open, with a highlight
      g.fillCircle(x, y, r, FACE_EYE);
      g.fillCircle(x - r / 3, y - r / 3, r / 3, TFT_WHITE);
  }
}

// A small music note, used while the greeting sound plays.
void drawFaceNote(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
  g.fillCircle(x, y, 4, color);
  g.drawLine(x + 4, y, x + 4, y - 16, color);
  g.drawLine(x + 4, y - 16, x + 12, y - 11, color);
}

// The character: one big head that fills the panel, with big eyes, blush and a mouth
// that follows the mood. colorIndex picks the body palette, so one drawing routine
// serves every A / B / C colour. (An index is used instead of the palette type itself
// because the Arduino preprocessor inserts generated prototypes above this file's own
// type definitions.)
void drawFace(lgfx::LGFXBase& g, int mood, uint8_t colorIndex) {
  const FacePalette& p = PALETTES[colorIndex % 3];
  const int eyeStyle = mood == MOOD_SING ? 2 : mood == MOOD_WAKE ? 1 : mood == MOOD_SLEEP ? 3 : 0;
  // The head is clipped by the panel edges, which is what makes the screen the face.
  g.fillCircle(FACE_CX, FACE_CY, FACE_R, p.fill);
  g.drawCircle(FACE_CX, FACE_CY, FACE_R, p.edge);
  drawFaceEye(g, FACE_CX - EYE_DX, EYE_Y, eyeStyle, EYE_R);
  drawFaceEye(g, FACE_CX + EYE_DX, EYE_Y, eyeStyle, EYE_R);
  g.fillEllipse(FACE_CX - BLUSH_DX, BLUSH_Y, BLUSH_RX, BLUSH_RY, p.blush);
  g.fillEllipse(FACE_CX + BLUSH_DX, BLUSH_Y, BLUSH_RX, BLUSH_RY, p.blush);
  // The mouth is small and solid black. Normal and sleepy are a filled half disc (a smile),
  // surprise is a round one, and singing opens it a little with a small tongue.
  if (mood == MOOD_SING) {
    g.fillEllipse(FACE_CX, MOUTH_Y, 18, 22, FACE_MOUTH);
    g.fillEllipse(FACE_CX, MOUTH_Y + 12, 10, 7, FACE_TONGUE);
  } else if (mood == MOOD_WAKE) { // surprised: small round mouth
    g.fillCircle(FACE_CX, MOUTH_Y, 10, FACE_MOUTH);
  } else {                        // smile, a smaller one while sleepy
    const int rx = mood == MOOD_SLEEP ? 12 : 15;
    const int ry = mood == MOOD_SLEEP ? 8 : 11;
    g.fillEllipse(FACE_CX, MOUTH_Y, rx, ry, FACE_MOUTH);
    g.fillEllipse(FACE_CX, MOUTH_Y - ry, rx + 4, ry, p.fill);  // trim the top half
  }
  // The music notes and the sleepy zZ live in the dark corner left free by the dome.
  if (mood == MOOD_SING) {
    drawFaceNote(g, 250, 82, TFT_CYAN);
    drawFaceNote(g, 286, 54, TFT_CYAN);
  } else if (mood == MOOD_SLEEP) {
    g.setTextSize(1);
    g.setTextColor(TFT_LIGHTGREY, 0x0843);
    g.drawString("z", 248, 74);
    g.setTextSize(2);
    g.drawString("Z", 264, 44);
  }
}

void displayState() {
  uint32_t now = millis();
  uint32_t elapsed = now - cooldownAt;
  uint32_t seconds = cooling && elapsed < COOLDOWN_MS ? (COOLDOWN_MS - elapsed + 999) / 1000 : 0;
  String state = pending ? "MOTION DETECTED" : audio != IDLE ? "PLAYING" :
                 cooling ? "COOLDOWN" : pir ? "WAIT LOW" : "READY";
  // "Okaeri!" stays up while the sensor is held, during the short detect delay and for the
  // whole playback, so the greeting never blinks off in the middle of the sound.
  const bool greet = pir || pending || audio != IDLE;
  const char* hint = pending ? "Here it comes..." : audio != IDLE ?
                     (muted ? "Muted / hold B to unmute" : "Please wait for playback to end") :
                     cooling ? "Next motion after cooldown" : pir ?
                     "Move away and wait for LOW" : "Cross in front of the sensor";
  String key = state + String(seconds) + ":" + String(count) + ":" + String(pir) +
               String(muted) + String(showGuide) + String(faceColor) + String(audio) + errorText +
               String(greet) + String(envTemp, 1) + "/" + String(envHum, 1) +
               String(envOK) + String(envSeen);
  // Redraw on visible changes only; avoid display work competing with SD streaming.
  if (key == lastState || uint32_t(now - screenAt) < 50) return;
  screenAt = now;
  lastState = key;
  // Fall back to direct drawing if the sprite allocation fails.
  lgfx::LGFXBase& g = uiBuffered ? static_cast<lgfx::LGFXBase&>(ui) :
                                 static_cast<lgfx::LGFXBase&>(M5.Display);
  const uint16_t bg = 0x0843;
  uint16_t accent = pending ? TFT_ORANGE : audio != IDLE ? TFT_CYAN :
                    (cooling || pir) ? TFT_YELLOW : TFT_GREEN;
  g.fillScreen(bg);
  g.setFont(&fonts::lgfxJapanGothic_16);
  g.setTextSize(1);
  if (showGuide) {
    // Long-press C page: the button list plus the current state. The face is hidden here.
    g.setTextColor(TFT_WHITE, bg);
    g.drawString("TEST PROCEDURE", 12, 5);
    g.setTextColor(muted ? TFT_ORANGE : TFT_GREEN, bg);
    g.drawRightString(muted ? "MUTE ON" : "SOUND ON", 308, 5);
    g.setTextColor(TFT_WHITE, bg);
    g.drawString("Short A / B / C -> body colour", 12, 40);
    g.drawString("Long A  -> test the sound", 12, 68);
    g.drawString("Long B  -> mute on / off", 12, 96);
    g.drawString("Long C  -> close this page", 12, 124);
    g.setTextColor(TFT_CYAN, bg);
    g.drawString("Cross in front of the sensor", 12, 156);
    g.drawString("to greet. 10s cooldown after.", 12, 176);
    g.setTextColor(accent, bg);
    g.drawString(String("Now: ") + state, 12, 204);
    if (*errorText) {
      g.setTextColor(TFT_ORANGE, bg);
      g.drawString(errorText, 12, 224);
    }
  } else {
    // ---- dark strip on top: environment data, detection count, then the hint line ----
    // The face's dome starts at y = 45, so this strip stays clear of the drawing.
    g.setTextColor(envOK ? TFT_CYAN : TFT_ORANGE, bg);
    g.drawString(!envSeen ? "ENV III: no sensor" :
                 String("TEMP ") + String(envTemp, 1) + "C  HUM " + String(envHum, 1) + "%", 12, 4);
    g.setTextColor(TFT_WHITE, bg);
    g.drawRightString(String("Detects ") + String(count), 308, 4);
    // Second line: mute warning, SD error, or the current hint. Only one at a time.
    if (muted) {
      g.setTextColor(TFT_ORANGE, bg);
      g.drawCenterString("MUTED / hold B to unmute", 160, 24);
    } else if (*errorText) {
      g.setTextColor(TFT_ORANGE, bg);
      g.drawCenterString(errorText, 160, 24);
    } else {
      g.drawCenterString(hint, 160, 24);
    }

    // ---- the panel itself is the face, in the colour chosen with A / B / C ----
    const FaceMood mood = (audio != IDLE || pending) ? MOOD_SING :
                          greet ? MOOD_WAKE : (cooling ? MOOD_SLEEP : MOOD_IDLE);
    drawFace(g, mood, faceColor);

    // ---- greeting on the face, status word while nothing is happening ----
    g.setTextSize(2);
    g.setTextColor(FACE_INK, PALETTES[faceColor % 3].fill);
    g.drawCenterString(greet ? "Okaeri!" : state, 160, GREET_Y);
    g.setTextSize(1);
    if (cooling && audio == IDLE) {  // how much of the 10 s cooldown is left
      g.fillRect(20, BAR_Y, 280 * (COOLDOWN_MS - (elapsed < COOLDOWN_MS ? elapsed : COOLDOWN_MS)) / COOLDOWN_MS, 3,
                 PALETTES[faceColor % 3].edge);
    }
  }
  if (uiBuffered) ui.pushSprite(0, 0);
}

// Switch the character body colour and remember it, so a reboot keeps the choice.
// A / B / C map to pink / yellow / blue. Writing happens only when the colour changes.
void setFaceColor(uint8_t index) {
  if (index > 2 || index == faceColor) return;
  faceColor = index;
  Serial.printf("[%lu ms] face colour: %s\n", millis(), COLOR_NAME[faceColor]);
  prefs.begin("greeter", false);
  prefs.putUChar("face", faceColor);
  prefs.end();
}

// Long-press action of each button, kept from the earlier build.
// 0 (A) = manual test play, 1 (B) = mute toggle, 2 (C) = test procedure page.
void longAction(uint8_t index) {
  if (index == 0) {
    // Ignored while a greeting is already playing or about to start.
    if (audio == IDLE && !pending) {
      Serial.printf("[%lu ms] manual test play\n", millis());
      startAudio();
    }
  } else if (index == 1) {
    muted = !muted;
    // Silence queued audio without stopping buffers while the audio task owns them.
    M5.Speaker.setVolume(muted ? 0 : VOLUME);
    Serial.printf("[%lu ms] mute %s\n", millis(), muted ? "ON" : "OFF");
  } else {
    showGuide = !showGuide;
    Serial.printf("[%lu ms] guide %s\n", millis(), showGuide ? "on" : "off");
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Serial.begin(115200);
  // Hold a button this long to reach the older action instead of the colour.
  M5.BtnA.setHoldThresh(HOLD_MS);
  M5.BtnB.setHoldThresh(HOLD_MS);
  M5.BtnC.setHoldThresh(HOLD_MS);
  // Restore the body colour picked with A / B / C on the previous run.
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
  // Short press picks the body colour, long press keeps the older action.
  // A release after a hold is not counted as a click by M5Unified, so the two never clash.
  // The guard window skips the noisy first seconds after power-on (see hardware.md).
  if (int32_t(now - buttonReadyAt) >= 0) {
    if (M5.BtnA.wasHold()) longAction(0);
    else if (M5.BtnA.wasReleased() && !M5.BtnA.wasReleasedAfterHold()) setFaceColor(0);
    if (M5.BtnB.wasHold()) longAction(1);
    else if (M5.BtnB.wasReleased() && !M5.BtnB.wasReleasedAfterHold()) setFaceColor(1);
    if (M5.BtnC.wasHold()) longAction(2);
    else if (M5.BtnC.wasReleased() && !M5.BtnC.wasReleasedAfterHold()) setFaceColor(2);
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

