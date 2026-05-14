# Testing

## Overview

The prototype was evaluated through both platform validation and physical prototype testing.

A key boundary of this project is that Edge Impulse validation metrics and real-world physical prototype results are not the same thing.

## Test Categories

The project includes the following test categories:

1. Edge Impulse validation
2. ESP32 on-device inference check
3. Physical single-gesture recognition tests
4. Combined command tests
5. Infrared control tests
6. Continuous operation and robustness tests

## Platform Validation Results

| Metric | Result | Notes |
|---|---:|---|
| Edge Impulse validation accuracy | 95.8% | Validation-set result, not real-world accuracy |
| Validation loss | ~0.11 | Edge Impulse validation result |
| Weighted precision / recall / F1 | ~0.96 | Edge Impulse validation result |
| On-device inference time | ~1 ms | ESP32-side deployment estimate |
| Peak RAM usage | ~1.6 KB | Model deployment estimate |
| Flash usage | ~16.9 KB | Model deployment estimate |

## Physical Prototype Test Results

| Test Item | Result | Notes |
|---|---:|---|
| Physical single-gesture recognition rate | ~90.0% | Prototype-level test result |
| Combined command success rate | ~85.0% | Prototype-level test result |
| Infrared control success rate | ~89.0% | Prototype-level test result |

## Command Testing

The finite-state command logic supports six simulated commands:

| Prefix Gesture | Trigger Gesture | Simulated Command |
|---|---|---|
| None | Circle | Light on |
| None | Z | Light off |
| Upward swing | Circle | Curtain open |
| Upward swing | Z | Curtain close |
| Forward thrust | Circle | Door open |
| Forward thrust | Z | Door close |

## Observed Issues

During physical testing, the following issues were observed:

- Some gestures are sensitive to user motion style.
- Weak or incomplete gestures may not pass physical wake-up or secondary validation.
- Some movements may be misclassified as forward thrust.
- Infrared directionality affects receiver success.
- Servo response depends on power supply and mechanical setup.
- Real-world physical results are lower than validation-set metrics.

## Testing Boundary

These results are prototype-level results. They are useful for demonstrating the system workflow, but they should not be treated as production reliability metrics.
