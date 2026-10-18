# PSoC Edge E84 Evaluation Kit: vision passage node

## Official person segmentation sample and BSP

The Infineon [AI Hub deploy vision example](https://github.com/Infineon/mtb-example-psoc-edge-ml-aihub-deploy-vision) uses a USB camera, the DEEPCRAFT YOLACT person segmentation model, and draws detections over the camera image. Its README lists only `KIT_PSE84_EVAL_EPC2` as a supported kit and defaults `TARGET` to that BSP. The public model header exposes detections as `Box { x, y, w, h, score, coefficients[] }`; the model library that fills these values is prebuilt, so the header does not define coordinate scaling or mask-to-box conversion.

The Evaluation Kit has both EPC2 and EPC4 BSP variants. Select the BSP from the MCU on the SOM, not from the kit family name:

| SOM MCU marking | ModusToolbox target |
| --- | --- |
| `PSE846GPS2DBZC4A` | `KIT_PSE84_EVAL_EPC2` |
| `PSE846GPS4DBZC4A` | `KIT_PSE84_EVAL_EPC4` |

Infineon's BSP repository describes the EPC2 part, and the current board guide lists both BSPs. The deploy-vision sample does not list EPC4. The source sample has therefore not been copied or relabeled as EPC4 here. An EPC4 build and inference run must be established with the actual SOM before connecting the model adapter in `include/vision_ai.h`. This checkout has no ModusToolbox SDK/toolchain or connected board, so it cannot provide that hardware result.

## Current module boundary

- `vision_ai`: adapter contract for the USB frame and model-produced person boxes. The model-specific adapter is pending validation against the selected BSP and official model output.
- `person_tracker` / `direction_detector`: `direction_detector` consumes person boxes, filters small regions, tracks one person, compensates mirrored X coordinates, rejects abrupt associations and marks reversals or overlapping people as `UNKNOWN` after sufficient travel. A stationary detection alone returns no event.
- `vision_events`: turns a completed track into a passage event and increments `seq` once.
- `protocol_v1`: serializes the fixed 24-byte event with CRC-8 and provides a critical-section-protected snapshot.
- `i2c_target`: EZI2C target adapter. The generated Device Configurator instance and its pin/address settings are supplied by the board application.

The detector currently assumes its input boxes already describe people. It is deliberately not a generic motion detector. `min_area_ratio`, `min_displacement_ratio`, `min_frames`, `max_missed_frames`, `cooldown_frames`, `max_step_ratio`, `reversal_ratio`, and `mirrored_input` are runtime configuration fields.

## I²C and board setup

Wire E84 `J3.9/P8[1]` to C3M5 SDA, `J3.10/P8[0]` to C3M5 SCL, and GND to GND. Configure the selected BSP's EZI2C peripheral as a Target with 7-bit address `0x31`, 100 kHz bus timing, and a 24-byte read-only buffer (`rwBoundary=0`). The PSoC kit guide maps those P8 pins to J3.9/J3.10 and specifies that header voltage follows `VDD_PERI` (1.8 V or 3.3 V); check the board jumper/measurement and set `VDD_PERI` to 3.3 V before connecting to the C3M5. Do not infer this voltage from USB power.

Provide `protocol_lock_fn`/`protocol_unlock_fn` using a short interrupt-safe critical section. Publish events from one application task. Call `i2c_target_refresh()` from the application loop after state changes; it copies the immutable snapshot into the EZI2C transmit buffer while the same critical section prevents an ISR from observing a partial frame. The EZI2C interrupt handler must call `Cy_SCB_EZI2C_Interrupt()` with the generated context. The generated target config must bind to P8[1]/P8[0]; verify the generated pin aliases because board defaults may reserve I²C for a different peripheral.

Frame layout is defined in `include/protocol_v1.h` and follows protocol v1: magic `0xA5`, version 1, source 2, passage kind 3, direction code 1/2/3, confidence, valid flag, little-endian boot ID/sequence/uptime, zero reserved bytes, and CRC-8/0x07 over bytes 0–22. CRC test vector: ASCII `123456789` -> `0xF4`. Idle reports `kind=NONE`, `valid=0`, and retains the last sequence number. Supply a new boot ID at startup from a platform entropy source or a persistent boot counter.

## Host logic check

Run `tests/edge-e84/run-host-tests.sh` on a host with a C11 compiler. It checks the CRC vector, byte order and fixed packet size, stationary person suppression, both directions, reversal and overlap behavior, and snapshot serialization. This does not exercise camera capture, segmentation masks, lighting, moving non-person objects, I²C electrical timing, display drawing, or MCU compilation.

## Bring-up work remaining

1. Inspect the Evaluation Kit SOM marking and select the matching EPC2/EPC4 BSP.
2. On the actual board, build and run the official vision sample for that BSP. For EPC4, first validate that target explicitly; the upstream README currently lists EPC2 only.
3. Inspect the model library/sample data path to establish whether `Box` coordinates are normalized, model-space, or frame-space. Implement that conversion in the AI adapter, then wire its USB frame source and detection boxes into the tracker.
4. Port the sample drawing path for camera, boxes, centroid, trail, inference duration, direction, and I²C state. The host logic code does not initialize the MIPI display.
5. Generate/configure the EZI2C target pins and address for the chosen BSP; register its ISR and confirm timing at 100 kHz on hardware.
6. Run the lighting-change, moving-object, and full camera/I²C hardware scenarios. Build with ModusToolbox 3.9 and report compile, serial, and physical behavior separately.
