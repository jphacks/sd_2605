// Host test: g++ -std=c++11 tests/cores3/pir_test.cpp -o /tmp/cores3-pir-test && /tmp/cores3-pir-test
#include <cassert>
#include <cstdint>
#include <cstdio>
constexpr int PIR_PIN=8, INPUT=0, HIGH=1;
constexpr uint32_t WARMUP_MS=30000, COOLDOWN_MS=10000;
uint32_t clockNow, pirStartedAt, cooldownAt, detectedAt, count;
bool level, pir, cooling, pending, armed, pirReady;
enum { IDLE, WAV }; int audio=IDLE;
void pinMode(int, int) {}
int digitalRead(int) { return level; }
uint32_t millis() { return clockNow; }
struct { void println(const char*) {} template<typename... T> void printf(const char*, T...) {} } Serial;
#include "../../firmware/m5stack/cores3/pir_sensor.h"
void sample(uint32_t now, bool high) { clockNow=now; level=high; updatePir(now); }
int main() {
  level=true; initPir();
  sample(100, false); sample(200, true); assert(count==0);
  sample(30000, true); assert(count==0 && !armed);
  sample(30001, false); sample(30002, true); assert(count==1 && pending);
  pending=false; audio=WAV;
  sample(30003,false); sample(30004,true); assert(count==1);
  audio=IDLE; cooling=true; cooldownAt=30005;
  sample(31000,false); sample(32000,true); assert(count==1);
  sample(40005,true); assert(!cooling && count==1);
  sample(40006,false); sample(40007,true); assert(count==2);
  pending=false; cooling=true; cooldownAt=UINT32_MAX-5000;
  sample(4998,false); assert(cooling);
  sample(4999,true); assert(!cooling && count==3);
  pending=false; sample(5000,true); assert(count==3);
  puts("PIR: warmup, LOW rearm, overlap, cooldown, held HIGH, millis wrap passed");
}
