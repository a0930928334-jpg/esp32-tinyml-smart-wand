# Hardware

## Overview

The project contains two hardware sides:

1. ESP32 smart wand side
2. Arduino/Nano receiver side

The ESP32 side handles sensing, inference, command decision, and infrared transmission. The Arduino/Nano side receives infrared commands and simulates device responses using an LED and servo motors.

## ESP32 Smart Wand Side

| Module | Function |
|---|---|
| ESP32-S3 development board | Main controller for sensing, inference, state logic, and infrared transmission |
| MPU6050 IMU | Collects three-axis acceleration and three-axis gyroscope data |
| Infrared transmitter | Sends 32-bit NEC infrared control codes |
| LED | Provides simple status feedback |

Wand-side wiring:

| Device | ESP32-S3 pin |
|---|---|
| MPU6050 SDA | GPIO4 |
| MPU6050 SCL | GPIO5 |
| IR LED (via transistor) | GPIO6 |
| Status LED | GPIO2 |

GPIO6 is free on the ESP32-S3. On a classic ESP32-WROOM module, GPIO6–11 are connected to the flash chip and cannot be used.

## Arduino/Nano Receiver Side

| Module | Function |
|---|---|
| Arduino/Nano | Receiver-side controller |
| IR receiver module | Receives and decodes infrared commands |
| LED | Simulates light on/off control |
| Servo motor 1 | Simulates door movement |
| Servo motor 2 | Simulates curtain movement |
| External 5V power supply | Powers the servo motors |

## Wiring Notes

The servo motors should use an external 5V power supply. The external power supply ground and the Arduino/Nano ground must be connected together.

Basic receiver-side wiring:

| Device | Pin |
|---|---|
| IR receiver OUT | D2 |
| LED | D13 |
| Door servo | D9 |
| Curtain servo | D10 |

## Things that affected reliability

- Loose jumper wires caused unstable behaviour.
- The servos need more current than the Nano can supply, hence the separate 5 V supply.
- The infrared LED and receiver need to be roughly aligned.
- The MPU6050 must be fixed firmly to the wand; if it shifts, the gesture data changes.
