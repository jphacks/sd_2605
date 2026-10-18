#include <M5Unified.h>
#include <HardwareSerial.h>

static HardwareSerial c3m5Uart(1);
static constexpr size_t MaxLineLength = 64;
static char line[MaxLineLength + 1];
static size_t lineLength = 0;
static bool discardLine = false;
static uint8_t sample[16];
static size_t sampleLength = 0;

static void receiveByte(char value)
{
    if (value == '\n') {
        if (discardLine) {
            Serial.print("[UART] Dropped invalid or overlong line; first bytes:");
            for (size_t i = 0; i < sampleLength; ++i) Serial.printf(" %02X", sample[i]);
            Serial.println();
        } else if (lineLength != 0) {
            line[lineLength] = '\0';
            Serial.print("[UART RX] ");
            Serial.println(line);
        }
        lineLength = 0;
        discardLine = false;
        sampleLength = 0;
        return;
    }
    if (sampleLength < sizeof(sample)) sample[sampleLength++] = static_cast<uint8_t>(value);
    if (value == '\r') return; // Accept CRLF as well as LF.
    if (discardLine) return;
    if (value < 0x20 || value > 0x7e || lineLength == MaxLineLength) {
        discardLine = true;
        return;
    }
    line[lineLength++] = value;
}

void setup()
{
    auto config = M5.config();
    config.internal_imu = false;
    config.internal_rtc = false;
    config.internal_mic = false;
    config.internal_spk = false;
    config.external_imu = false;
    config.external_rtc = false;
    config.external_speaker_value = 0;
    config.external_display_value = 0;
    M5.begin(config);

    Serial.begin(115200); // USB CDC, independent of hardware UART1.
    c3m5Uart.setRxBufferSize(1024);
    c3m5Uart.begin(115200, SERIAL_8N1, 2, 1);
    const uint32_t start = millis();
    while (!Serial && millis() - start < 3000) delay(10);
    Serial.println("[UART] Waiting for C3M5...");
}

void loop()
{
    M5.update();
    while (c3m5Uart.available()) {
        receiveByte(static_cast<char>(c3m5Uart.read()));
    }
    delay(1);
}
