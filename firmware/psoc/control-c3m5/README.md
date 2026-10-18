# Step 1: UART TX試験（今回の作業）

独立プロジェクト [uart-hello/README.md](uart-hello/README.md) を使用してください。
C3M5からSCB4のP3.3へ `HELLO,1\n`、`HELLO,2\n` を約1秒ごとに送信します。
P3.2はRXとして設定しますが、本試験では受信処理は行いません。
I²C、AI、Rails API、センサーフュージョンは本試験に含めていません。

PowerShellでリポジトリルートから:

```powershell
.\firmware\psoc\control-c3m5\uart.ps1 getlibs
.\firmware\psoc\control-c3m5\uart.ps1 build
.\firmware\psoc\control-c3m5\uart.ps1 program
```

Atom側の最終受信試験は [uart-rx](../../m5stack/atoms3r-cam/tests/uart-rx/README.md) を使用します。
HELLO,149〜HELLO,215の67行を欠番なく受信し、USB表示まで確認済みです。
`atom-uart-monitor/` は初期切り分け用です。配線・書き込み・確認結果は各試験READMEに記載します。
以下は既存統合アプリの記録で、今回の試験には使用しません。

---

# C3M5 communication hub

PSoC Control C3M5 Evaluation Kit (`KIT_PSC3M5_EVK`) application for polling the E84 radar and vision targets over one I²C bus, fusing supported events, and forwarding actions to AtomS3R-CAM over UART. The application uses ModusToolbox 3.9 and the Infineon CAT1 HAL.

## Connections

| Link | C3M5 | Remote board | Settings |
| --- | --- | --- | --- |
| I²C | J3.9/P9.2 SDA, J3.10/P9.0 SCL, GND | E84 AI Kit J14.18 SDA, J14.17 SCL, J14.20 GND | Controller, 100 kHz, target `0x30` |
| I²C | J12.6/P9.2 SDA, J12.5/P9.0 SCL, GND | E84 Evaluation Kit J3.9 SDA, J3.10 SCL, GND | Same bus, target `0x31` |
| UART | J4.2/P3.3 TX, J4.1/P3.2 RX, GND | AtomS3R-CAM GPIO2 RX, GPIO1 TX, GND | 115200, 8-N-1 |

Use separate USB power for each board and connect grounds. Before powering the setup, confirm there are no shorts, the E84 Evaluation Kit `VDD_PERI` is 3.3 V, the I²C pull-ups and bus capacitance are suitable, and KitProg3 is not actively driving the shared P9.2/P9.0 I²C lines. Stop before powering or programming if any voltage or wiring detail is uncertain. The firmware cannot determine these electrical conditions.

The BSP maps `CYBSP_I2C_SDA`/`CYBSP_I2C_SCL` to P9.2/P9.0 and `CYBSP_UART_TX`/`CYBSP_UART_RX` to P3.3/P3.2. Confirm those mappings against the selected BSP and board revision before connecting external targets. The status LEDs use the BSP LED aliases and BSP polarity definitions.

## Build and program

From a ModusToolbox 3.9 command prompt on Windows:

```powershell
cd firmware/psoc/control-c3m5
make getlibs build
make program
```

Select the KitProg3 programmer/debugger when prompted. Build artifacts and downloaded libraries are ignored by Git. This workspace does not include a prebuilt binary.

## Firmware behavior

- Polls each target every 50 ms. Each I²C read has a 10 ms timeout and up to three attempts.
- Validates the complete 24-byte protocol v1 snapshot, source, kind/code, reserved bytes, flags, confidence, and CRC-8 (poly `0x07`, init `0x00`). `123456789` yields `0xF4`.
- Tracks each target's latest receive time, boot ID, sequence, kind/code, availability, malformed-frame count, and NACK/timeout count. A repeated `(source, boot_id, seq)` is ignored; a new boot ID permits sequence numbers to restart.
- Routes vision left/right passage to `LEFT_TO_RIGHT`/`RIGHT_TO_LEFT`, radar PUSH to `REPLAY`, and suppresses unknown direction. CIRCLE and RADAR_MOTION do not trigger audio. Fusion receives C3M5 time for a future bounded sensor-match window.
- Sends `EVT,1,BOOT_ID,SEQ,ACTION\n` and accepts only a matching `ACK,1,BOOT_ID,SEQ\n`. It retries after 200 ms up to three times after the initial send, then drops the item. ACK denotes queue acceptance, not playback completion.
- Parses UART input in a fixed 64-byte line buffer and discards overlong lines. I²C polling continues while an ACK is outstanding.

The event FIFO holds eight actions. LED1 (BSP user LED) blinks while running or when all sensors are unavailable; LED2 (BSP user LED2) lights briefly when an event is queued. State is kept in the debugger-visible `state` variable (`INIT`, `RUNNING`, `SENSOR_ERROR`, `UART_ERROR`); per-sensor counters and UART retry state are likewise debugger-visible. Runtime status is not currently mirrored to the USB debug console.

## Bring-up sequence

1. **C3M5 alone:** build and program, confirm LED1 heartbeat, inspect the `state` variable, and single-step/check `protocol_v1_crc8` with ASCII `123456789` (expected `0xF4`). Verify the UART pins with a logic analyzer before attaching Atom.
2. **UART pair:** connect grounds and crossed TX/RX. With the matching Atom firmware running, produce a supported test event and confirm the EVT line, matching ACK, and retry behavior by temporarily withholding ACK in a controlled bench test.
3. **Radar I²C:** connect only the E84 AI Kit; confirm target `0x30` ACK/read, valid CRC, then PUSH handling.
4. **Vision I²C:** connect only the E84 Evaluation Kit; confirm target `0x31` ACK/read, then test both directions and UNKNOWN.
5. **Combined setup:** test both directions and PUSH, duplicate frames, target reboot with a new boot ID, missing ACK, and removal of either sensor while confirming the other keeps polling.

Build success only confirms compilation. Programming, serial traces, I²C capture, and actual sound playback require the physical boards and must be reported as separate checks.

## Source modules

- `i2c_controller`: bounded controller reads and retry policy
- `protocol_v1`: snapshot validation and CRC-8
- `sensor_registry`: per-target status and duplicate suppression
- `event_fusion`: mapping sensor events to actions
- `event_queue`: bounded UART action FIFO
- `uart_link`: EVT/ACK parser, pending message, and retry handling
- `main`: BSP startup, periodic polling, status state, and LEDs
