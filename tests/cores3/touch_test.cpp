#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/TouchInput.h"
int main() {
  TouchInput t;
  assert(t.poll(0, true, 50, 180, true) == 7);
  assert(t.poll(5, true, 50, 180, true) == -1);
  assert(t.poll(10, false, 50, 180, true) == -1);
  assert(t.poll(50, true, 150, 180, true) == -1);
  t.poll(60, false, 0, 0, true);
  assert(t.poll(200, true, 150, 180, true) == 8);
  t.poll(210, false, 0, 0, true);
  assert(t.poll(400, true, 150, 220, true) == 11);
  t.poll(410, false, 0, 0, true);
  assert(t.poll(600, true, 150, 220, false) == 4);
  t.poll(610, false, 0, 0, true);
  assert(t.poll(800, true, -1, 180, true) == -1);
  t.poll(810, false, 0, 0, true);
  assert(t.poll(1000, true, 20, 20, true) == 6);
  puts("Touch: hit mapping, press latch, debounce, sound button and bounds passed");
}
