#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"

namespace fl {
namespace platforms {
namespace teensy {

/// Compile-time routing for the SPI pin pairs exposed by Teensyduino.
template <fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, bool IS_TEENSY_41>
struct Teensy4SpiPinRoute {
    enum {
        is_spi = (DATA_PIN == 11 || DATA_PIN == 12) && CLOCK_PIN == 13,
        is_spi1 = (DATA_PIN == 26 || DATA_PIN == 1) && CLOCK_PIN == 27,
        is_spi2_40 = !IS_TEENSY_41 && (DATA_PIN == 35 || DATA_PIN == 34) &&
                     CLOCK_PIN == 37,
        is_spi2_41 = IS_TEENSY_41 && (DATA_PIN == 43 || DATA_PIN == 42) &&
                     CLOCK_PIN == 45,
        is_hardware = is_spi || is_spi1 || is_spi2_40 || is_spi2_41,
        bus_index = is_spi ? 0 : (is_spi1 ? 1 : (is_spi2_40 || is_spi2_41 ? 2 : -1))
    };
};

} // namespace teensy
} // namespace platforms
} // namespace fl
