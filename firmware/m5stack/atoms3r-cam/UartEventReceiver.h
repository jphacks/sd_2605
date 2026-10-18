#pragma once
#include <HardwareSerial.h>
#include <atomic>
#include "ProtocolParser.h"
class UartEventReceiver {
  HardwareSerial port{1};
  QueueHandle_t queue = nullptr;
  DoorProtocol::Dedup dedup;
  char line[DoorProtocol::maxLine + 1]{};
  size_t length = 0;
  bool overflow = false;
  std::atomic<uint32_t> acceptedCount{0}, duplicateCount{0}, rejectedCount{0};
  void receive(char c) {
    if (c == '\n') {
      if (length && line[length - 1] == '\r') --length;
      line[length] = 0;
      DoorProtocol::Event event;
      if (!overflow && DoorProtocol::parse(line, event)) {
        bool accepted = dedup.contains(event);
        if (accepted) ++duplicateCount;
        if (!accepted && dedup.canAdd(event) && xQueueSend(queue, &event.action, 0) == pdTRUE) {
          dedup.add(event);
          ++acceptedCount;
          accepted = true;
        }
        if (accepted) port.printf("ACK,1,%08lX,%lu\n", (unsigned long)event.boot,
                                  (unsigned long)event.seq);
        if (!accepted) ++rejectedCount;
        // No ACK on queue/dedup exhaustion: sender must retry, never silently lose acceptance.
      } else if (length || overflow) ++rejectedCount;
      length = 0; overflow = false;
    } else if (!overflow) {
      if ((uint8_t(c) < 32 && c != '\r') || uint8_t(c) > 126) { overflow = true; return; }
      if (length < DoorProtocol::maxLine) line[length++] = c;
      else overflow = true;
    }
  }
  static void task(void* p) {
    auto& self = *static_cast<UartEventReceiver*>(p);
    for (;;) {
      for (unsigned i = 0; i < 256 && self.port.available(); ++i)
        self.receive(self.port.read());
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
 public:
  void printStatus() {
    Serial.printf("[UART] accepted=%lu duplicate=%lu rejected=%lu pending=%u RX=2 TX=1 baud=115200\n",
      (unsigned long)acceptedCount.load(), (unsigned long)duplicateCount.load(),
      (unsigned long)rejectedCount.load(), queue ? unsigned(uxQueueMessagesWaiting(queue)) : 0);
  }
  bool begin() {
    queue = xQueueCreate(16, sizeof(DoorProtocol::Action));
    if (!queue) return false;
    port.setRxBufferSize(1024);
    port.begin(115200, SERIAL_8N1, 2, 1);
    return xTaskCreate(task, "door-uart", 3072, this, 2, nullptr) == pdPASS;
  }
  bool enqueue(DoorProtocol::Action a) { return queue && xQueueSend(queue, &a, 0) == pdTRUE; }
  bool next(DoorProtocol::Action& a) { return queue && xQueueReceive(queue, &a, 0) == pdTRUE; }
};
