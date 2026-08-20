/// @file fl/chipsets/encoders/tm1812.h
/// @brief TM1812 12-channel encoder for two RGBWW pixels per IC.

#pragma once

#include "fl/chipsets/encoders/encoder_constants.h"
#include "fl/stl/array.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

/// @brief Pack 5-channel RGBCCT pixels into TM1812 12-channel frames.
///
/// The TM1812 consumes four RGB groups (12 bytes / 96 bits) per IC. RGBCCT
/// strips reported in issue #995 wire two logical 5-channel pixels to the
/// first ten outputs and leave the final two outputs unused. Consequently,
/// each IC frame is `pixel 0 (5) + pixel 1 (5) + zero padding (2)`.
///
/// An odd final logical pixel is followed by five zero bytes for the absent
/// second pixel, then the normal two padding bytes. This preserves the 96-bit
/// boundary so downstream TM1812 chips stay aligned.
template <typename InputIterator, typename OutputIterator>
void encodeTM1812_RGBWW(InputIterator first, InputIterator last,
                        OutputIterator out) FL_NO_EXCEPT {
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGBWW>& firstPixel = *first;
        for (u8 byte : firstPixel) {
            *out++ = byte;
        }
        ++first;

        if (first != last) {
            const fl::array<u8, BYTES_PER_PIXEL_RGBWW>& secondPixel = *first;
            for (u8 byte : secondPixel) {
                *out++ = byte;
            }
            ++first;
        } else {
            for (u8 i = 0; i < BYTES_PER_PIXEL_RGBWW; ++i) {
                *out++ = 0;
            }
        }

        *out++ = 0;
        *out++ = 0;
    }
}

} // namespace fl
