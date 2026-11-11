// Shared infrared command table for the wand (sender) and the receiver.
//
// Both firmwares include this header so the two sides can never disagree on
// what a code means. Plain C++11 and <stdint.h> only, so it builds with
// avr-gcc (Nano), xtensa-gcc (ESP32-S3) and a desktop compiler for host tests.
//
// Bit order: the wand sends with IRremoteESP8266, whose sendNEC() takes the
// 32-bit value most-significant bit first. The receiver uses IRremote, whose
// decodedRawData stores the same frame least-significant bit first. The
// receiver therefore reverses the bits before looking a code up.
// See docs/engineering-notes.md, issue 1.

#ifndef SMART_WAND_IR_CODES_H
#define SMART_WAND_IR_CODES_H

#include <stdint.h>

namespace ir {

enum class Command : uint8_t {
    kNone = 0,
    kLightOn = 1,
    kLightOff = 2,
    kCurtainOpen = 3,
    kCurtainClose = 4,
    kDoorOpen = 5,
    kDoorClose = 6,
};

// Upper 24 bits mark a frame as ours; the low byte carries the command.
const uint32_t kCodePrefix = 0xAA000000UL;
const uint32_t kPrefixMask = 0xFFFFFF00UL;
const uint8_t kNecBits = 32;

// Value passed to IRsend::sendNEC() on the wand.
inline uint32_t encode(Command command) {
    return kCodePrefix | static_cast<uint32_t>(command);
}

inline uint32_t reverse_bits32(uint32_t value) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < 32; ++i) {
        result = (result << 1) | (value & 1UL);
        value >>= 1;
    }
    return result;
}

// Turns IRremote's decodedRawData (LSB-first) back into a command.
// Returns Command::kNone for frames that are not ours.
inline Command decode_lsb_first(uint32_t raw_lsb_first) {
    const uint32_t value = reverse_bits32(raw_lsb_first);
    if ((value & kPrefixMask) != kCodePrefix) {
        return Command::kNone;
    }
    const uint8_t id = static_cast<uint8_t>(value & 0xFFUL);
    if (id < static_cast<uint8_t>(Command::kLightOn) ||
        id > static_cast<uint8_t>(Command::kDoorClose)) {
        return Command::kNone;
    }
    return static_cast<Command>(id);
}

inline const char* name(Command command) {
    switch (command) {
        case Command::kLightOn:      return "light on";
        case Command::kLightOff:     return "light off";
        case Command::kCurtainOpen:  return "curtain open";
        case Command::kCurtainClose: return "curtain close";
        case Command::kDoorOpen:     return "door open";
        case Command::kDoorClose:    return "door close";
        case Command::kNone:         break;
    }
    return "none";
}

}  // namespace ir

#endif  // SMART_WAND_IR_CODES_H
