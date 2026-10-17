#pragma once
#include <esp_system.h>
#include <time.h>
class EventManager {
  Storage& s;

 public:
  String status = "";
  EventManager(Storage& v) : s(v) {}
  bool enqueue(int slot) {
    StorageGuard guard(s);
    time_t now = time(nullptr);
    if (now < 1767225600) {
      status = "Event: clock unsynced";
      return false;
    }
    JsonDocument d;
    if (deserializeJson(d, s.prefs.getString("events", "[]")) || !d.is<JsonArray>()) {
      status = "Event queue corrupt";
      return false;
    }
    JsonArray a = d.as<JsonArray>();
    if (a.size() >= 24) {
      status = "Event queue FULL";
      return false;
    }
    char utc[24], nonce[33];
    tm t;
    gmtime_r(&now, &t);
    strftime(utc, sizeof(utc), "%Y-%m-%dT%H:%M:%SZ", &t);
    uint32_t r[4];
    esp_fill_random(r, sizeof(r));
    snprintf(nonce, sizeof(nonce), "%08lx%08lx%08lx%08lx", (unsigned long)r[0], (unsigned long)r[1],
             (unsigned long)r[2], (unsigned long)r[3]);
    JsonObject e = a.add<JsonObject>();
    e["event_id"] = String(Config::deviceId) + "-" + nonce;
    e["event_type"] = slot ? "left_home" : "arrived_home";
    e["occurred_at"] = utc;
    String j;
    serializeJson(d, j);
    bool ok = s.prefs.putString("events", j) == j.length();
    status = ok ? "Event queued" : "Event persistence failed";
    Serial.println("[EVENT] " + status);
    return ok;
  }
  String front() {
    StorageGuard guard(s);
    JsonDocument d;
    if (deserializeJson(d, s.prefs.getString("events", "[]")) || !d.is<JsonArray>() || !d.size())
      return "";
    String j;
    serializeJson(d[0], j);
    return j;
  }
  bool acknowledge(const String& sent) {
    StorageGuard guard(s);
    JsonDocument d;
    if (deserializeJson(d, s.prefs.getString("events", "[]")) || !d.size()) return false;
    String j;
    serializeJson(d[0], j);
    if (j != sent) return false;
    d.as<JsonArray>().remove(0);
    j = "";
    serializeJson(d, j);
    return s.prefs.putString("events", j) == j.length();
  }
};
