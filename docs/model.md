# Model

The gesture classifier was trained in Edge Impulse and exported as an Arduino library. It runs on the ESP32-S3; no data leaves the device.

## Input

A sliding window of six-axis IMU data from the MPU6050, sampled every 10 ms:

```text
ax, ay, az (m/s^2), gx, gy, gz (rad/s)
```

The sensor is configured for ±8 g, ±500 °/s and a 21 Hz low-pass filter. The firmware must use the same settings as the training recordings, otherwise the model sees data on a different scale.

## Classes

| Label | Gesture | Role |
|---|---|---|
| `huaquan` | Circle | Trigger: on / open |
| `Z` | Z-shaped stroke | Trigger: off / close |
| `qianci` | Forward thrust | Prefix: selects the door |
| `shangtiao` | Upward swing | Prefix: selects the curtain |
| `zaosheng` | Noise (any other movement) | Never triggers an action |

The labels are the pinyin names used when the data was recorded, and the export keeps their exact spelling (lower case except `Z`). The firmware matches them case-sensitively. The noise class matters as much as the gestures: without it, every strong movement would be forced into one of the four gesture classes.

## Results

| Metric | Result | Source |
|---|---:|---|
| Validation accuracy | 95.8% | Edge Impulse validation set |
| Validation loss | ~0.11 | Edge Impulse |
| Weighted precision / recall / F1 | ~0.96 | Edge Impulse |
| Inference time | ~1 ms classifier, ~21 ms with spectral features | Edge Impulse estimate, Cortex-M4F at 80 MHz |
| Peak RAM | ~1.6 KB classifier, ~3.2 KB with spectral features | Edge Impulse estimate |
| Flash | ~16.9 KB (int8, EON Compiler); 21.0 KB as float32 | Edge Impulse estimate |

The timing and memory figures are Edge Impulse's deployment estimates for its default target, a Cortex-M4F at 80 MHz. They were not measured on the ESP32-S3, whose 240 MHz core should be faster. Quantising to int8 did not change validation accuracy (95.8% for both int8 and float32) and saved about 20% of flash. Measuring the real numbers on an Arm board is listed in [limitations.md](limitations.md).

The 95.8% figure is measured on data held out from the same recordings as the training set. Recognition on the physical prototype was about 90%; [testing.md](testing.md) covers that gap.

## How the firmware uses the model

The model output is one input to the decision, not the decision itself. The firmware:

1. Skips inference unless the window contains a peak acceleration of at least 13 m/s².
2. Requires the best gesture class to score above 0.80 and the noise class below 0.75.
3. For thrust and swing, also requires the motion to pass simple physical checks.
4. Ignores new gestures for 1 second after one is accepted.

Step 3 exists because the model learned a shortcut: it sometimes labelled other motion as a thrust or a swing. More varied training data would be the proper fix; the physical check is a workaround.

## Rebuilding

The export is not committed to git. Download `ei-smartwand2-arduino-1.0.2.zip` from the [GitHub release](https://github.com/a0930928334-jpg/esp32-tinyml-smart-wand/releases) and unzip it into `firmware/wand-esp32s3/lib/SmartWand2_inferencing/`, or export the Edge Impulse project yourself as an Arduino library (Quantized int8, EON Compiler). The firmware includes it as:

```cpp
#include <SmartWand2_inferencing.h>
```

At start-up the firmware looks up each class by label name, so a retrained model may list its labels in any order, but the five label names must stay the same.
