# Testing

The project was tested at three levels: the model on held-out data, the prototype in the hand, and the decision logic in unit tests.

## 1. Model validation (Edge Impulse)

| Metric | Result |
|---|---:|
| Validation accuracy | 95.8% |
| Validation loss | ~0.11 |
| Weighted precision / recall / F1 | ~0.96 |

## 2. Physical prototype

| Test | Result |
|---|---:|
| Single gesture recognised correctly | ~90% |
| Two-step command completed (prefix, then trigger) | ~85% |
| Infrared command received and executed | ~89% |

The two-step commands are:

| First gesture | Then | Command |
|---|---|---|
| | Circle | Light on |
| | Z | Light off |
| Upward swing | Circle | Curtain open |
| Upward swing | Z | Curtain close |
| Forward thrust | Circle | Door open |
| Forward thrust | Z | Door close |

## Why the prototype scores lower than the validation set

- **Held-out data is still similar data.** The validation set is split from the same recordings as the training set, so it shares the same grip, speed and gesture size. Real use varies all three.
- **Two gestures must both succeed.** A two-step command needs two correct recognitions in a row. Two independent 90% steps give about 81%, close to the 85% measured.
- **Extra gates reject real gestures.** The wake-up threshold and the physical checks cut false triggers, and they also reject weak or incomplete gestures that the model alone would have accepted.
- **Infrared is directional.** The command can be recognised correctly and still not arrive if the wand is not pointing at the receiver, which is why each command is sent six times.

Problems seen during testing: some movements were misread as a forward thrust, weak gestures were ignored, and servo behaviour depended on the power supply.

## 3. Host unit tests

The decision logic (`gesture_logic.h`) and the shared headers have no Arduino dependency, so they are tested on a PC:

```bash
make -C tests
```

56 checks cover the infrared encode/decode round trip, timer behaviour across a 32-bit counter wrap, the wake-up gate, the confidence and physical checks, all six two-step commands, and prefix timeout. GitHub Actions runs them with GCC and Clang, and again with AddressSanitizer and UBSan.

These tests were written during the code review described in [engineering-notes.md](engineering-notes.md). They verify the logic and complement the physical tests above.
