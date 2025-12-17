#pragma once

/// @file chipsets/encoders/ws2803.h
/// @brief WS2803 SPI chipset encoder (WS2801 alias)
///
/// WS2803 uses identical protocol to WS2801 - just simple RGB byte streaming.
/// The only difference is the default clock speed (typically higher for WS2803).
///
/// Protocol:
/// - LED data: 3 bytes per LED (RGB order)
/// - No frame overhead (latch is timing-based, not data-based)
/// - Clock speed: typically higher than WS2801
///
/// @note This is just a wrapper around the WS2801 encoder

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"

namespace fl {

// Forward declaration of WS2801 encoder
template <typename InputIterator, typename OutputIterator>
void encodeWS2801(InputIterator first, InputIterator last, OutputIterator out);

/// @brief Encode pixel data in WS2803 format (alias for WS2801)
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note WS2803 protocol is identical to WS2801
template <typename InputIterator, typename OutputIterator>
void encodeWS2803(InputIterator first, InputIterator last, OutputIterator out) {
    // WS2803 uses identical protocol to WS2801
    encodeWS2801(first, last, out);
}

} // namespace fl
