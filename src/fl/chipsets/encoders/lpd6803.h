#pragma once

/// @file chipsets/encoders/lpd6803.h
/// @brief LPD6803 SPI chipset encoder
///
/// Free function encoder for LPD6803 chipsets.
///
/// Protocol:
/// - Start boundary: 4 bytes of 0x00
/// - LED data: 16-bit per LED (1 marker + 5-5-5 RGB)
/// - End boundary: (num_leds / 32) DWords of 0xFF000000
///
/// 16-bit format: 1bbbbbgggggrrrrr
/// - Bit 15: start marker (1)
/// - Bits 14-10: Red (5 bits, high bits of 8-bit value)
/// - Bits 9-5: Green (5 bits, high bits of 8-bit value)
/// - Bits 4-0: Blue (5 bits, high bits of 8-bit value)

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/chipsets/encoders/encoder_constants.h"

namespace fl {

/// @brief Encode pixel data in LPD6803 format
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note LPD6803 uses RGB wire order: pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue
template <typename InputIterator, typename OutputIterator>
void encodeLPD6803(InputIterator first, InputIterator last, OutputIterator out) {
    // Start boundary: 4 bytes of 0x00
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;

    // LED data: 16-bit format (1bbbbbgggggrrrrr, count as we go)
    size_t num_leds = 0;
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;

        // RGB order: 0=R, 1=G, 2=B
        u16 command = lpd6803EncodeRGB(pixel[0], pixel[1], pixel[2]);

        *out++ = static_cast<u8>(command >> 8);   // MSB
        *out++ = static_cast<u8>(command & 0xFF); // LSB

        ++first;
        ++num_leds;
    }

    // End boundary: (num_leds / 32) DWords of 0xFF000000
    size_t end_dwords = (num_leds / 32);
    for (size_t i = 0; i < end_dwords; i++) {
        *out++ = 0xFF;
        *out++ = 0x00;
        *out++ = 0x00;
        *out++ = 0x00;
    }
}

} // namespace fl
