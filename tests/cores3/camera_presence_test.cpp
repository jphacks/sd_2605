#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/CameraPresenceGate.h"
int main() {
  CameraPresenceGate g;
  assert(!g.update(100, .9f, .7f, true));
  assert(!g.update(200, .2f, .7f, true)); // One positive frame is insufficient.
  assert(!g.update(300, .9f, .7f, true));
  assert(g.update(400, .9f, .7f, true));
  assert(g.present);
  for (uint32_t t = 500; t < 20000; t += 100)
    assert(!g.update(t, .9f, .7f, true)); // A person staying never retriggers.
  assert(!g.update(20000, .2f, .7f, true));
  assert(!g.update(20100, .2f, .7f, true));
  assert(g.present);
  assert(!g.update(20200, .2f, .7f, true));
  assert(!g.present);
  assert(!g.update(20300, .9f, .7f, true));
  assert(g.update(20400, .9f, .7f, true));
  for (uint32_t t = 20500; t <= 20700; t += 100)
    assert(!g.update(t, .2f, .7f, true));
  assert(!g.update(20800, .9f, .7f, true));
  assert(!g.update(20900, .9f, .7f, true)); // Cooldown rejects quick re-entry.
  assert(!g.update(21000, .9f, .7f, false));
  assert(!g.present);
  assert(!g.update(32000, .9f, .7f, true));
  assert(!g.update(32100, .9f, .7f, true)); // Resume requires clear view first.
  for (uint32_t t = 32200; t <= 32400; t += 100)
    assert(!g.update(t, .2f, .7f, true));
  assert(!g.update(32500, .9f, .7f, true));
  assert(g.update(32600, .9f, .7f, true));
  g.reset();
  assert(!g.update(50000, .6f, .7f, true)); // Ambiguous scores cannot rearm.
  assert(!g.update(50100, .9f, .7f, true));
  assert(!g.update(50200, .9f, .7f, true));
  puts("Camera presence: consecutive frames, stable presence, absence, cooldown, pause/resume passed");
}
