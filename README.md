# ESP32 Smart Wand: TinyML-Based Gesture Recognition and IoT Control Prototype

[![host-tests](https://github.com/a0930928334-jpg/esp32-tinyml-smart-wand/actions/workflows/host-tests.yml/badge.svg)](https://github.com/a0930928334-jpg/esp32-tinyml-smart-wand/actions/workflows/host-tests.yml)

An embedded AI prototype that uses an ESP32-S3, an MPU6050 IMU sensor, and an Edge Impulse TinyML model to recognize hand gestures on-device and trigger simulated smart-home control actions through NEC infrared signals.

This project was originally developed as my undergraduate final-year project. It is packaged here as a technical portfolio project to demonstrate embedded software development, sensor data processing, TinyML deployment, finite-state command logic, and hardware-software integration.

> This is a working prototype for simulated smart-home control.  
> It is not a production-grade smart-home platform or a commercial IoT product.

## At a Glance

| | |
|---|---|
| On-device inference | ~1 ms per window on the ESP32-S3 |
| Model footprint | ~1.6 KB peak RAM, ~16.9 KB flash |
| Validation accuracy | 95.8% (Edge Impulse validation set) |
| Physical tests | ~90% single gesture, ~85% two-step command, ~89% infrared delivery |
| Commands | 6, built from 5 gesture classes by a two-step state machine |
| Host unit tests | 56 checks on the decision logic and shared headers, run in CI |

## Demo

<img src="media/demo.gif" width="320" alt="Demo of ESP32 Smart Wand gesture control">

The demo shows the smart wand sending gesture-triggered infrared commands to a simulated receiver with LED and servo-based responses.

## Tech Stack

- **Microcontrollers:** ESP32-S3 (wand), Arduino Nano / ATmega328P (receiver)
- **Language:** C++ (Arduino framework, PlatformIO)
- **Sensor:** MPU6050 six-axis IMU
- **ML Deployment:** Edge Impulse TinyML
- **Communication:** NEC infrared transmission
- **Control Logic:** Sliding-window processing, confidence filtering, finite-state machine
- **Actuation:** LED and servo-based simulated device control
- **Testing:** host unit tests (GCC, Clang, AddressSanitizer/UBSan) in GitHub Actions

## Repository Layout

```text
firmware/
  common/            ir_codes.h, timing.h   shared by both boards
  wand-esp32s3/      sensing, inference, gesture logic, IR sender
  receiver-nano/     IR decoding, LED and servo control
tests/               host unit tests for the decision logic (make -C tests)
docs/                architecture, hardware, model, testing, limitations, engineering notes
model/               Edge Impulse export notes
```

## Build and Test

```bash
make -C tests                                   # host unit tests, no hardware needed
cd firmware/wand-esp32s3 && pio run -t upload    # needs the Edge Impulse library, see its README
cd firmware/receiver-nano && pio run -t upload
```

## Quick Links

- [Engineering Notes: issues found in code review and how they were fixed](docs/engineering-notes.md)
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
