// Host-side unit tests for the wand's decision logic and the shared headers.
// No Arduino, no hardware: build and run with `make -C tests`.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "gesture_logic.h"
#include "ir_codes.h"
#include "timing.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                    \
    } while (0)

using ir::Command;
using wand::CommandStateMachine;
using wand::Gesture;

// --- ir_codes.h -------------------------------------------------------------

static void test_every_command_survives_the_bit_order_round_trip() {
    for (uint8_t id = 1; id <= 6; ++id) {
        const Command sent = static_cast<Command>(id);
        // What IRremote's decodedRawData holds on the receiver for this frame.
        const uint32_t received_raw = ir::reverse_bits32(ir::encode(sent));
        CHECK(ir::decode_lsb_first(received_raw) == sent);
    }
}

static void test_matches_raw_values_logged_on_the_prototype() {
    // The original receiver hard-coded these LSB-first values for the light.
    CHECK(ir::reverse_bits32(ir::encode(Command::kLightOn)) == 0x80000055UL);
    CHECK(ir::reverse_bits32(ir::encode(Command::kLightOff)) == 0x40000055UL);
}

static void test_foreign_and_repeat_frames_are_ignored() {
    CHECK(ir::decode_lsb_first(0x00000000UL) == Command::kNone);  // NEC repeat
    CHECK(ir::decode_lsb_first(ir::reverse_bits32(0xAA000000UL)) == Command::kNone);
    CHECK(ir::decode_lsb_first(ir::reverse_bits32(0xAA000007UL)) == Command::kNone);
    CHECK(ir::decode_lsb_first(ir::reverse_bits32(0xBB000001UL)) == Command::kNone);
    // A TV remote style NEC frame
    CHECK(ir::decode_lsb_first(0xF708FB04UL) == Command::kNone);
}

static void test_reverse_bits_is_its_own_inverse() {
    const uint32_t samples[] = {0x1UL, 0x80000000UL, 0xDEADBEEFUL, 0x12345678UL};
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        CHECK(ir::reverse_bits32(ir::reverse_bits32(samples[i])) == samples[i]);
    }
    CHECK(ir::reverse_bits32(0x1UL) == 0x80000000UL);
}

// --- timing.h ---------------------------------------------------------------

static void test_timeouts_survive_counter_wrap() {
    // micros() wraps every 2^32 us (~71.6 min). Start 2 ms before the wrap.
    const uint32_t start = 0xFFFFFFFFUL - 1999UL;
    CHECK(!timing::has_elapsed(start, start + 500u, 10000UL));
    const uint32_t after_wrap = start + 10000u;  // wraps to a small value
    CHECK(after_wrap < start);
    CHECK(timing::has_elapsed(start, after_wrap, 10000UL));
    CHECK(timing::elapsed(start, after_wrap) == 10000UL);
}

static void test_old_absolute_deadline_style_fails_at_wrap() {
    // Documents the original bug: a 64-bit deadline vs a 32-bit micros().
    const uint32_t micros_now = 0xFFFFFFFFUL - 1000UL;
    const uint64_t deadline = static_cast<uint64_t>(micros_now) + 10000ULL;
    const uint32_t micros_later = micros_now + 20000u;  // wrapped, period passed
    const bool old_loop_still_waiting = micros_later < deadline;
    CHECK(old_loop_still_waiting);  // old code would spin for ~71 minutes
    CHECK(timing::has_elapsed(micros_now, micros_later, 10000UL));  // fixed
}

// --- gesture_logic.h: features ---------------------------------------------

static void fill_frames(float* window, int frames, float ax, float ay, float az) {
    for (int f = 0; f < frames; ++f) {
        float* v = &window[f * wand::kAxesPerFrame];
        v[0] = ax; v[1] = ay; v[2] = az; v[3] = 0.0f; v[4] = 0.0f; v[5] = 0.0f;
    }
}

static void test_still_wand_does_not_wake() {
    float window[100 * wand::kAxesPerFrame];
    fill_frames(window, 100, 0.0f, 0.0f, wand::kGravity);
    const wand::WindowFeatures f = wand::extract_features(window, 600, 0.01f);
    CHECK(fabsf(f.max_accel - wand::kGravity) < 1e-4f);
    CHECK(f.estimated_distance == 0.0f);
    CHECK(!wand::should_wake(f));
}

static void test_strong_motion_wakes_and_moves() {
    float window[100 * wand::kAxesPerFrame];
    fill_frames(window, 100, 0.0f, 0.0f, wand::kGravity);
    fill_frames(window, 20, 0.0f, 14.0f, wand::kGravity);  // first 0.2 s: hard push on Y
    const wand::WindowFeatures f = wand::extract_features(window, 600, 0.01f);
    CHECK(wand::should_wake(f));
    CHECK(f.max_abs_y >= 14.0f);
    CHECK(f.estimated_distance > 0.0f);
}

// --- gesture_logic.h: selection ---------------------------------------------

static wand::WindowFeatures big_motion() {
    wand::WindowFeatures f = {20.0f, 10.0f, 10.0f};
    return f;
}

static void test_selects_confident_gesture() {
    const float scores[5] = {0.05f, 0.90f, 0.02f, 0.02f, 0.01f};
    CHECK(wand::select_gesture(scores, big_motion()) == wand::kCircle);
}

static void test_rejects_low_confidence() {
    const float scores[5] = {0.10f, 0.79f, 0.05f, 0.05f, 0.01f};
    CHECK(wand::select_gesture(scores, big_motion()) == wand::kNoGesture);
}

static void test_rejects_when_noise_is_likely() {
    const float scores[5] = {0.0f, 0.95f, 0.0f, 0.0f, 0.75f};
    CHECK(wand::select_gesture(scores, big_motion()) == wand::kNoGesture);
}

static void test_physical_veto_for_thrust_and_swing() {
    const float thrust[5] = {0.0f, 0.0f, 0.95f, 0.0f, 0.0f};
    wand::WindowFeatures weak_y = {20.0f, 4.9f, 10.0f};
    CHECK(wand::select_gesture(thrust, weak_y) == wand::kNoGesture);
    wand::WindowFeatures short_move = {20.0f, 10.0f, 3.9f};
    CHECK(wand::select_gesture(thrust, short_move) == wand::kNoGesture);
    CHECK(wand::select_gesture(thrust, big_motion()) == wand::kThrust);

    const float swing[5] = {0.0f, 0.0f, 0.0f, 0.95f, 0.0f};
    CHECK(wand::select_gesture(swing, short_move) == wand::kNoGesture);
    CHECK(wand::select_gesture(swing, big_motion()) == wand::kSwing);
}

// --- gesture_logic.h: command state machine ---------------------------------

static void test_plain_triggers_control_the_light() {
    CommandStateMachine fsm;
    CHECK(fsm.on_gesture(wand::kCircle, 1000) == Command::kLightOn);
    CHECK(fsm.on_gesture(wand::kZ, 2000) == Command::kLightOff);
}

static void test_prefix_then_trigger_gives_six_commands() {
    CommandStateMachine fsm;
    CHECK(fsm.on_gesture(wand::kSwing, 0) == Command::kNone);
    CHECK(fsm.on_gesture(wand::kCircle, 1000) == Command::kCurtainOpen);
    CHECK(fsm.on_gesture(wand::kSwing, 2000) == Command::kNone);
    CHECK(fsm.on_gesture(wand::kZ, 3000) == Command::kCurtainClose);
    CHECK(fsm.on_gesture(wand::kThrust, 4000) == Command::kNone);
    CHECK(fsm.on_gesture(wand::kCircle, 5000) == Command::kDoorOpen);
    CHECK(fsm.on_gesture(wand::kThrust, 6000) == Command::kNone);
    CHECK(fsm.on_gesture(wand::kZ, 7000) == Command::kDoorClose);
    CHECK(fsm.prefix() == CommandStateMachine::kNoPrefix);
}

static void test_prefix_expires_after_timeout() {
    CommandStateMachine fsm(5000);
    fsm.on_gesture(wand::kThrust, 1000);
    CHECK(!fsm.expire(6000));  // exactly 5000 ms: still valid
    CHECK(fsm.expire(6001));
    CHECK(fsm.on_gesture(wand::kCircle, 6002) == Command::kLightOn);
}

static void test_later_prefix_replaces_earlier_one() {
    CommandStateMachine fsm;
    fsm.on_gesture(wand::kThrust, 0);
    fsm.on_gesture(wand::kSwing, 100);
    CHECK(fsm.on_gesture(wand::kCircle, 200) == Command::kCurtainOpen);
}

static void test_prefix_timeout_across_millis_wrap() {
    CommandStateMachine fsm(5000);
    const uint32_t t0 = 0xFFFFFFFFUL - 1000UL;
    fsm.on_gesture(wand::kSwing, t0);
    CHECK(fsm.on_gesture(wand::kZ, t0 + 3000u) == Command::kCurtainClose);  // wrapped, 3 s
}

static void test_noise_gesture_never_sends() {
    CommandStateMachine fsm;
    CHECK(fsm.on_gesture(wand::kNoise, 0) == Command::kNone);
    CHECK(fsm.on_gesture(wand::kNoGesture, 0) == Command::kNone);
}

int main() {
    test_every_command_survives_the_bit_order_round_trip();
    test_matches_raw_values_logged_on_the_prototype();
    test_foreign_and_repeat_frames_are_ignored();
    test_reverse_bits_is_its_own_inverse();
    test_timeouts_survive_counter_wrap();
    test_old_absolute_deadline_style_fails_at_wrap();
    test_still_wand_does_not_wake();
    test_strong_motion_wakes_and_moves();
    test_selects_confident_gesture();
    test_rejects_low_confidence();
    test_rejects_when_noise_is_likely();
    test_physical_veto_for_thrust_and_swing();
    test_plain_triggers_control_the_light();
    test_prefix_then_trigger_gives_six_commands();
    test_prefix_expires_after_timeout();
    test_later_prefix_replaces_earlier_one();
    test_prefix_timeout_across_millis_wrap();
    test_noise_gesture_never_sends();

    printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
