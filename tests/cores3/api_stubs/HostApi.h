#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
class String : public std::string {
 public:
  using std::string::string;
  String() = default;
  String(const std::string& v) : std::string(v) {}
  String(int v) : std::string(std::to_string(v)) {}
  bool startsWith(const char* p) const { return rfind(p, 0) == 0; }
  bool isEmpty() const { return empty(); }
  bool concat(const char* p, size_t n) { append(p, n); return true; }
};
#include <ArduinoJson.h>
namespace ArduinoJson {
template<> struct Converter<String> {
 static String fromJson(JsonVariantConst v) { return String(v.as<const char*>()); }
 static bool checkJson(JsonVariantConst v) { return v.is<const char*>(); }
};
}
inline uint32_t millis() { return 50000; }
struct { template<typename... T> void printf(const char*, T...) {} void println(const String&) {} } Serial;
class Stream {
 public:
 virtual ~Stream() = default;
 virtual int available() = 0;
 virtual int read() = 0;
 virtual int peek() = 0;
 virtual void flush() = 0;
 virtual size_t write(uint8_t) = 0;
 virtual size_t write(const uint8_t*, size_t) = 0;
};
struct File {
 std::shared_ptr<std::vector<uint8_t>> bytes;
 size_t pos = 0;
 explicit operator bool() const { return bool(bytes); }
 size_t size() { return bytes ? bytes->size() : 0; }
 size_t position() { return pos; }
 bool seek(size_t p) { pos = p; return p <= size(); }
 size_t read(uint8_t* p, size_t n) { n = std::min(n, size()-pos); memcpy(p,bytes->data()+pos,n); pos+=n; return n; }
 size_t write(const uint8_t* p, size_t n) { bytes->insert(bytes->end(),p,p+n); return n; }
 void flush() {}
 void close() { bytes.reset(); }
};
#define FILE_READ 0
#define FILE_WRITE 1
struct FakeFS {
 std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
 File open(const String& name, int mode) {
  if (mode == FILE_WRITE) files[name] = std::make_shared<std::vector<uint8_t>>();
  File f; if(files.count(name)) f.bytes=files[name]; return f;
 }
 size_t totalBytes() { return 1000000; }
 size_t usedBytes() { return 1000; }
 bool exists(const String& n) { return files.count(n); }
 bool remove(const String& n) { return files.erase(n); }
 bool rename(const String& a,const String& b) { if(!files.count(a))return false; files[b]=files[a]; files.erase(a); return true; }
} LittleFS;
struct Record { int64_t version=-1; int bank=0; };
struct Settings { bool leave,arrive; uint8_t volume; };
struct Storage {
 bool mounted=true,failCommit=false;
 Record records[2]; Settings settings={true,true,70};
 String path(int slot,int bank) { return String(slot?"/leave":"/arrive")+std::to_string(bank)+".wav"; }
 bool saveSettings(Settings v) { settings=v; return true; }
 bool commitAudio(int slot,int bank,int64_t version) {
  if(failCommit)return false; records[slot]={version,bank}; return true;
 }
};
struct StorageGuard { explicit StorageGuard(Storage&) {} };
struct EventManager {
 String pending;
 String front() { return pending; }
 bool acknowledge(const String& value) { assert(value==pending); pending.clear(); return true; }
};
struct WiFiClient { virtual ~WiFiClient() = default; };
struct WiFiClientSecure : WiFiClient {
 bool rooted=false; void setCACert(const char*) {rooted=true;}
 void setInsecure() { assert(false); } void setHandshakeTimeout(int) {}
};
constexpr int WL_CONNECTED=3,WIFI_STA=1;
struct { int status(){return WL_CONNECTED;} void disconnect(){} void begin(const char*,const char*){} void mode(int){} void setAutoReconnect(bool){} } WiFi;
struct Reply { int status=200; std::string body; int declared=-1; int truncate=-1; };
std::map<std::string,Reply> replies;
std::map<std::string,int> calls;
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
struct HTTPClient {
 String url; Reply response;
 void setConnectTimeout(int){} void setTimeout(int){} void setReuse(bool){} void setFollowRedirects(int){}
 bool begin(WiFiClientSecure& c,const String& u) { assert(c.rooted); url=u; return true; }
 bool begin(WiFiClient&,const String& u) { url=u; return true; }
 void addHeader(const char*,const char*){}
 int GET() { ++calls[url]; response=replies.at(url); return response.status; }
 int POST(const String&) { return GET(); }
 int getSize() {return response.declared;}
 int writeToStream(Stream* out) {
  size_t n=response.truncate<0?response.body.size():size_t(response.truncate);
  return int(out->write(reinterpret_cast<const uint8_t*>(response.body.data()),n));
 }
 void end(){}
 static String errorToString(int) {return "mock error";}
};
void configTime(int,int,const char*,const char*) {}
struct StopTask {};
void vTaskDelay(int) { throw StopTask(); }
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
int xTaskCreate(void(*fn)(void*),const char*,int,void* p,int,void*) { try{fn(p);}catch(StopTask&){} return pdPASS; }
#define PROGMEM
#define API_TLS_INSECURE false
#define WIFI_SSID "test"
#define WIFI_PASSWORD "test"
namespace Config {
constexpr char baseUrl[]="https://test.invalid",deviceId[]="entrance-1";
constexpr uint32_t maxWav=200*1024,heartbeatMs=20000;
}
