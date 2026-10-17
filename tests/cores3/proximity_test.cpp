// Test the actual detector with synthetic I2C samples and a fake clock.
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#define __LTR5XX_H__
struct Ltr5xx_Init_Basic_Para {
  int ps_led_pulse_freq, ps_measurement_rate;
};
#define LTR5XX_BASE_PARA_CONFIG_DEFAULT {0, 0}
#define LTR5XX_LED_PULSE_FREQ_40KHZ 0
#define LTR5XX_PS_MEASUREMENT_RATE_50MS 0
#define LTR5XX_PS_ACTIVE_MODE 2
struct LTR5XX {
  bool begin(Ltr5xx_Init_Basic_Para*) { return true; }
  bool setPsMode(int) { return true; }
};
uint16_t input = 100;
bool readOK = true;
uint8_t statusRegister = 0x01;
struct Bus {
  bool readRegister(int, int reg, uint8_t* p, int, int) {
    if (!readOK) return false;
    if (reg == 0x87)
      *p = 5;
    else if (reg == 0x8c)
      *p = statusRegister;
    else {
      p[0] = input & 255;
      p[1] = input >> 8;
    }
    return true;
  }
};
struct {
  Bus In_I2C;
} M5;
struct {
  template <class... T>
  void printf(const char*, T...) {}
} Serial;
namespace Config {
constexpr uint32_t sampleMs = 75, cooldownMs = 10000;
constexpr uint16_t nearDelta = 150, releaseDelta = 70;
}  // namespace Config
#include "../../firmware/m5stack/cores3/ProximityDetector.h"
int main() {
  ProximityDetector d;
  d.begin();
  assert(d.ok);
  uint32_t now = 0;
  auto sample = [&](uint16_t value, bool busy = false) {
    input = value;
    now += 75;
    return d.update(now, busy);
  };
  auto clear = [&](int count = 6) {
    for (int i = 0; i < count; i++) assert(!sample(100));
  };
  auto detect = [&]() {
    assert(!sample(300));
    assert(!sample(300));
    assert(!sample(300));
    assert(sample(300));
  };
  clear(32);
  assert(d.calibrated && d.high == 250 && d.low == 170);
  clear();
  assert(!sample(900));
  clear();  // Isolated spike is rejected.
  detect();
  assert(d.detections == 1);
  for (int i = 0; i < 200; i++) assert(!sample(300));  // No release, even after cooldown.
  clear();
  assert(!sample(300, true));
  for (int i = 0; i < 20; i++) assert(!sample(300));  // Playback forces release.
  clear();
  detect();
  assert(d.detections == 2);
  clear();
  for (int i = 0; i < 8; i++) assert(!sample(300));  // Still cooling.
  clear(140);
  assert(!sample(300));
  assert(!sample(300));
  now += 1000;  // A long gap must discard history and consecutive sample evidence.
  assert(!sample(300));
  assert(!sample(300));
  assert(!sample(300));
  assert(!sample(300));
  assert(sample(300));
  clear(140);
  assert(!sample(300));
  readOK = false;
  assert(!sample(300));
  readOK = true;
  assert(!sample(300));
  assert(!sample(300));
  assert(!sample(300));
  assert(!sample(300));
  assert(sample(300));
  assert(d.invalidSamples == 1);
  clear(140);
  assert(d.setOffset(100));
  assert(d.high == 200 && d.low == 146);
  clear();
  assert(!d.setOffset(0));
  assert(!d.setOffset(2000));
  d.calibrate();
  for (int i = 0; i < 32; i++) assert(!sample(i % 2 ? 600 : 100));
  assert(!d.calibrated);
  clear(300);
  assert(!d.calibrated);  // Error requires an explicit retry.
  d.calibrate();
  for (int i = 0; i < 32; i++) assert(!sample(i == 10 ? 1000 : 100));
  assert(d.calibrated && d.baseline == 100 && d.noise == 0);
  assert(d.setOffset(5));
  assert(d.offset() == 5);
  assert(!d.setOffset(4));
  d.setOffset(100);
  d.calibrate();
  statusRegister = 0x02;
  clear(40);
  assert(!d.calibrated);  // Interrupt alone is not fresh data.
  statusRegister = 0x01;
  clear(32);
  assert(d.calibrated);  // Fresh data without any interrupt must calibrate.
  d.resetStats();
  assert(d.detections == 0 && d.invalidSamples == 0);
  d.calibrate();
  clear(16);
  readOK = false;
  assert(!sample(100));
  readOK = true;
  clear(16);
  assert(!d.calibrated);
  clear(16);
  assert(d.calibrated);
  puts(
      "Proximity: median/spike rejection, robust calibration, hysteresis, release, cooldown, busy, "
      "stale/I2C reset, sensitivity and stats passed");
}
