// Arduino Nano receiver: decodes the wand's NEC infrared commands and drives
// a model house (LED "light", continuous-rotation servos for door and curtain).
//
// Wiring:
//   IR receiver OUT -> D2        Door servo signal    -> D9
//   LED (light)     -> D13       Curtain servo signal -> D10
//   Both servos are powered from a separate 5 V supply whose GND is joined to
//   the Nano's GND.

#include <Arduino.h>
#include <IRremote.hpp>
#include <Servo.h>

#include "ir_codes.h"
#include "timing.h"

const uint8_t kIrReceivePin = 2;
const uint8_t kLightLedPin = 13;
const uint8_t kDoorServoPin = 9;
const uint8_t kCurtainServoPin = 10;

// 360-degree servos: write() sets speed and direction (0 / 180 = full speed
// either way, 90 = stop), not an angle, so movements are timed. The
// directions below reproduce the behaviour verified on the prototype.
const int kDoorOpenSpeed = 0;
const int kDoorCloseSpeed = 180;
const uint32_t kDoorRunMs = 100;
const int kCurtainOpenSpeed = 0;
const int kCurtainCloseSpeed = 180;
const uint32_t kCurtainRunMs = 1000;

// The wand repeats each command 6 times over ~2 s; act on it only once.
const uint32_t kDuplicateWindowMs = 2000;

// A continuous servo that runs for a fixed time, then is detached so it
// cannot creep. Non-blocking: call update() every loop.
class TimedServo {
public:
    TimedServo(uint8_t pin, uint32_t run_ms, const char* name)
        : pin_(pin), run_ms_(run_ms), name_(name), busy_(false), started_ms_(0) {}

    void start(int speed, const char* action) {
        if (busy_) {
            Serial.print(name_);
            Serial.println(" busy, command ignored");
            return;
        }
        if (!servo_.attached()) servo_.attach(pin_);
        servo_.write(speed);
        busy_ = true;
        started_ms_ = millis();
        Serial.print(action);
        Serial.print(" (");
        Serial.print(run_ms_);
        Serial.println(" ms)");
    }

    void update(uint32_t now_ms) {
        if (busy_ && timing::has_elapsed(started_ms_, now_ms, run_ms_)) {
            servo_.detach();
            busy_ = false;
            Serial.print(name_);
            Serial.println(" stopped");
        }
    }

private:
    Servo servo_;
    uint8_t pin_;
    uint32_t run_ms_;
    const char* name_;
    bool busy_;
    uint32_t started_ms_;
};

TimedServo door(kDoorServoPin, kDoorRunMs, "door");
TimedServo curtain(kCurtainServoPin, kCurtainRunMs, "curtain");

ir::Command last_command = ir::Command::kNone;
uint32_t last_command_ms = 0;

void execute(ir::Command command) {
    switch (command) {
        case ir::Command::kLightOn:      digitalWrite(kLightLedPin, HIGH); break;
        case ir::Command::kLightOff:     digitalWrite(kLightLedPin, LOW); break;
        case ir::Command::kCurtainOpen:  curtain.start(kCurtainOpenSpeed, "curtain opening"); break;
        case ir::Command::kCurtainClose: curtain.start(kCurtainCloseSpeed, "curtain closing"); break;
        case ir::Command::kDoorOpen:     door.start(kDoorOpenSpeed, "door opening"); break;
        case ir::Command::kDoorClose:    door.start(kDoorCloseSpeed, "door closing"); break;
        case ir::Command::kNone:         break;
    }
}

void setup() {
    Serial.begin(9600);
    pinMode(kLightLedPin, OUTPUT);
    digitalWrite(kLightLedPin, LOW);
    // Servos stay detached until a command arrives.
    IrReceiver.begin(kIrReceivePin, DISABLE_LED_FEEDBACK);
    Serial.println("Receiver ready.");
}

void loop() {
    const uint32_t now = millis();
    door.update(now);
    curtain.update(now);

    if (!IrReceiver.decode()) {
        return;
    }
    const uint32_t raw = IrReceiver.decodedIRData.decodedRawData;  // LSB-first
    IrReceiver.resume();

    const ir::Command command = ir::decode_lsb_first(raw);
    if (command == ir::Command::kNone) {
        return;  // NEC repeat frame (raw 0) or a code that is not ours
    }

    const bool duplicate = command == last_command &&
                           !timing::has_elapsed(last_command_ms, now, kDuplicateWindowMs);
    if (duplicate) {
        return;
    }
    last_command = command;
    last_command_ms = now;

    Serial.print("Received: ");
    Serial.println(ir::name(command));
    execute(command);
}
