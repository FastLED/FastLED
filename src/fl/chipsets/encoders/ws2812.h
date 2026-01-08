#pragma once

/// @file chipsets/encoders/ws2812.h
/// @brief WS2812/WS2812B/WS2813/NeoPixel encoder
///
/// Free function encoder for WS2812 family chipsets.
/// Supports both RGB (3 bytes/LED) and RGBW (4 bytes/LED) modes.
///
/// Protocol:
/// - RGB mode: 3 bytes per LED (R, G, B)
/// - RGBW mode: 4 bytes per LED (R, G, B, W)
/// - No frame overhead (timing-based protocol)
/// - Uses PWM timing on a single data line (not SPI)
///
/// @note This is a simple byte-streaming protocol - actual timing
///       is handled by the chipset driver (RMT, SPI emulation, bitbang, etc.)

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_constants.h"
#include "fl/rgbw.h"

namespace fl {

/// @brief Encode 3-byte pixel data in WS2812 format
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t (e.g., fl::back_inserter)
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note Writes 3 bytes per pixel in whatever color order they're already in
template <typename InputIterator, typename OutputIterator>
void encodeWS2812_RGB(InputIterator first, InputIterator last, OutputIterator out) {
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        *out++ = pixel[0];  // First byte
        *out++ = pixel[1];  // Second byte
        *out++ = pixel[2];  // Third byte
        ++first;
    }
}

/// @brief Encode 4-byte pixel data in WS2812 format
/// @tparam InputIterator Iterator yielding fl::array<u8, 4> (4 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t (e.g., fl::back_inserter)
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note Writes 4 bytes per pixel in whatever color order they're already in
template <typename InputIterator, typename OutputIterator>
void encodeWS2812_RGBW(InputIterator first, InputIterator last, OutputIterator out) {
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGBW>& pixel = *first;
        *out++ = pixel[0];  // First byte
        *out++ = pixel[1];  // Second byte
        *out++ = pixel[2];  // Third byte
        *out++ = pixel[3];  // Fourth byte
        ++first;
    }
}

/// @brief Encode pixel data in WS2812 format (automatic RGB/RGBW selection)
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> or fl::array<u8, 4>
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @param rgbw Rgbw configuration (if active, uses RGBW mode)
/// @note Dispatches to RGB or RGBW encoder based on rgbw.active()
template <typename InputIterator, typename OutputIterator>
void encodeWS2812(InputIterator first, InputIterator last, OutputIterator out, const Rgbw& rgbw) {
    if (rgbw.active()) {
        encodeWS2812_RGBW(first, last, out);
    } else {
        encodeWS2812_RGB(first, last, out);
    }
}

} // namespace fl
