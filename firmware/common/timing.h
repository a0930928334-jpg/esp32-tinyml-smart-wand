// Wrap-safe timing helpers for Arduino's millis() and micros().
//
// On both targets millis() and micros() return a 32-bit unsigned value that
// wraps to zero: micros() every 2^32 us (about 71.6 minutes), millis() every
// 2^32 ms (about 49.7 days). Comparing absolute deadlines such as
// `micros() < start + period` breaks at the wrap. Unsigned subtraction
// `now - start` stays correct across one wrap, so every timeout in the
// firmware goes through this helper. See docs/engineering-notes.md, issue 2.

#ifndef SMART_WAND_TIMING_H
#define SMART_WAND_TIMING_H

#include <stdint.h>

namespace timing {

inline uint32_t elapsed(uint32_t start, uint32_t now) {
    return static_cast<uint32_t>(now - start);
}

inline bool has_elapsed(uint32_t start, uint32_t now, uint32_t period) {
    return elapsed(start, now) >= period;
}

}  // namespace timing

#endif  // SMART_WAND_TIMING_H
