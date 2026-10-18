#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <deque>
#include <string>
#include <cstring>
using QueueHandle_t = std::deque<uint8_t>*;
constexpr int pdTRUE=1, pdPASS=1, SERIAL_8N1=0;
inline QueueHandle_t xQueueCreate(int, size_t) { return new std::deque<uint8_t>; }
inline int xQueueSend(QueueHandle_t q, const void* a, int) {
  if(q->size() == 16) return 0;
  q->push_back(*static_cast<const uint8_t*>(a)); return 1;
}
inline int xQueueReceive(QueueHandle_t q, void* a, int) {
  if(q->empty()) return 0;
  *static_cast<uint8_t*>(a)=q->front(); q->pop_front(); return 1;
}
inline size_t uxQueueMessagesWaiting(QueueHandle_t q) { return q->size(); }
inline void vTaskDelay(int) {}
inline int pdMS_TO_TICKS(int n) { return n; }
inline int xTaskCreate(void(*)(void*), const char*, int, void*, int, void*) { return 1; }
class HardwareSerial {
 public:
  static std::string output;
  explicit HardwareSerial(int) {}
  void setRxBufferSize(int) {}
  void begin(int, int, int, int) {}
  int available() { return 0; }
  int read() { return -1; }
  void printf(const char* fmt, ...) {
    char s[256]; va_list args; va_start(args,fmt); vsnprintf(s,sizeof(s),fmt,args); va_end(args);
    output += s;
  }
};
extern HardwareSerial Serial;
