#pragma once

/// @file chipsets/encoders/sm16716.h
/// @brief SM16716 SPI chipset encoder
///
/// Free function encoder for SM16716 chipsets.
///
/// Protocol:
/// - LED data: RGB bytes (3 bytes per LED)
/// - Header: 50 zero bits (7 bytes of 0x00)
/// - Start bit: Handled by SPI hardware layer (FLAG_START_BIT)
///
/// @note SM16716 requires a start bit before each RGB triplet,
///       typically handled by the SPI hardware layer, not the encoder.

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_constants.h"

namespace fl {

/// @brief Encode pixel data in SM16716 format
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note Start bit handling is assumed to be done by SPI hardware
/// @note SM16716 uses RGB wire order: pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue
template <typename InputIterator, typename OutputIterator>
void encodeSM16716(InputIterator first, InputIterator last, OutputIterator out) {
    // LED data: RGB bytes
    // (Start bit for each triplet is handled by SPI hardware layer)
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        *out++ = pixel[0];  // Index 0 (RGB order: Red)
        *out++ = pixel[1];  // Index 1 (RGB order: Green)
        *out++ = pixel[2];  // Index 2 (RGB order: Blue)
        ++first;
    }

    // Header: 50 zero bits (7 bytes of 0x00)
    for (int i = 0; i < 7; i++) {
        *out++ = 0x00;
    }
}

} // namespace fl
