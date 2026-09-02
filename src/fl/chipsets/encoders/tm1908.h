/// @file tm1908.h
/// @brief TM1908 mode/current command prefix and RGB pixel encoder.

#pragma once

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

/// @brief Encode one complete TM1908 frame.
///
/// TM1908 V1.2 requires every frame to begin with the 48-bit normal-mode
/// command (0xFFFFFF followed by its inverse 0x000000), then a 24-bit current
/// command. The maximum in-spec 25 mA setting is 0x7F for each channel; bit 7
/// is reserved and must remain zero. Pixel PWM data follows in RGB wire order.
template <typename InputIterator, typename OutputIterator>
void encodeTM1908(InputIterator first, InputIterator last,
                  OutputIterator out) FL_NO_EXCEPT {
    const u8 prefix[] = {
        0xff, 0xff, 0xff, 0x00, 0x00, 0x00, // normal-mode command
        0x7f, 0x7f, 0x7f,                   // 25 mA R/G/B current
    };
    for (u8 byte : prefix) {
        *out++ = byte;
    }

    while (first != last) {
        const auto& pixel = *first;
        *out++ = pixel[0];
        *out++ = pixel[1];
        *out++ = pixel[2];
        ++first;
    }
}

} // namespace fl
