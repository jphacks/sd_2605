#include <cassert>
#include <cstdio>
#include "../../firmware/m5stack/cores3/WifiFailover.h"
int main() {
  WifiFailover both;
  assert(both.poll(100, false, true, true) == 0);
  assert(both.poll(15099, false, true, true) == -1);
  assert(both.poll(15100, false, true, true) == 1);
  assert(both.poll(30100, false, true, true) == 0);
  assert(both.poll(30101, true, true, true) == -1);
  assert(both.poll(90000, true, true, true) == -1);
  assert(both.poll(90001, false, true, true) == 0);
  WifiFailover one;
  assert(one.poll(0, false, true, false) == 0);
  assert(one.poll(15000, false, true, false) == 0);
  WifiFailover secondary;
  assert(secondary.poll(0, false, false, true) == 1);
  assert(secondary.poll(15000, false, false, true) == 1);
  WifiFailover empty;
  assert(empty.poll(0, false, false, false) == -1);
  assert(empty.poll(15000, false, false, false) == -1);
  WifiFailover wrap;
  assert(wrap.poll(UINT32_MAX - 1000, false, true, true) == 0);
  assert(wrap.poll(13998, false, true, true) == -1);
  assert(wrap.poll(13999, false, true, true) == 1);
  puts("WiFi failover: priority, retry timing, alternation, reconnect, single/empty config, rollover passed");
}
