# Limitations

This project is a working embedded AI prototype, but it has clear limitations.

## Dataset Limitations

The dataset size is limited. More samples from more users would be needed to improve generalization.

The model may perform well on the validation set but still behave less reliably in real physical use because user gestures vary in speed, direction, amplitude, and holding posture.

## Gesture Robustness

Recognition performance depends on:

- user motion style
- holding direction
- gesture amplitude
- sensor mounting stability
- IMU noise
- similarity between some gesture patterns

The robustness mechanisms reduce some false triggers, but they do not fully solve the problem.

## Physical Validation Boundary

The secondary validation logic uses simple physical features such as acceleration intensity and estimated movement.

The estimated movement distance is only a heuristic. It should not be described as accurate spatial tracking or precise displacement measurement.

## Infrared Control Boundary

Infrared control is useful for low-cost prototype demonstration, but it has limitations:

- It requires direction alignment.
- It can be affected by obstacles.
- It does not provide device state feedback.
- It is not equivalent to a full IoT communication system.

## Product Boundary

This project is not a commercial smart-home product.

It does not include:

- cloud backend
- mobile application
- Wi-Fi device management
- user authentication
- device state synchronization
- production enclosure
- long-term reliability testing
- safety certification

## Future Work

Potential improvements include:

- collecting more multi-user gesture data
- improving dataset balance
- adding more systematic physical testing
- comparing TinyML with rule-based baselines
- improving IMU feature extraction
- using BLE or Wi-Fi instead of infrared
- designing a more stable enclosure
- adding configurable thresholds
- improving receiver-side feedback
