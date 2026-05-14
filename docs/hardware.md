# Hardware

## Overview

The project contains two hardware sides:

1. ESP32 smart wand side
2. Arduino/Nano receiver side

The ESP32 side handles sensing, inference, command decision, and infrared transmission. The Arduino/Nano side receives infrared commands and simulates device responses using an LED and servo motors.

## ESP32 Smart Wand Side

| Module | Function |
|---|---|
| ESP32 development board | Main controller for sensing, inference, state logic, and infrared transmission |
| MPU6050 IMU | Collects three-axis acceleration and three-axis gyroscope data |
| Infrared transmitter | Sends 32-bit NEC infrared control codes |
| LED | Provides simple status feedback |

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

## Practical Hardware Issues

During prototype testing, several hardware-related issues can affect reliability:

- Loose jumper wires may cause unstable behavior.
- Servo motors may behave incorrectly without enough current.
- The IR transmitter and receiver require reasonable alignment.
- MPU6050 mounting stability affects gesture data quality.
- Holding direction and gesture amplitude influence recognition performance.

## Hardware Boundary

This project uses LED and servo motors to simulate smart-home devices. It does not directly control real household electrical equipment.
