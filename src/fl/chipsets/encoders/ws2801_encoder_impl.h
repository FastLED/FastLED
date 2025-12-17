#pragma once

/// @file chipsets/encoders/ws2801_encoder_impl.h
/// @brief WS2801/WS2803 SPI chipset encoder (implementation)
///
/// Implementation of WS2801 encoder template function.
/// This file must be included AFTER PixelIterator class is fully defined.
///
/// @note Included automatically at end of src/fl/chipsets/encoders/pixel_iterator.h
/// @note NO namespace declaration - already inside fl namespace when included

#include "ws2801_encoder.h"
#include "fl/stl/stdint.h"

// No namespace declaration - this file is included inside the fl namespace

/// @brief Encode pixel data in WS2801/WS2803 format (implementation)
///
/// @tparam OUTPUT_SINK Type with `void write(u8)` method
/// @param pixels PixelIterator with pixel data (will be advanced)
/// @param out Output sink (buffer, SPI hardware, etc.)
///
/// @note This file is included inside the fl namespace, so use unqualified types
///
/// @example
/// ```cpp
/// // Buffer-based usage (PixelIterator)
/// fl::vector<u8> buffer;
/// BufferSink<fl::vector<u8>> sink{&buffer};
/// encodeWS2801(pixelIterator, sink);
///
/// // Hardware-based usage (future - template controllers)
/// SPISink<SPIOutput<DATA_PIN, CLOCK_PIN>> sink{&spiHardware};
/// encodeWS2801(pixelIterator, sink);
/// ```
template <typename OUTPUT_SINK>
void encodeWS2801(PixelIterator& pixels, OUTPUT_SINK& out) {
    while (pixels.has(1)) {
        u8 r, g, b;  // Already in fl namespace, use unqualified type
        pixels.loadAndScaleRGB(&r, &g, &b);

        // WS2801 protocol: simple RGB byte sequence
        out.write(r);
        out.write(g);
        out.write(b);

        pixels.stepDithering();
        pixels.advanceData();
    }
    // No end frame needed - WS2801 latches via timing (clock pause)
}

// No closing of fl namespace - already inside it
