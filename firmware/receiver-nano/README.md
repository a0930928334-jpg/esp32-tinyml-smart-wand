# Receiver firmware (Arduino Nano)

Decodes the wand's NEC infrared frames and drives a model house: an LED for the light and two
360-degree servos for the door and the curtain.

## Wiring

| Part | Nano pin |
|---|---|
| IR receiver OUT | D2 |
| Light LED | D13 |
| Door servo signal | D9 |
| Curtain servo signal | D10 |

Power both servos from a separate 5 V supply and connect its GND to the Nano's GND. Powered from the
Nano alone, the servos brown out the board.

## How it works

- `IrReceiver.decodedIRData.decodedRawData` holds the frame least-significant bit first. The wand
  sends most-significant bit first, so `ir::decode_lsb_first()` reverses the bits before looking the
  command up in the shared table (`../common/ir_codes.h`).
- The wand sends every command 6 times. The receiver ignores a repeat of the same command within
  2 seconds.
- The servos are continuous-rotation: `write()` sets speed and direction, not angle. Each movement
  runs for a fixed time (door 100 ms, curtain 1000 ms). The servo is then detached so it cannot creep.
  This is non-blocking, so new commands are still received while a servo moves.

## Build

`pio run -e nano -t upload && pio device monitor`
