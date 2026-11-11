// ESP32-S3 smart wand: IMU gesture recognition with on-device TinyML,
// sent to the receiver as NEC infrared commands.
//
// Pipeline (every 10 ms sample, model every 5th sample):
//   MPU6050 -> sliding window -> wake-up gate -> Edge Impulse classifier
//   -> confidence / noise / physical checks -> command state machine -> IR
//
// This file does the I/O. The decision logic is in gesture_logic.h and is
// unit-tested on the host (tests/).

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Arduino library exported from Edge Impulse (not committed; see README).
#include <SmartWand2_inferencing.h>

#include "gesture_logic.h"
#include "ir_codes.h"
#include "timing.h"

// ---------------------------------------------------------------------------
// Pins (ESP32-S3 DevKit). GPIO6 is free on the S3; on a classic ESP32 module
// GPIO6-11 are wired to the SPI flash and must not be used.
// ---------------------------------------------------------------------------
const uint8_t kStatusLedPin = 2;
const uint8_t kI2cSdaPin = 4;
const uint8_t kI2cSclPin = 5;
const uint8_t kIrLedPin = 6;
const uint8_t kMpuAddress = 0x68;

// ---------------------------------------------------------------------------
// Timing and robustness settings
// ---------------------------------------------------------------------------
const int kInferenceEveryN = 5;          // run the model every 5th sample (50 ms)
const uint32_t kCooldownMs = 1000;       // ignore new gestures for 1 s after one fires
const int kWarmupSamples = 50;           // discard first samples while the IMU settles
const int kIrRepeatCount = 6;            // send each command 6 times...
const uint32_t kIrRepeatIntervalMs = 400;  // ...400 ms apart (IR has no acknowledgement)
const uint32_t kPrefixTimeoutMs = 5000;  // a prefix gesture expires after 5 s
const uint32_t kDebugPrintIntervalMs = 1000;

const uint32_t kSamplePeriodUs = static_cast<uint32_t>(EI_CLASSIFIER_INTERVAL_MS * 1000.0f);
const float kSamplePeriodS = EI_CLASSIFIER_INTERVAL_MS / 1000.0f;
const int kWindowValues = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

static_assert(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE % wand::kAxesPerFrame == 0,
              "model input must be whole 6-axis frames");
static_assert(EI_CLASSIFIER_LABEL_COUNT == wand::kClassCount,
              "model must have the 5 classes listed in wand::kLabelNames");

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
Adafruit_MPU6050 mpu;
IRsend ir_sender(kIrLedPin);
wand::CommandStateMachine command_fsm(kPrefixTimeoutMs);

float window_values[kWindowValues] = {0};  // interleaved ax, ay, az, gx, gy, gz
int samples_since_inference = 0;
uint32_t last_trigger_ms = 0;
// model output index for each wand::Gesture, filled once in setup()
int model_index_of[wand::kClassCount];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
[[noreturn]] void halt(const char* message) {
    Serial.println(message);
    while (true) {
        digitalWrite(kStatusLedPin, !digitalRead(kStatusLedPin));
        delay(200);
    }
}

void read_frame(float* frame) {
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);
    frame[0] = accel.acceleration.x;
    frame[1] = accel.acceleration.y;
    frame[2] = accel.acceleration.z;
    frame[3] = gyro.gyro.x;
    frame[4] = gyro.gyro.y;
    frame[5] = gyro.gyro.z;
}

// Looks the model's classes up by label name instead of assuming an order.
void map_model_labels() {
    for (int g = 0; g < wand::kClassCount; ++g) {
        model_index_of[g] = -1;
        for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
            if (strcmp(ei_classifier_inferencing_categories[i], wand::kLabelNames[g]) == 0) {
                model_index_of[g] = i;
            }
        }
        if (model_index_of[g] < 0) {
            Serial.printf("Model has no label \"%s\"\n", wand::kLabelNames[g]);
            halt("Halted: model labels do not match the firmware.");
        }
    }
}

void send_command(ir::Command command) {
    const uint32_t code = ir::encode(command);
    digitalWrite(kStatusLedPin, HIGH);
    Serial.printf("Sending %s (0x%08lX)\n", ir::name(command), static_cast<unsigned long>(code));

    // Blocking by design: sampling pauses for ~2 s while the bursts go out.
    for (int i = 0; i < kIrRepeatCount; ++i) {
        ir_sender.sendNEC(code, ir::kNecBits);
        if (i + 1 < kIrRepeatCount) delay(kIrRepeatIntervalMs);
    }

    digitalWrite(kStatusLedPin, LOW);
    // Clear the window so the tail of this gesture cannot trigger again.
    memset(window_values, 0, sizeof(window_values));
    last_trigger_ms = millis();
}

// Runs once per kInferenceEveryN samples.
void process_window() {
    const wand::WindowFeatures features =
        wand::extract_features(window_values, kWindowValues, kSamplePeriodS);
    if (!wand::should_wake(features)) {
        return;
    }

    signal_t signal;
    numpy::signal_from_buffer(window_values, kWindowValues, &signal);
    ei_impulse_result_t result = {};
    const EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        Serial.printf("run_classifier failed (%d)\n", static_cast<int>(err));
        return;
    }

    float scores[wand::kClassCount];
    for (int g = 0; g < wand::kClassCount; ++g) {
        scores[g] = result.classification[model_index_of[g]].value;
    }

    static uint32_t last_debug_ms = 0;
    const uint32_t now = millis();
    if (timing::has_elapsed(last_debug_ms, now, kDebugPrintIntervalMs)) {
        last_debug_ms = now;
        Serial.printf("scores Z=%.2f circle=%.2f thrust=%.2f swing=%.2f noise=%.2f | "
                      "peak|a|=%.2f peak|ay|=%.2f dist=%.2f\n",
                      scores[wand::kZ], scores[wand::kCircle], scores[wand::kThrust],
                      scores[wand::kSwing], scores[wand::kNoise], features.max_accel,
                      features.max_abs_y, features.estimated_distance);
    }

    if (!timing::has_elapsed(last_trigger_ms, now, kCooldownMs + 1)) {
        return;
    }

    const wand::Gesture gesture = wand::select_gesture(scores, features);
    if (gesture == wand::kNoGesture) {
        return;
    }

    Serial.printf("Gesture: %s\n", wand::kLabelNames[gesture]);
    const ir::Command command = command_fsm.on_gesture(gesture, now);
    last_trigger_ms = now;
    if (command != ir::Command::kNone) {
        send_command(command);
    }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(kStatusLedPin, OUTPUT);
    digitalWrite(kStatusLedPin, LOW);
    ir_sender.begin();

    Wire.begin(kI2cSdaPin, kI2cSclPin);
    if (!mpu.begin(kMpuAddress)) {
        halt("MPU6050 not found: check wiring.");
    }
    // Must match the settings used when the training data was recorded.
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    map_model_labels();

    Serial.println("Warming up IMU...");
    float discard[wand::kAxesPerFrame];
    for (int i = 0; i < kWarmupSamples; ++i) {
        read_frame(discard);
        delay(EI_CLASSIFIER_INTERVAL_MS);
    }
    for (int i = 0; i < kWindowValues; i += wand::kAxesPerFrame) {
        read_frame(&window_values[i]);
        delay(EI_CLASSIFIER_INTERVAL_MS);
    }
    Serial.println("Ready.");
}

void loop() {
    const uint32_t tick_start_us = micros();

    if (command_fsm.expire(millis())) {
        Serial.println("Prefix expired.");
    }

    // Slide the window by one frame and append the newest sample.
    memmove(window_values, window_values + wand::kAxesPerFrame,
            (kWindowValues - wand::kAxesPerFrame) * sizeof(float));
    read_frame(&window_values[kWindowValues - wand::kAxesPerFrame]);

    if (++samples_since_inference >= kInferenceEveryN) {
        samples_since_inference = 0;
        process_window();
    }

    // Hold a fixed sample period. Wrap-safe: see timing.h.
    while (!timing::has_elapsed(tick_start_us, micros(), kSamplePeriodUs)) {
    }
}
