#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/CameraScreenTouch.h"
int main() {
  CameraScreenTouch t;
  assert(t.poll(0, true, 20, 10, false) == -1);
  assert(t.poll(999, true, 20, 10, false) == -1);
  assert(t.poll(1000, true, 20, 10, false) == 13);
  assert(t.poll(2500, true, 20, 10, true) == -1);
  assert(t.poll(2600, false, 0, 0, true) == -1);
  assert(t.poll(3000, true, 150, 220, true) == -1);
  assert(t.poll(3100, false, 0, 0, true) == 11);
  assert(t.poll(3200, false, 0, 0, true) == -1);
  assert(t.poll(4000, true, 150, 220, true) == -1);
  assert(t.poll(5000, true, 150, 220, true) == 13);
  assert(t.poll(5100, false, 0, 0, false) == -1);
  assert(t.poll(6000, true, 150, 220, false) == -1);
  assert(t.poll(6100, false, 0, 0, false) == 14);
  assert(t.poll(6200, false, 0, 0, false) == -1);
  assert(t.poll(6300, true, 50, 50, false) == -1);
  assert(t.poll(6400, false, 0, 0, false) == 14);
  assert(t.poll(7000, true, -1, 220, true) == -1);
  t.poll(7100, false, 0, 0, true);
  assert(t.poll(7200, true, 20, 10, true) == -1);
  assert(t.poll(7300, false, 0, 0, true) == 6);
  puts("Screen touch: hold threshold, one toggle per hold, suppressed controls and release taps passed");
}
