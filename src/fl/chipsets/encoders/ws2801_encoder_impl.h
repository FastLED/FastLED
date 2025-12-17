#pragma once

/// @file chipsets/encoders/ws2801_encoder_impl.h
/// @brief WS2801/WS2803 SPI chipset encoder (pixel iterator adapter)
///
/// DEPRECATED: This file is no longer used. The modern input/output iterator
/// version is in ws2801.h and is used throughout the codebase.
///
/// This file provided a direct PixelIterator interface but is now obsolete.
/// Use pixel_iterator_adapters.h to convert PixelIterator to input iterators,
/// then use the encoders in ws2801.h.
///
/// @note If you need this functionality, see PixelIterator::writeWS2801()
/// @note Or use: encodeWS2801(pixel_range.first, pixel_range.second, output_iterator)

#include "ws2801.h"
#include "fl/stl/stdint.h"

// No namespace declaration - this file is included inside the fl namespace

/// @brief Encode pixel data in WS2801/WS2803 format using PixelIterator
///
/// @deprecated Use the input iterator version in ws2801.h instead:
///   auto pixel_range = makeScaledPixelRangeRGB(&pixelIterator);
///   auto out = fl::back_inserter(buffer);
///   encodeWS2801(pixel_range.first, pixel_range.second, out);
///
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (RGB bytes)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
///
/// @note This is now just a thin wrapper around the modern iterator version
template <typename InputIterator, typename OutputIterator>
void encodeWS2801(InputIterator first, InputIterator last, OutputIterator out) {
    // Forward to the modern implementation in ws2801.h
    // This function signature is identical, so just call through
    while (first != last) {
        const array<u8, 3>& pixel = *first;
        *out++ = pixel[0];  // Red
        *out++ = pixel[1];  // Green
        *out++ = pixel[2];  // Blue
        ++first;
    }
    // No end frame needed - WS2801 latches via timing (clock pause)
}

// No closing of fl namespace - already inside it
