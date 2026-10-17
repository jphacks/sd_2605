#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
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
bool mounted = false, muted = false, pir = false, cooling = false, pending = false;
// Idle, streaming, first chime, last chime, startup beep, draining after error.
enum AudioMode { IDLE, WAV, CHIME1, CHIME2, BOOT, DRAIN };
AudioMode audio = IDLE;
const char* errorText = "";
String lastState;
M5Canvas ui(&M5.Display);
bool uiBuffered = false, showGuide = false;

#include "storage.h"
#include "audio_player.h"
#include "pir_sensor.h"

void displayState() {
  uint32_t now = millis();
  uint32_t elapsed = now - cooldownAt;
  uint32_t seconds = cooling && elapsed < COOLDOWN_MS ? (COOLDOWN_MS - elapsed + 999) / 1000 : 0;
  String state = pending ? "MOTION DETECTED" : audio != IDLE ? "PLAYING" :
                 cooling ? "COOLDOWN" : pir ? "WAIT LOW" : "READY";
  String key = state + String(seconds) + ":" + String(count) + ":" + String(pir) +
               String(muted) + String(showGuide) + String(audio) + errorText;
  // Redraw on visible changes only; avoid display work competing with SD streaming.
  if (key == lastState || uint32_t(now - screenAt) < 50) return;
  screenAt = now;
  lastState = key;
  // Fall back to direct drawing if the sprite allocation fails.
  lgfx::LGFXBase& g = uiBuffered ? static_cast<lgfx::LGFXBase&>(ui) :
                                 static_cast<lgfx::LGFXBase&>(M5.Display);
  const uint16_t bg = 0x0843, panel = 0x10A5;
  uint16_t accent = pending ? TFT_ORANGE : audio != IDLE ? TFT_CYAN :
                    (cooling || pir) ? TFT_YELLOW : TFT_GREEN;
  g.fillScreen(bg);
  g.setFont(&fonts::lgfxJapanGothic_16);
  g.setTextSize(1);
  g.setTextColor(TFT_WHITE, bg);
  g.drawString(showGuide ? "動作テストの手順" : "人感ボイス", 12, 5);
  g.setTextColor(muted ? TFT_ORANGE : TFT_GREEN, bg);
  g.drawRightString(muted ? "消音 ON" : "音 ON", 308, 5);
  if (showGuide) {
    g.setTextColor(TFT_WHITE, bg);
    g.drawString("1  Aを押す → 音が鳴るか確認", 12, 34);
    g.drawString("2  準備OKまでセンサーから離れる", 12, 59);
    g.drawString("3  前を横切る → 検知 + 音", 12, 84);
    g.drawString("4  再生後10秒待ち、もう一度", 12, 109);
    g.drawString("5  Bで消音 → Aで音が出ない", 12, 134);
    g.setTextColor(TFT_CYAN, bg);
    g.drawString("SDなしでもチャイムでテストOK", 12, 163);
    g.setTextColor(accent, bg);
    g.drawString(String("現在: ") + state, 12, 186);
  } else {
    g.fillRoundRect(8, 31, 304, 99, 8, panel);
    g.fillRoundRect(8, 31, 5, 99, 2, accent);
    const char* title = pending ? "動きを検知！" : audio != IDLE ? "再生中" :
                        cooling ? "しばらく待機" : pir ? "反応が続いています" : "準備OK";
    const char* hint = pending ? "まもなく音を再生します" : audio != IDLE ?
                       (muted ? "消音中です / Bで音を戻す" : "再生が終わるまでお待ちください") :
                       cooling ? "待機終了後、次の動きを検知します" : pir ?
                       "センサーから離れてLOWを待つ" : "センサーの前を横切ってください";
    g.setTextColor(accent, panel);
    // The long wait-for-LOW title fits at the normal font size.
    g.setTextSize(!cooling && pir && audio == IDLE && !pending ? 1 : 2);
    g.drawString(title, 20, 38);
    g.setTextSize(1);
    g.drawString(cooling && audio == IDLE ? state + "  " + String(seconds) + "秒" : state, 20, 77);
    g.setTextColor(TFT_WHITE, panel);
    g.drawString(hint, 20, 103);
    if (cooling && audio == IDLE) {
      g.fillRect(16, 126, 288 * (COOLDOWN_MS - (elapsed < COOLDOWN_MS ? elapsed : COOLDOWN_MS)) / COOLDOWN_MS, 3, accent);
    }
    g.setTextColor(TFT_WHITE, bg);
    g.drawString(String("センサー: ") + (pir ? "反応 HIGH" : "静止 LOW"), 12, 139);
    g.drawRightString(String("検知 ") + String(count) + "回", 308, 139);
    g.setTextColor(*errorText ? TFT_ORANGE : TFT_CYAN, bg);
    g.drawString(*errorText ? "音源: チャイム / SD ERROR" : "音源: welcome.wav", 12, 165);
    const char* detail = !*errorText ? "PCM / 16kHz / 16bit / モノラル" :
                         !mounted ? "SD未認識 / SDなしでもテストできます" :
                         !strcmp(errorText, "/welcome.wav missing") ? "SD内に /welcome.wav を用意" :
                         "WAV形式・SDを確認 / 詳細はログ";
    g.setTextColor(TFT_WHITE, bg);
    g.drawString(detail, 12, 185);
  }
  // Labels follow the physical A / B / C button order along the bottom edge.
  for (int i = 0; i < 3; ++i) {
    int x = 4 + i * 106;
    g.fillRoundRect(x, 212, 100, 26, 4, panel);
  }
  g.setTextColor(audio != IDLE || pending ? TFT_DARKGREY : TFT_WHITE, panel);
  g.drawCenterString(audio != IDLE || pending ? "A 再生中" : "A 音テスト", 54, 216);
  g.setTextColor(TFT_WHITE, panel);
  g.drawCenterString(muted ? "B 音を戻す" : "B 消音", 160, 216);
  g.drawCenterString(showGuide ? "C 戻る" : "C 手順", 266, 216);
  if (uiBuffered) ui.pushSprite(0, 0);
}
void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  M5.begin(cfg);
  Serial.begin(115200);
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
  Serial.printf("[%lu ms] boot complete; PIR=%s\n", millis(), pir ? "HIGH" : "LOW");
  if (M5.Speaker.tone(1320, 120, 0)) {
    audio = BOOT;
    Serial.println("startup beep start");
  } else Serial.println("ERROR: startup beep failed");
  displayState();
}
void loop() {
  M5.update();
  uint32_t now = millis();
  if (M5.BtnC.wasPressed()) showGuide = !showGuide;
  if (M5.BtnB.wasPressed()) {
    muted = !muted;
    // Silence queued audio without stopping buffers while the audio task owns them.
    M5.Speaker.setVolume(muted ? 0 : VOLUME);
    Serial.printf("[%lu ms] mute %s\n", now, muted ? "ON" : "OFF");
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
  // Read millis again: finishAudio may have set cooldownAt later than 'now'.
  if (cooling && uint32_t(millis() - cooldownAt) >= COOLDOWN_MS) cooling = false;
  if (pending && uint32_t(now - detectedAt) >= DETECT_DISPLAY_MS) {
    pending = false;
    startAudio();
  }
  if (M5.BtnA.wasPressed() && audio == IDLE && !pending) startAudio();
  displayState();
  yield();
}

