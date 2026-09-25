#pragma once

// IWYU pragma: private

/// @file block_lane_pins.hpp
/// @brief Definitions for block_lane_pins.h (issue #4588).

#include "platforms/arm/teensy/teensy4_common/block_lane_pins.h"

namespace fl {

/// True when `pin` ends a GPIO block sequence.
inline bool teensy4IsBlockTerminator(i32 pin) {
    return pin == 27 || pin == 7 || pin == 30;
}

/// Fill `outPins` with up to `lanes` pins starting at `firstPin`, stopping
/// after a block terminator. Returns the actual lane count, or 0 when
/// `firstPin` is not a block pin. `outPins` must hold at least `lanes` entries.
inline u8 teensy4BlockLanePins(i32 firstPin, u8 lanes, u8* outPins) {
    i32 start = -1;
    for (i32 i = 0; i < kTeensy4BlockPinCount; ++i) {
        if (static_cast<i32>(kTeensy4BlockPinOrder[i]) == firstPin) {
            start = i;
            break;
        }
    }
    if (start < 0) {
        return 0;
    }
    u8 count = 0;
    for (i32 i = start; i < kTeensy4BlockPinCount && count < lanes; ++i) {
        const u8 pin = kTeensy4BlockPinOrder[i];
        outPins[count++] = pin;
        if (teensy4IsBlockTerminator(pin)) {
            break;
        }
    }
    return count;
}

}  // namespace fl
