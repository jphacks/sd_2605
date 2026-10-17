#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
using String = std::string;
namespace Config {
constexpr char deviceId[] = "entrance-test";
}
time_t testClock = 1792215000;
time_t eventTestTime(time_t*) { return testClock; }
struct FakePreferences {
  std::map<String, String> data;
  bool fail = false;
  String getString(const char* key, const char* fallback = "") {
    return data.count(key) ? data[key] : fallback;
  }
  size_t putString(const char* key, const String& value) {
    if (fail) return 0;
    data[key] = value;
    return value.length();
  }
};
struct Storage {
  FakePreferences prefs;
};
struct StorageGuard {
  explicit StorageGuard(Storage&) {}
};
struct {
  void println(const String&) {}
} Serial;
tm* gmtime_r(const time_t* value, tm* out) {
  *out = *std::gmtime(value);
  return out;
}
#define time eventTestTime
#include "../../firmware/m5stack/cores3/EventManager.h"
#undef time
int main() {
  Storage storage;
  EventManager events(storage);
  testClock = 0;
  assert(!events.enqueue(0));
  assert(events.front().empty());
  testClock = 1792215000;
  assert(events.enqueue(0));
  String first = events.front();
  JsonDocument d;
  assert(!deserializeJson(d, first));
  assert(d["event_type"].as<String>() == "arrived_home");
  assert(d["occurred_at"].as<String>() == "2026-10-17T05:30:00Z");
  assert(events.enqueue(1));
  assert(events.front() == first);
  EventManager restarted(storage);
  assert(restarted.front() == first);
  assert(!events.acknowledge("wrong-id"));
  assert(events.front() == first);
  storage.prefs.fail = true;
  assert(!events.acknowledge(first));
  assert(events.front() == first);
  assert(!events.enqueue(1));
  storage.prefs.fail = false;
  assert(events.acknowledge(first));
  String second = events.front();
  assert(second != first);
  assert(!deserializeJson(d, second));
  assert(d["event_type"].as<String>() == "left_home");
  assert(events.acknowledge(second));
  assert(events.front().empty());
  for (int i = 0; i < 24; i++) assert(events.enqueue(i % 2));
  assert(!events.enqueue(0));
  for (int i = 0; i < 24; i++) {
    String current = events.front();
    assert(!current.empty());
    assert(events.acknowledge(current));
  }
  assert(events.front().empty());
  storage.prefs.data["events"] = "corrupt";
  assert(!events.enqueue(0));
  assert(storage.prefs.data["events"] == "corrupt");
  puts(
      "Events: unsynced clock rejection, UTC, restart, stable IDs, ack, persistence failure, "
      "capacity, corrupt queue passed");
}
