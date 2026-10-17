#pragma once
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
struct LocalSettings {
  bool leave = true, arrive = true;
  uint8_t volume = 70;
};
struct AudioRecord {
  int64_t version = -1;
  int bank = 0;
};
class Storage {
 public:
  Preferences prefs;
  SemaphoreHandle_t mutex = nullptr;
  bool mounted = false;
  LocalSettings settings;
  AudioRecord records[2];
  bool leaveMode = false;
  void lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
  void unlock() { xSemaphoreGive(mutex); }
  String key(int slot) { return slot ? "leave" : "arrive"; }
  String path(int slot, int bank) { return "/" + key(slot) + String(bank) + ".wav"; }
  bool begin() {
    mutex = xSemaphoreCreateMutex();
    if (!mutex || !prefs.begin("atom-entrance", false)) return false;
    // Bind cached audio/settings/event queue to one device identity.
    String identity = String(Config::baseUrl) + "|" + Config::deviceId;
    String previous = prefs.getString("identity", "");
    if (previous.length() && previous != identity) {
      Serial.println("[FATAL] Device/API changed; restore config or explicitly erase Atom flash before provisioning.");
      return false;
    }
    if (previous != identity && prefs.putString("identity", identity) != identity.length()) return false;
    mounted = LittleFS.begin(Config::formatIfMountFails);
    JsonDocument d;
    deserializeJson(d, prefs.getString("settings", "{}"));
    settings.leave = d["leave"] | true;
    settings.arrive = d["arrive"] | true;
    settings.volume = d["volume"] | 70;
    if (settings.volume > 100) settings.volume = 70;
    leaveMode = prefs.getBool("mode", false);
    for (int i = 0; i < 2; i++) {
      JsonDocument r;
      deserializeJson(r, prefs.getString(key(i).c_str(), "{}"));
      records[i].version = r["version"] | int64_t(-1);
      records[i].bank = r["bank"] | 0;
      if (records[i].bank != 1) records[i].bank = 0;
    }
    if (mounted) LittleFS.remove("/download.tmp");
    if (mounted)
      Serial.printf("[FS] total=%u used=%u free=%u\n", unsigned(LittleFS.totalBytes()),
                    unsigned(LittleFS.usedBytes()),
                    unsigned(LittleFS.totalBytes() - LittleFS.usedBytes()));
    return true;
  }
  bool saveSettings(LocalSettings s) {
    JsonDocument d;
    d["leave"] = s.leave;
    d["arrive"] = s.arrive;
    d["volume"] = s.volume;
    String j;
    serializeJson(d, j);
    if (prefs.getString("settings") == j) {
      settings = s;
      return true;
    }
    if (prefs.putString("settings", j) != j.length()) return false;
    settings = s;
    return true;
  }
  bool commitAudio(int slot, int bank, int64_t version) {
    JsonDocument d;
    d["bank"] = bank;
    d["version"] = version;
    String j;
    serializeJson(d, j);
    if (prefs.putString(key(slot).c_str(), j) != j.length()) return false;
    records[slot] = {version, bank};
    return true;
  }
  bool setMode(bool value) {
    if (prefs.putBool("mode", value) != 1) return false;
    leaveMode = value;
    return true;
  }
};
class StorageGuard {
  Storage& s;

 public:
  StorageGuard(Storage& v) : s(v) { s.lock(); }
  ~StorageGuard() { s.unlock(); }
};
