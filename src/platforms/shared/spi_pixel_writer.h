#pragma once

/// @file spi_pixel_writer.h
/// @brief Generic SPI pixel-to-byte conversion utility
///
/// Provides a platform-independent template function for converting pixel data
/// to bytes and writing them via any SPI device. This separates the high-level
/// pixel rendering logic from the low-level SPI transmission, allowing SPI
/// devices to focus purely on byte I/O.

#include "fl/stl/stdint.h"
#include "pixel_controller.h"
#include "eorder.h"
#include "fastspi_types.h"

// Forward declarations - these should be defined by the including header
// (e.g., chipsets.h includes pixel_iterator.h and eorder.h before this header)

namespace fl {

/// Free template function to convert pixel data to bytes and write via SPI device
/// This is used by chipset controllers to avoid mixing pixel rendering logic
/// with low-level SPI transmission.
///
/// @tparam FLAGS flags for pixel writing (e.g., FLAG_START_BIT)
/// @tparam D data adjustment class (e.g., DATA_NOP, LPD8806_ADJUST)
/// @tparam RGB_ORDER RGB ordering for pixels
/// @tparam SPI_OUT SPI output device type (must provide writeByte(), select(), release(), writeBit<>() methods)
/// @param pixels the PixelController containing pixel data
/// @param spi the SPI output device to write bytes to
/// @param context optional context pointer for post-block callbacks
template <uint8_t FLAGS, class D, EOrder RGB_ORDER, class SPI_OUT>
void writePixelsToSPI(PixelController<RGB_ORDER> pixels, SPI_OUT& spi, void* context = nullptr) {
    spi.select();
    int len = pixels.mLen;

    while(pixels.has(1)) {
        // Write start bit if FLAG_START_BIT is set
        if(FLAGS & FLAG_START_BIT) {
            spi.template writeBit<0>(1);
        }

        // Convert and write RGB bytes
        spi.writeByte(D::adjust(pixels.loadAndScale0()));
        spi.writeByte(D::adjust(pixels.loadAndScale1()));
        spi.writeByte(D::adjust(pixels.loadAndScale2()));

        // Advance to next pixel
        pixels.advanceData();
        pixels.stepDithering();
    }

    // Call post-block handler
    D::postBlock(len, context);
    spi.release();
}

}  // namespace fl
