#pragma once

/// @file chipsets/encoders/ws2801.h
/// @brief WS2801/WS2803 SPI chipset encoder
///
/// Free function encoder for WS2801 and WS2803 chipsets.
/// These chipsets use simple RGB byte streaming with timing-based latching.
///
/// Protocol:
/// - LED data: 3 bytes per LED (RGB order)
/// - No frame overhead (latch is timing-based, not data-based)
/// - Clock speed: typically 1-25 MHz
///
/// @note This consolidates encoding logic previously duplicated in:
///       - src/fl/chipsets/ws2801.h (WS2801Controller template)
///       - src/fl/chipsets/encoders/pixel_iterator.h (PixelIterator::writeWS2801 method)

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_constants.h"

namespace fl {

/// @brief Encode pixel data in WS2801/WS2803 format
///
/// Writes RGB pixel data in WS2801 protocol format.
/// WS2801 is one of the simplest SPI protocols - just 3 bytes per LED (RGB)
/// with no start/end frames. Latching occurs via timing (pause in clock).
///
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t (e.g., fl::back_inserter)
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
///
/// @note WS2803 uses identical protocol to WS2801 (just different default clock speed)
/// @note Writes 3 bytes per pixel in wire order
/// @note WS2801 uses RGB wire order: pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue
template <typename InputIterator, typename OutputIterator>
void encodeWS2801(InputIterator first, InputIterator last, OutputIterator out) {
    while (first != last) {
        const array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        *out++ = pixel[0];  // Index 0 (RGB order: Red)
        *out++ = pixel[1];  // Index 1 (RGB order: Green)
        *out++ = pixel[2];  // Index 2 (RGB order: Blue)
        ++first;
    }
    // No end frame needed - WS2801 latches via timing (clock pause)
}

} // namespace fl
