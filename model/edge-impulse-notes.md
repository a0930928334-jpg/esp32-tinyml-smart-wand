# Edge Impulse Model Notes

## Model Role

The Edge Impulse model is used for gesture classification on the ESP32. It classifies a sliding window of MPU6050 six-axis IMU data into five gesture classes.

## Classes

- Z
- HUAQUAN
- QIANCI
- SHANGTIAO
- ZAOSHENG

## Deployment Note

The ESP32 firmware includes the Edge Impulse exported Arduino library.

The main firmware references the generated model header:

```cpp
#include <SmartWand2_inferencing.h>
```

To reproduce the firmware build, export the trained model from Edge Impulse as an Arduino library and place the generated library where the firmware can include it.

## Metric Boundary

Validation accuracy should be described as Edge Impulse validation-set accuracy only.

It should not be described as real-world accuracy.

## Honest Description

Recommended wording:

"Deployed an Edge Impulse TinyML gesture classifier on ESP32 for on-device inference over six-axis IMU time-series data."

Avoid wording:

- state-of-the-art model
- production-grade AI
- fully robust gesture recognition
- commercial smart-home AI system
