#include <M5Unified.h>

HardwareSerial uartPort(1);

void setup()
{
    Serial.begin(115200);
    auto config = M5.config();
    config.internal_imu = false;
    config.internal_rtc = false;
    config.internal_mic = false;
    config.internal_spk = false;
    M5.begin(config);
    uartPort.setRxBufferSize(1024);
    uartPort.begin(115200, SERIAL_8N1, 2, 1);
    Serial.println("UART monitor: RX=GPIO2 TX=GPIO1 115200 8N1");
}

void loop()
{
    static uint32_t bytes = 0;
    static uint32_t lastStatus = 0;
    M5.update();
    while (uartPort.available()) {
        Serial.write(uartPort.read());
        ++bytes;
    }
    if (millis() - lastStatus >= 5000) {
        lastStatus = millis();
        Serial.printf("[MONITOR] bytes=%lu RX=2 TX=1 baud=115200\n", (unsigned long)bytes);
    }
    delay(1);
}

