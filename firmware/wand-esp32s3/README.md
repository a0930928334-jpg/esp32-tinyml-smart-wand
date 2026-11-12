# Wand firmware (ESP32-S3)

Reads the MPU6050 every 10 ms, classifies gestures on-device with an Edge Impulse model, and sends
the resulting command as a 32-bit NEC infrared frame.

## Files

| File | Role |
|---|---|
| `src/main.cpp` | I/O: IMU sampling, sliding window, classifier call, infrared output |
| `src/gesture_logic.h` | Pure C++ decision logic: features, wake-up gate, gesture selection, command state machine (unit-tested on the host) |
| `../common/ir_codes.h` | Command table shared with the receiver |
| `../common/timing.h` | Wrap-safe `millis()` / `micros()` helpers |

## Wiring

| Signal | ESP32-S3 pin |
|---|---|
| MPU6050 SDA | GPIO4 |
| MPU6050 SCL | GPIO5 |
| IR LED (via transistor) | GPIO6 |
| Status LED | GPIO2 |

GPIO6 is safe on the S3. On a classic ESP32-WROOM module, GPIO6–11 are wired to the flash chip, so
the IR pin would need to move.

## Build

1. Get the Edge Impulse export and unzip it into `lib/SmartWand2_inferencing/`. The model is not
   committed to git; the prebuilt export (`ei-smartwand2-arduino-1.0.2.zip`) is attached to the
   [GitHub release](https://github.com/a0930928334-jpg/esp32-tinyml-smart-wand/releases). To rebuild it instead, export the Edge Impulse project as an
   **Arduino library** (Quantized int8, EON Compiler).
2. `pio run -e esp32-s3 -t upload && pio device monitor`

The firmware checks at start-up that the model has exactly the five labels it expects
(`Z`, `huaquan`, `qianci`, `shangtiao`, `zaosheng`; case-sensitive) and looks each score up by label name, so the
model's output order does not matter.

## Key settings

| Setting | Value | Why |
|---|---|---|
| Sample period | 10 ms | Must match the training data's sampling rate |
| Inference | every 5th sample (50 ms) | Spectral features plus classifier are estimated at ~21 ms (Cortex-M4F, 80 MHz), so one window fits inside the 50 ms budget |
| Wake-up gate | peak \|a\| ≥ 13 m/s² | Skips inference for small movements |
| Confidence / noise | > 0.80 / < 0.75 | Rejects uncertain windows |
| Cooldown | 1000 ms | Stops one gesture from firing twice |
| Prefix timeout | 5000 ms | A forgotten prefix should not change a later command |
| IR repeats | 6 × 400 ms | Infrared has no acknowledgement; the receiver de-duplicates |
