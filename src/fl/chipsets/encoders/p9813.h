#pragma once

/// @file chipsets/encoders/p9813.h
/// @brief P9813 SPI chipset encoder
///
/// Free function encoder for P9813 chipsets.
///
/// Protocol:
/// - Start boundary: 4 bytes of 0x00
/// - LED data: [Flag][B][G][R] (4 bytes per LED)
/// - End boundary: 4 bytes of 0x00
///
/// Flag byte: 0xC0 | checksum
/// - Checksum uses top 2 bits of each RGB component
/// - Formula: (~B & 0xC0) >> 2 | (~G & 0xC0) >> 4 | (~R & 0xC0) >> 6

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/chipsets/encoders/encoder_constants.h"

namespace fl {

/// @brief Encode pixel data in P9813 format
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note P9813 uses BGR wire order: pixel[0]=Blue, pixel[1]=Green, pixel[2]=Red
template <typename InputIterator, typename OutputIterator>
void encodeP9813(InputIterator first, InputIterator last, OutputIterator out) {
    // Start boundary: 4 bytes of 0x00
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;

    // LED data: flag + BGR
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;

        // P9813 flag byte: 0xC0 | checksum (BGR order: 0=B, 1=G, 2=R)
        u8 flag = p9813FlagByte(pixel[2], pixel[1], pixel[0]);  // r, g, b

        *out++ = flag;
        *out++ = pixel[0];  // Index 0 (BGR order: Blue)
        *out++ = pixel[1];  // Index 1 (BGR order: Green)
        *out++ = pixel[2];  // Index 2 (BGR order: Red)

        ++first;
    }

    // End boundary: 4 bytes of 0x00
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
}

} // namespace fl
