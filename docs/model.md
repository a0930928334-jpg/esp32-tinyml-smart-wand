# TinyML Model

## Overview

The gesture recognition model was trained using Edge Impulse and exported for Arduino-compatible embedded deployment. The exported model is integrated into the ESP32 firmware and used for on-device gesture classification.

## Input Data

The model input is a sliding window of six-axis IMU data from the MPU6050:

```text
ax, ay, az, gx, gy, gz
```

Each gesture is treated as a short time-series pattern rather than a single static reading.

## Gesture Classes

| Class | Meaning | Role |
|---|---|---|
| Z | Z-shaped gesture | Trigger gesture for off/close actions |
| HUAQUAN | Circle gesture | Trigger gesture for on/open actions |
| QIANCI | Forward thrust | Prefix gesture for door control |
| SHANGTIAO | Upward swing | Prefix gesture for curtain control |
| ZAOSHENG | Noise | Invalid class used to suppress false triggers |

## Deployment

The ESP32 firmware integrates the Edge Impulse exported model and calls the classifier on buffered IMU data.

The firmware uses the model output as one part of the decision process, not as the final decision by itself. The model confidence is combined with physical thresholds, noise suppression, cooldown logic, and secondary validation.

## Validation Metrics

| Metric | Result | Meaning |
|---|---:|---|
| Edge Impulse validation accuracy | 95.8% | Validation-set result |
| Validation loss | ~0.11 | Edge Impulse validation result |
| Weighted precision / recall / F1 | ~0.96 | Edge Impulse validation result |
| On-device inference time | ~1 ms | ESP32-side deployment estimate |
| Peak RAM usage | ~1.6 KB | Model deployment estimate |
| Flash usage | ~16.9 KB | Model deployment estimate |

## Important Metric Boundary

The 95.8% accuracy is the Edge Impulse validation-set accuracy. It should not be presented as real-world physical recognition accuracy.

Physical prototype performance is lower because it is affected by user motion differences, gesture amplitude, holding direction, sensor noise, and infrared directionality.

## What This Model Demonstrates

This model demonstrates:

- TinyML deployment on ESP32
- IMU-based gesture recognition
- on-device inference
- time-series sensor data processing
- practical model integration with embedded control logic

## What This Model Does Not Claim

This model does not claim:

- production-grade gesture recognition
- state-of-the-art recognition performance
- reliable operation in all real-world environments
- commercial smart-home readiness
