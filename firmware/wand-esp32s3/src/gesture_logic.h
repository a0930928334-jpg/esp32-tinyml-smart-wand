// Platform-independent decision logic for the smart wand.
//
// Everything that turns sensor windows and model scores into a command lives
// here, with no Arduino or Edge Impulse dependency, so it can be unit-tested
// on a desktop compiler (see tests/). main.cpp only does I/O: read the IMU,
// run the classifier, send infrared.

#ifndef SMART_WAND_GESTURE_LOGIC_H
#define SMART_WAND_GESTURE_LOGIC_H

#include <math.h>
#include <stdint.h>

#include "ir_codes.h"
#include "timing.h"

namespace wand {

// Gesture classes. The model's label names are listed in kLabelNames in the
// same order; main.cpp looks scores up by name, so the model's own output
// order does not matter.
enum Gesture : int8_t {
    kNoGesture = -1,
    kZ = 0,         // Z-shaped stroke: trigger for "off / close"
    kCircle = 1,    // circle: trigger for "on / open"
    kThrust = 2,    // forward thrust: prefix selecting the door
    kSwing = 3,     // upward swing: prefix selecting the curtain
    kNoise = 4,     // anything else; never triggers an action
};

const int kClassCount = 5;
const char* const kLabelNames[kClassCount] = {
    "Z", "HUAQUAN", "QIANCI", "SHANGTIAO", "ZAOSHENG",
};

const int kAxesPerFrame = 6;  // ax, ay, az (m/s^2), gx, gy, gz (rad/s)
const float kGravity = 9.81f;

struct Thresholds {
    float trigger_confidence = 0.80f;  // best class must score above this
    float noise_suppress = 0.75f;      // ignore the window if noise >= this
    float wake_accel = 13.0f;          // peak |a| (m/s^2) needed to run the model
    float thrust_min_abs_y = 5.0f;     // thrust must show a strong Y-axis peak
    float thrust_min_distance = 4.0f;  // and enough estimated movement
    float swing_min_distance = 4.0f;   // swing must show enough estimated movement
    // A separate "swing force >= 8 m/s^2" check was removed: wake_accel (13)
    // already guarantees it, so it could never reject anything.
    // See docs/engineering-notes.md, issue 3.
};

struct WindowFeatures {
    float max_accel;           // peak acceleration magnitude in the window
    float max_abs_y;           // peak |ay|
    float estimated_distance;  // heuristic only, see note below
};

// Simple physical features over a window of interleaved 6-axis frames.
// estimated_distance double-integrates (|a| - g) with a 2 m/s^2 dead band.
// It drifts and has no real unit; it is only a "did the wand travel" score,
// not a displacement measurement.
inline WindowFeatures extract_features(const float* window, int value_count, float dt_s) {
    WindowFeatures f = {0.0f, 0.0f, 0.0f};
    float velocity = 0.0f;
    for (int i = 0; i + kAxesPerFrame <= value_count; i += kAxesPerFrame) {
        const float ax = window[i + 0];
        const float ay = window[i + 1];
        const float az = window[i + 2];
        const float accel = sqrtf(ax * ax + ay * ay + az * az);
        if (accel > f.max_accel) f.max_accel = accel;
        if (fabsf(ay) > f.max_abs_y) f.max_abs_y = fabsf(ay);

        float dynamic = accel - kGravity;
        if (fabsf(dynamic) < 2.0f) dynamic = 0.0f;
        velocity += dynamic * dt_s;
        f.estimated_distance += fabsf(velocity) * dt_s;
    }
    return f;
}

// Cheap gate run before the classifier: skip inference for small movements.
inline bool should_wake(const WindowFeatures& f, const Thresholds& t = Thresholds()) {
    return f.max_accel >= t.wake_accel;
}

// Picks a gesture from model scores, or kNoGesture.
// scores[] is indexed by Gesture (kZ..kNoise).
inline Gesture select_gesture(const float scores[kClassCount], const WindowFeatures& f,
                              const Thresholds& t = Thresholds()) {
    if (scores[kNoise] >= t.noise_suppress) {
        return kNoGesture;
    }

    Gesture best = kNoGesture;
    float best_score = t.trigger_confidence;
    for (int g = kZ; g <= kSwing; ++g) {
        if (scores[g] > best_score) {
            best_score = scores[g];
            best = static_cast<Gesture>(g);
        }
    }

    // Physical veto: the model sometimes labels weak or sideways motion as a
    // thrust or swing (shortcut learning). Require the motion to look right.
    if (best == kThrust &&
        (f.max_abs_y < t.thrust_min_abs_y || f.estimated_distance < t.thrust_min_distance)) {
        return kNoGesture;
    }
    if (best == kSwing && f.estimated_distance < t.swing_min_distance) {
        return kNoGesture;
    }
    return best;
}

// Two-step command grammar:
//   circle            -> light on        Z            -> light off
//   swing, circle     -> curtain open    swing, Z     -> curtain close
//   thrust, circle    -> door open       thrust, Z    -> door close
// A prefix (swing / thrust) is forgotten after prefix_timeout_ms.
class CommandStateMachine {
public:
    enum Prefix : uint8_t { kNoPrefix, kSwingPrefix, kThrustPrefix };

    explicit CommandStateMachine(uint32_t prefix_timeout_ms = 5000)
        : timeout_ms_(prefix_timeout_ms), prefix_(kNoPrefix), prefix_start_ms_(0) {}

    // Call regularly; returns true if a pending prefix just expired.
    bool expire(uint32_t now_ms) {
        if (prefix_ != kNoPrefix &&
            timing::elapsed(prefix_start_ms_, now_ms) > timeout_ms_) {
            prefix_ = kNoPrefix;
            return true;
        }
        return false;
    }

    // Feeds one accepted gesture. Returns the command to send, or
    // Command::kNone when the gesture only set a prefix.
    ir::Command on_gesture(Gesture gesture, uint32_t now_ms) {
        expire(now_ms);
        switch (gesture) {
            case kSwing:
                set_prefix(kSwingPrefix, now_ms);
                return ir::Command::kNone;
            case kThrust:
                set_prefix(kThrustPrefix, now_ms);
                return ir::Command::kNone;
            case kCircle:
                return take(ir::Command::kCurtainOpen, ir::Command::kDoorOpen,
                            ir::Command::kLightOn);
            case kZ:
                return take(ir::Command::kCurtainClose, ir::Command::kDoorClose,
                            ir::Command::kLightOff);
            default:
                return ir::Command::kNone;
        }
    }

    Prefix prefix() const { return prefix_; }

private:
    void set_prefix(Prefix p, uint32_t now_ms) {
        prefix_ = p;
        prefix_start_ms_ = now_ms;
    }

    ir::Command take(ir::Command with_swing, ir::Command with_thrust, ir::Command plain) {
        const Prefix p = prefix_;
        prefix_ = kNoPrefix;
        if (p == kSwingPrefix) return with_swing;
        if (p == kThrustPrefix) return with_thrust;
        return plain;
    }

    uint32_t timeout_ms_;
    Prefix prefix_;
    uint32_t prefix_start_ms_;
};

}  // namespace wand

#endif  // SMART_WAND_GESTURE_LOGIC_H
