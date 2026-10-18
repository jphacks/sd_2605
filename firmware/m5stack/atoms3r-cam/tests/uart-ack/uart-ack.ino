#include <M5Unified.h>
#include <HardwareSerial.h>

// Test-only responder; production Atom firmware is unchanged.
// 0=normal, 1=no reply, 2=wrong sequence, 3=late reply.
#ifndef ACK_TEST_MODE
#define ACK_TEST_MODE 0
#endif
static HardwareSerial c3m5Uart(1);
static constexpr size_t MaxLineLength = 64;
static char line[MaxLineLength + 1];
static size_t lineLength = 0;
static bool discardLine = false;
static bool carriageReturn = false;
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
            if (strncmp(line, "HELLO,", 6) == 0 && line[6] != '\0') {
                uint32_t sequence = 0;
                bool valid = true;
                for (const char *p = line + 6; *p; ++p) {
                    if (*p < '0' || *p > '9') { valid = false; break; }
                    uint32_t digit = static_cast<uint32_t>(*p - '0');
                    if (sequence > (UINT32_MAX - digit) / 10u) { valid = false; break; }
                    sequence = sequence * 10u + digit;
                }
                if (valid && ACK_TEST_MODE != 1) {
                    if (ACK_TEST_MODE == 2) ++sequence;
                    if (ACK_TEST_MODE == 3) delay(600);
                    char ack[24];
                    snprintf(ack, sizeof(ack), "ACK,%lu\n", static_cast<unsigned long>(sequence));
                    c3m5Uart.print(ack);
                    Serial.print("[UART TX] ");
                    Serial.print(ack);
                }
            }
        }
        lineLength = 0;
        discardLine = false;
        carriageReturn = false;
        sampleLength = 0;
        return;
    }
    if (sampleLength < sizeof(sample)) sample[sampleLength++] = static_cast<uint8_t>(value);
    if (discardLine) return;
    // CR is accepted only immediately before LF, never inside a message.
    if (carriageReturn) { discardLine = true; return; }
    if (value == '\r') { carriageReturn = true; return; }
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
    Serial.println("[UART] ACK responder RX=2 TX=1 baud=115200");
}

void loop()
{
    M5.update();
    while (c3m5Uart.available()) {
        receiveByte(static_cast<char>(c3m5Uart.read()));
    }
    delay(1);
}
