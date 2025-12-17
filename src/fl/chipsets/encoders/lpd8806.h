#pragma once

/// @file chipsets/encoders/lpd8806.h
/// @brief LPD8806 SPI chipset encoder
///
/// Free function encoder for LPD8806 chipsets.
///
/// Protocol:
/// - LED data: [G7][R7][B7] (3 bytes per LED, MSB always set, 7-bit color)
/// - Latch: ((num_leds * 3 + 63) / 64) bytes of 0x00
///
/// Color encoding:
/// - Each byte has MSB set (0x80)
/// - 7-bit color depth (bits 0-6)
/// - Order: GRB (not RGB!)

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/chipsets/encoders/encoder_constants.h"

namespace fl {

/// @brief Encode pixel data in LPD8806 format
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note LPD8806 uses GRB wire order: pixel[0]=Green, pixel[1]=Red, pixel[2]=Blue
template <typename InputIterator, typename OutputIterator>
void encodeLPD8806(InputIterator first, InputIterator last, OutputIterator out) {
    // LED data: GRB with MSB set (count as we go)
    size_t num_leds = 0;
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;

        // LPD8806 requires MSB set on each byte, and uses 7-bit color depth
        // GRB order: 0=G, 1=R, 2=B
        *out++ = lpd8806Encode(pixel[0]);  // Index 0 (GRB order: Green)
        *out++ = lpd8806Encode(pixel[1]);  // Index 1 (GRB order: Red)
        *out++ = lpd8806Encode(pixel[2]);  // Index 2 (GRB order: Blue)

        ++first;
        ++num_leds;
    }

    // Latch: ((num_leds * 3 + 63) / 64) bytes of 0x00
    size_t latch_bytes = (num_leds * 3 + 63) / 64;
    for (size_t i = 0; i < latch_bytes; i++) {
        *out++ = 0x00;
    }
}

} // namespace fl
