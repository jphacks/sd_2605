#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/CameraMotionGate.h"
int main() {
  CameraMotionGate g;
  assert(g.ready()); // Warmup is handled by the camera; no quiet-frame deadlock at startup.
  assert(!g.update(0, 10, 5, true));
  for (uint32_t t = 100; t <= 300; t += 100) assert(!g.update(t, 0, 5, true));
  assert(g.ready());
  assert(!g.update(400, 10, 5, true));
  assert(!g.update(500, 0, 5, true)); // Isolated noise does not trigger.
  assert(!g.update(600, 10, 5, true));
  assert(g.update(700, 10, 5, true));
  for (uint32_t t = 800; t <= 3000; t += 100) assert(!g.update(t, 10, 5, true));
  assert(!g.ready()); // Continuous movement never retriggers.
  for (uint32_t t = 3100; t <= 3300; t += 100) assert(!g.update(t, 4, 5, true)); // Noise below trigger threshold can rearm.
  assert(g.ready());
  assert(!g.update(3400, 10, 5, true));
  assert(g.update(3500, 10, 5, true));
  for (uint32_t t = 3600; t <= 3800; t += 100) assert(!g.update(t, 0, 5, true));
  assert(!g.ready()); // Quiet alone cannot bypass the 2 second cooldown.
  assert(!g.update(5500, 0, 5, true));
  assert(g.ready());
  assert(!g.update(5600, 10, 5, true));
  assert(!g.update(5700, 10, 5, false)); // Invalid frame/lighting/pause clears streak.
  assert(!g.update(5800, 10, 5, true));
  for (uint32_t t = 5900; t <= 6100; t += 100) assert(!g.update(t, 0, 5, true));
  assert(!g.update(6200, 10, 5, true));
  assert(g.update(6300, 10, 5, true));
  puts("Camera motion: quiet rearm, consecutive changes, cooldown, continuous motion and invalid reset passed");
}
