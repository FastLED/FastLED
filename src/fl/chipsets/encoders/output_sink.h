#pragma once

/// @file chipsets/encoders/output_sink.h
/// @brief Output sink adapters for SPI chipset encoding functions
///
/// This file defines the OutputSink concept and adapter implementations
/// that allow encoder free functions to work with both buffer-based
/// (PixelIterator) and hardware-based (template controllers) architectures.

#include "fl/stl/stdint.h"

namespace fl {

/// @brief OutputSink Concept
///
/// Any type that provides a `write(u8)` method can serve as an OutputSink.
/// This allows encoder functions to be agnostic about whether they're
/// writing to a buffer, hardware SPI, or any other byte sink.
///
/// @code
/// struct MyOutputSink {
///     void write(fl::u8 byte) {
///         // Write byte to destination (buffer, hardware, etc.)
///     }
/// };
/// @endcode
///
/// Requirements:
/// - Must provide `void write(fl::u8 byte)` method
/// - May be stateful (e.g., tracking buffer position)
/// - Should be lightweight (passed by reference, not value)

/// @brief Modern encoder pattern using output iterators
///
/// Modern encoder functions use output iterators (like fl::back_inserter)
/// instead of custom sink adapters. This provides better compatibility with
/// standard algorithms and zero-overhead abstraction.
///
/// @example
/// ```cpp
/// fl::vector<u8> buffer;
/// auto out = fl::back_inserter(buffer);
/// auto pixel_range = makeScaledPixelRangeRGB(&pixelIterator);
/// encodeWS2801(pixel_range.first, pixel_range.second, out);
/// // buffer now contains encoded data
/// ```
///
/// @note fl::back_inserter is defined in fl/stl/iterator.h
/// @note Pixel iterator adapters are in fl/chipsets/encoders/pixel_iterator_adapters.h

} // namespace fl
