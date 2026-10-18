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
  int16_t localVolume = -1;  // -1 follows the Web setting; guarded by mutex.
  uint8_t effectiveVolume() const { return localVolume < 0 ? settings.volume : localVolume; }
  bool setLocalVolume(int16_t value) {
    if (value < -1 || value > 100) return false;
    if (value != localVolume && prefs.putShort("localVolume", value) != 2) return false;
    localVolume = value;
    return true;
  }
  void lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
  void unlock() { xSemaphoreGive(mutex); }
  String key(int slot) { return slot ? "leave" : "arrive"; }
  String path(int slot, int bank) { return "/" + key(slot) + String(bank) + ".wav"; }
  bool begin() {
    mutex = xSemaphoreCreateMutex();
    if (!mutex || !prefs.begin("entrance", false)) return false;
    mounted = LittleFS.begin(Config::formatIfMountFails);
    JsonDocument d;
    deserializeJson(d, prefs.getString("settings", "{}"));
    settings.leave = d["leave"] | true;
    settings.arrive = d["arrive"] | true;
    settings.volume = d["volume"] | 70;
    if (settings.volume > 100) settings.volume = 70;
    localVolume = prefs.getShort("localVolume", -1);
    if (localVolume < -1 || localVolume > 100) localVolume = -1;
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
