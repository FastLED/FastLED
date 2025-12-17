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

/// @brief Buffer sink adapter for container-based output
///
/// Wraps any container that supports `push_back(u8)` and allows
/// encoder functions to write to it via the OutputSink interface.
///
/// @tparam CONTAINER Container type (e.g., fl::vector<u8>, fl::vector_psram<u8>)
///
/// @example
/// ```cpp
/// fl::vector<u8> buffer;
/// BufferSink<fl::vector<u8>> sink{&buffer};
/// encodeWS2801(pixelIterator, sink);
/// // buffer now contains encoded data
/// ```
template <typename CONTAINER>
struct BufferSink {
    CONTAINER* mBuffer;  ///< Pointer to output buffer

    /// @brief Write a single byte to the buffer
    /// @param byte Byte to write
    void write(fl::u8 byte) {
        mBuffer->push_back(byte);
    }
};

/// @brief Helper function to create BufferSink with type deduction
/// @tparam CONTAINER Container type (automatically deduced)
/// @param buffer Pointer to output buffer
/// @return BufferSink adapter wrapping the buffer
///
/// @example
/// ```cpp
/// fl::vector<u8> buffer;
/// auto sink = makeBufferSink(&buffer);
/// encodeWS2801(pixelIterator, sink);
/// ```
template <typename CONTAINER>
BufferSink<CONTAINER> makeBufferSink(CONTAINER* buffer) {
    return BufferSink<CONTAINER>{buffer};
}

} // namespace fl
