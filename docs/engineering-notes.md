# Engineering Notes: Issues Found in Code Review

A code review after the project was submitted found the issues below. The prototype hardware had
already been taken apart, so the fixes are verified by host unit tests (`tests/`, run in CI) and
have not been re-tested on the device. Where a change could affect behaviour, the behaviour tested
on the prototype was kept.

| # | Issue | Effect | Fix | Test |
|---|---|---|---|---|
| 1 | IR bit order handled by hand-reversed constants | 4 of 6 command labels crossed between sender and receiver | One shared command table; receiver reverses bits explicitly | `test_every_command_survives_the_bit_order_round_trip` |
| 2 | Sample-period wait used an absolute 64-bit deadline against 32-bit `micros()` | Main loop could freeze for ~71 minutes after ~71.6 minutes of uptime | Wrap-safe unsigned subtraction (`timing.h`) | `test_old_absolute_deadline_style_fails_at_wrap` |
| 3 | Upward-swing "force" check could never fail | Dead code that looked like a safeguard | Removed and documented | `test_physical_veto_for_thrust_and_swing` |
| 4 | Model class indices hard-coded | Retraining could silently remap gestures | Scores looked up by label name; label count checked at compile time | Start-up check |
| 5 | Logic mixed with I/O in one file | Could only be tested on hardware | Decision logic moved to `gesture_logic.h` | 56 host checks |

## 1. Infrared bit order

NEC sends each frame least-significant bit first. The two libraries describe the same 32 bits in
opposite orders:

- On the wand, IRremoteESP8266's `sendNEC(0xAA000001, 32)` takes the value **MSB-first**.
- On the receiver, IRremote's `decodedRawData` stores the received frame **LSB-first**, so the same
  frame reads `0x80000055`.

The original receiver handled this by hard-coding the bit-reversed values. Light on and light off
matched. For the other four commands, the receiver's labels were swapped:

| Wand sent | Receiver read it as |
|---|---|
| curtain open (`0xAA000003`) | curtain close |
| curtain close (`0xAA000004`) | curtain open |
| door open (`0xAA000006`) | door close |
| door close (`0xAA000005`) | door open |

The prototype still behaved correctly. The servos are continuous-rotation, and their direction
constants had been tuned by trial and error, which cancelled the swap. The code was wrong, but the
error was hidden.

**Fix.** Both sides include `firmware/common/ir_codes.h`. The wand sends `ir::encode(command)`. The
receiver calls `ir::decode_lsb_first(raw)`, which reverses the 32 bits and checks the `0xAA0000`
prefix. Servo directions were moved so the physical behaviour is unchanged: "open" still turns
each servo the way it did in testing.

**Lesson.** When two libraries meet over a protocol, write the bit order down once, in shared
code, and test the round trip.

## 2. `micros()` overflow in the sampling loop

The loop held a fixed 10 ms sample period like this:

```cpp
uint64_t next_tick = micros() + EI_CLASSIFIER_INTERVAL_MS * 1000;
// ... work ...
while (micros() < next_tick) { }
```

On the ESP32 Arduino core, `micros()` returns a 32-bit value that wraps every 2^32 µs (about
71.6 minutes). Within 10 ms of a wrap, `next_tick` is computed in 64 bits and lands above 2^32.
`micros()` then wraps to a small value and stays below `next_tick` until it climbs back up, about
71 minutes later. During that time the wand stops sampling and appears frozen.

**Fix.** Store the start time and compare elapsed time with unsigned 32-bit subtraction, which is
correct across a wrap:

```cpp
const uint32_t tick_start_us = micros();
// ... work ...
while (!timing::has_elapsed(tick_start_us, micros(), kSamplePeriodUs)) { }
```

All other timeouts (cooldown, prefix expiry, receiver servo timers and de-duplication) now use the
same helper. The receiver previously compared `millis() >= stop_ms`, which has the same class of bug
(after ~49.7 days).

## 3. A safety check that could not fail

The upward-swing gesture had a secondary check, `max_accel < 8.0` → reject. Inference only runs when
`max_accel >= 13.0` (the wake-up gate), so this check never rejected anything. It was removed rather
than replaced with a new threshold that could not be tuned without hardware. If the hardware is rebuilt, a better
check would use the vertical axis (for example a peak in `az` or `gy`), measured from recorded data.

## 4. Model output order

The firmware used fixed indices (`classification[0]` = Z, and so on). Edge Impulse orders labels
itself, so a retrained model could silently remap gestures to the wrong commands. The firmware now
looks up each class by label name at start-up, halts with a message if one is missing, and fails to
compile if the model has a different number of classes.

## 5. Testability

`main.cpp` previously contained everything: sensor I/O, feature extraction, thresholds and the state
machine. Pure logic now lives in `gesture_logic.h` (no Arduino or Edge Impulse includes), which
`tests/host_tests.cpp` covers on a desktop compiler:

- IR encode/decode round trip for all six commands, and rejection of foreign or repeat frames
- Timer behaviour across a 32-bit wrap, including a test showing the old code's failure
- Wake-up gate and feature extraction on synthetic IMU windows
- Confidence, noise and physical-veto rules
- All six two-step commands, prefix timeout and prefix replacement

CI runs the tests with GCC and Clang, then again with AddressSanitizer and UBSan.

## Known limitation kept on purpose

`send_command()` blocks for about 2 seconds (6 bursts, 400 ms apart). Sampling pauses during that
time, and the window is cleared afterwards. This is acceptable because the 1 s cooldown would reject
new gestures in that period anyway. A non-blocking sender would be needed before adding features
that must keep sampling, such as continuous gestures.
