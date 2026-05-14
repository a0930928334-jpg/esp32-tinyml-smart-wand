# ESP32 Smart Wand: TinyML-Based Gesture Recognition and IoT Control Prototype

An embedded AI prototype that uses an ESP32, an MPU6050 IMU sensor, and an Edge Impulse TinyML model to recognize hand gestures on-device and trigger simulated smart-home control actions through NEC infrared signals.

This project was originally developed as my undergraduate final-year project. It is packaged here as a technical portfolio project to demonstrate embedded software development, sensor data processing, TinyML deployment, finite-state command logic, and hardware-software integration.

> This is a working prototype for simulated smart-home control.  
> It is not a production-grade smart-home platform or a commercial IoT product.
> ## Quick Links

- [System Architecture](docs/architecture.md)
- [Hardware Setup](docs/hardware.md)
- [TinyML Model](docs/model.md)
- [Testing Results](docs/testing.md)
- [Limitations](docs/limitations.md)

---

## 1. Project Overview

The system uses a handheld "smart wand" as an interaction device. When the user waves the wand, an MPU6050 six-axis IMU collects acceleration and gyroscope data. The ESP32 maintains a sliding-window buffer, performs physical wake-up detection, runs an on-device TinyML gesture classifier, applies robustness checks, and maps valid gestures into control commands using a finite-state machine.

The final command is transmitted through a 32-bit NEC infrared signal to an Arduino/Nano-based receiver. The receiver controls an LED and servo motors to simulate light, door, and curtain actions.

---

## 2. What Problem This Project Explores

Traditional smart-home interaction methods have trade-offs:

- Wall switches are reliable but fixed in location.
- Mobile apps can be powerful but require multiple interaction steps.
- Voice control can be convenient but may be affected by noise and privacy concerns.

This project explores whether a low-cost embedded device can perform local gesture recognition and convert motion gestures into simple simulated control commands without relying on cloud inference.

---

## 3. System Architecture

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
