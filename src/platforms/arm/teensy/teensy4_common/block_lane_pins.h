#pragma once

// IWYU pragma: private

/// @file block_lane_pins.h
/// @brief Platform-agnostic lane pin selection for the Teensy 4.x
/// FlexibleInlineBlockClocklessController (issue #4588).
///
/// Mirrors the historical `_BLOCK_PIN` switch fallthrough: starting at the
/// first pin, pins are taken in GPIO block order until the requested lane
/// count is reached or a block terminator pin (27, 7, 30) has been taken.
/// Only fl/stl headers are included so host tests can exercise it.

#include "fl/stl/stdint.h"

namespace fl {

constexpr u8 kTeensy4BlockPinOrder[] = {
    // GPIO6 block
    1, 0, 24, 25, 19, 18, 14, 15, 17, 16, 22, 23, 20, 21, 26, 27,
    // GPIO7 block
    10, 12, 11, 13, 6, 9, 32, 8, 7,
    // GPIO 37 block
    37, 36, 35, 34, 39, 38, 28, 31, 30,
};

constexpr i32 kTeensy4BlockPinCount =
    static_cast<i32>(sizeof(kTeensy4BlockPinOrder) / sizeof(kTeensy4BlockPinOrder[0]));

/// True when `pin` ends a GPIO block sequence (27, 7 or 30).
inline bool teensy4IsBlockTerminator(i32 pin);

/// Fill `outPins` with up to `lanes` pins starting at `firstPin`, stopping
/// after a block terminator. Returns the actual lane count, or 0 when
/// `firstPin` is not a block pin. `outPins` must hold at least `lanes` entries.
/// Defined in block_lane_pins.hpp.
inline u8 teensy4BlockLanePins(i32 firstPin, u8 lanes, u8* outPins);

}  // namespace fl
