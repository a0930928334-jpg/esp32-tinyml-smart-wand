# Limitations

## Data

The dataset is small and covers few users. The model would need more samples from more people, with different grips, speeds and gesture sizes, before it generalises.

## Recognition

- Recognition depends on how the wand is held and how large the gesture is.
- Some movements are misread as a forward thrust. The physical check reduces this but does not remove it.
- The "estimated distance" used in the physical check is a rough score from double-integrating acceleration. It drifts and is not a real displacement measurement.

## Infrared

- The wand must point roughly at the receiver.
- There is no acknowledgement: the wand cannot know whether a command arrived, so it sends each one six times and the receiver ignores the repeats.
- Sending blocks the wand for about 2 seconds, during which it does not sample.

## Scope

The receiver drives an LED and two servos on a model house. There is no network connection, app, or device-state feedback.

## What I would do next

1. Record data from more people and retrain, then measure accuracy per person.
2. Replace the forward-thrust physical check with features chosen from recorded data, and compare the classifier against a simple rule-based baseline.
3. Port the wand to an Arm Cortex-M board and compare latency and memory with the ESP32-S3, with int8 and float32 models.
4. Replace infrared with BLE so the receiver can acknowledge commands.
