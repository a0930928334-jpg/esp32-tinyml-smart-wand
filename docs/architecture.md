# System Architecture

## Overview

This project implements an embedded gesture-control prototype using an ESP32, an MPU6050 IMU sensor, an Edge Impulse TinyML model, and NEC infrared communication.

The project is designed as a working embedded AI prototype for simulated smart-home control. It is not a production-grade smart-home platform.

## Core Pipeline

```text
User gesture
    ↓
MPU6050 six-axis IMU data collection
    ↓
Sliding-window sensor buffer
    ↓
Physical wake-up threshold
    ↓
Edge Impulse TinyML on-device inference
    ↓
Confidence thresholding and noise suppression
    ↓
Secondary physical motion validation
    ↓
Finite-state command logic
    ↓
NEC infrared transmission
    ↓
Arduino/Nano receiver
    ↓
LED and servo-based simulated device response
```

## Main Modules

### 1. Sensor Data Collection

The MPU6050 provides six-axis motion data:

- ax, ay, az
- gx, gy, gz

The ESP32 reads these values through I2C and stores them in a fixed-size sliding-window buffer.

### 2. Sliding-Window Buffer

A single IMU reading cannot represent a full gesture. The firmware therefore keeps a continuous time-series window of IMU data. Older values are shifted forward and the newest frame is appended to the end of the buffer.

This allows the TinyML model to classify motion patterns rather than isolated sensor readings.

### 3. Physical Wake-Up

Before running the classifier, the firmware checks whether the motion intensity is strong enough. This helps avoid unnecessary inference and reduces weak false triggers caused by small movements.

### 4. TinyML Inference

The Edge Impulse exported model runs on the ESP32. It outputs confidence scores for five gesture classes:

- Z
- HUAQUAN
- QIANCI
- SHANGTIAO
- ZAOSHENG

### 5. Robustness Filtering

The firmware does not directly trust the highest model score. It applies additional checks:

- confidence thresholding
- noise suppression
- cooldown control
- secondary physical validation
- prefix timeout

These mechanisms reduce false triggers, but they do not make the prototype fully robust in all environments.

### 6. Finite-State Command Logic

The system uses a finite-state machine based on prefix gestures and trigger gestures.

- QIANCI and SHANGTIAO are prefix gestures.
- HUAQUAN and Z are trigger gestures.
- ZAOSHENG is treated as noise and does not trigger control actions.

This design allows a small gesture set to generate six simulated control commands.

### 7. Infrared Transmission and Receiver

After a command is generated, the ESP32 sends a 32-bit NEC infrared code. The Arduino/Nano receiver decodes the signal and controls an LED and servo motors to simulate light, curtain, and door actions.
