#pragma once
#include <HTTPClient.h>
#include <WiFi.h>
#include <atomic>
// HTTPClient decodes both Content-Length and chunked bodies into this bounded sink.
class BoundedBody : public Stream {
  Storage* storage;
  File* file;
  String* text;
  size_t limit;
  uint32_t start = millis();

 public:
  size_t count = 0;
  BoundedBody(size_t max, String& out) : storage(nullptr), file(nullptr), text(&out), limit(max) {}
  BoundedBody(size_t max, Storage& s, File& f) : storage(&s), file(&f), text(nullptr), limit(max) {}
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    if (n > limit - count || uint32_t(millis() - start) > 15000) return 0;
    size_t done = 0;
    if (file) {
      StorageGuard guard(*storage);
      done = file->write(b, n);
    } else {
      done = text->concat(reinterpret_cast<const char*>(b), n) ? n : 0;
    }
    count += done;
    return done;
  }
};
class ApiClient {
  Storage& s;
  EventManager& events;
  std::atomic<bool> requested{true};
  std::atomic<int> lastCode{0};
  String endpoint(const char* suffix) {
    return String("/api/devices/") + Config::deviceId + suffix;
  }
  bool begin(HTTPClient& h, WiFiClient& client, const String& path) {
    h.setConnectTimeout(2000);
    h.setTimeout(3000);
    h.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    return h.begin(client, String(Config::baseUrl) + path);
  }
  void error(HTTPClient& h, int code) {
    String body;
    BoundedBody sink(2048, body);
    h.writeToStream(&sink);
    JsonDocument d;
    deserializeJson(d, body);
    Serial.printf("[API] HTTP %d %s code=%s message=%s\n", code,
                  code == 404   ? "not_found"
                  : code == 422 ? "validation_error"
                                : "request_failed",
                  d["error"]["code"] | "", d["error"]["message"] | "");
  }
  bool metadata(JsonVariantConst m, int slot) {
    return m.is<JsonObjectConst>() && m["version"].is<int64_t>() &&
           m["version"].as<int64_t>() >= 0 && m["byte_size"].is<uint32_t>() &&
           m["byte_size"].as<uint32_t>() >= 44 && m["byte_size"].as<uint32_t>() <= Config::maxWav &&
           m["content_type"].is<const char*>() &&
           String(m["content_type"].as<const char*>()) == "audio/wav" &&
           m["path"].is<const char*>() &&
           String(m["path"].as<const char*>()) == endpoint(slot ? "/audio/leave" : "/audio/arrive");
  }
  void download(JsonVariantConst m, int slot) {
    int bank;
    int64_t version = m["version"].as<int64_t>();
    uint32_t expected = m["byte_size"];
    {
      StorageGuard g(s);
      if (!s.mounted) return;
      File old = LittleFS.open(s.path(slot, s.records[slot].bank), FILE_READ);
      if (s.records[slot].version == version && old && validWav(old)) return;
      bank = 1 - s.records[slot].bank;
      if (LittleFS.totalBytes() - LittleFS.usedBytes() < expected + 8192) {
        Serial.println("[AUDIO] insufficient free FS space");
        return;
      }
    }
    WiFiClient c;
    HTTPClient h;
    if (!begin(h, c, m["path"].as<String>())) return;
    int code = h.GET();
    if (code != 200) {
      error(h, code);
      h.end();
      return;
    }
    if (h.getSize() >= 0 && uint32_t(h.getSize()) != expected) {
      Serial.println("[AUDIO] Content-Length mismatch");
      h.end();
      return;
    }
    // Stream in bounded chunks, with both idle and overall time limits; no filesystem lock during
    // network waits.
    String temp = "/download.tmp";
    File f;
    {
      StorageGuard g(s);
      f = LittleFS.open(temp, FILE_WRITE);
    }
    BoundedBody sink(expected, s, f);
    int written = f ? h.writeToStream(&sink) : -1;
    uint32_t total = sink.count;
    bool good = written == int(expected);
    {
      StorageGuard g(s);
      f.flush();
      f.close();
      f = LittleFS.open(temp, FILE_READ);
      good = good && total == expected && f && f.size() == expected && validWav(f);
      f.close();
      if (good) {
        String target = s.path(slot, bank);
        if (LittleFS.exists(target)) good = LittleFS.remove(target);
        if (good) good = LittleFS.rename(temp, target);
        if (good) good = s.commitAudio(slot, bank, version);
      }
      LittleFS.remove(temp);
    }
    h.end();
    Serial.printf("[AUDIO] slot=%d version=%lld update=%s bytes=%u\n", slot, (long long)version,
                  good ? "OK" : "FAILED (previous retained)", total);
  }
  void heartbeat() {
    WiFiClient c;
    HTTPClient h;
    if (!begin(h, c, endpoint("/heartbeat"))) return;
    h.addHeader("Content-Type", "application/json");
    int code = h.POST("{}");
    lastCode = code;
    if (code != 200) {
      error(h, code);
      h.end();
      return;
    }
    // Limit JSON response without an unbounded getString allocation.
    String body;
    body.reserve(4096);
    BoundedBody sink(4096, body);
    int received = h.writeToStream(&sink);
    h.end();
    JsonDocument d;
    bool valid =
        received >= 0 && body.length() <= 4096 && !deserializeJson(d, body) &&
        d["device_id"].is<const char*>() && d["device_id"].as<String>() == Config::deviceId &&
        d["leave_greeting_enabled"].is<bool>() && d["arrive_greeting_enabled"].is<bool>() &&
        d["volume"].is<int>() && d["volume"].as<int>() >= 0 && d["volume"].as<int>() <= 100 &&
        metadata(d["leave_audio"], 1) && metadata(d["arrive_audio"], 0);
    if (!valid) {
      lastCode = -100;
      Serial.println("[API] invalid heartbeat JSON/fields");
      return;
    }
    {
      StorageGuard g(s);
      if (!s.saveSettings({d["leave_greeting_enabled"].as<bool>(),
                           d["arrive_greeting_enabled"].as<bool>(), d["volume"].as<uint8_t>()})) {
        lastCode = -101;
        return;
      }
    }
    download(d["arrive_audio"], 0);
    download(d["leave_audio"], 1);
  }
  void sendEvent() {
    String j = events.front();
    if (j.isEmpty()) return;
    WiFiClient c;
    HTTPClient h;
    if (!begin(h, c, endpoint("/events"))) return;
    h.addHeader("Content-Type", "application/json");
    int code = h.POST(j);
    if (code == 200 || code == 201) {
      bool ok = events.acknowledge(j);
      Serial.printf("[EVENT] HTTP %d ack=%d\n", code, ok);
    } else
      error(h, code);
    h.end();
  }
  static void task(void* p) { static_cast<ApiClient*>(p)->run(); }
  void run() {
    uint32_t hb = millis() - Config::heartbeatMs, retry = millis() - 10000,
             wifiAt = millis() - 15000;
    bool ntpStarted = false;
    for (;;) {
      uint32_t now = millis();
      if (WiFi.status() != WL_CONNECTED) {
        if (now - wifiAt >= 15000) {
          wifiAt = now;
          WiFi.disconnect();
          WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
      } else {
        if (!ntpStarted) {
          configTime(0, 0, "pool.ntp.org", "time.google.com");
          ntpStarted = true;
        }
        if (requested.exchange(false) || now - hb >= Config::heartbeatMs) {
          hb = now;
          heartbeat();
        }
        if (now - retry >= 10000) {
          retry = now;
          sendEvent();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }

 public:
  ApiClient(Storage& v, EventManager& e) : s(v), events(e) {}
  void request() { requested = true; }
  int code() { return lastCode.load(); }
  bool start() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    return xTaskCreate(task, "rails-api", 12288, this, 1, nullptr) == pdPASS;
  }
};
