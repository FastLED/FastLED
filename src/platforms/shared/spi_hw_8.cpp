/// @file spi_hw_8.cpp
/// @brief Default (weak) implementation of 8-lane hardware SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_hw_8.h"
#include "fl/compiler_control.h"

namespace fl {

/// Get all available 8-lane hardware SPI devices on this platform
/// This is moved out of the header to avoid __cxa_guard conflicts on some platforms
const fl::vector<SpiHw8*>& SpiHw8::getAll() {
    static fl::vector<SpiHw8*> instances = createInstances();
    return instances;
}

#ifndef FASTLED_TESTING  // Skip weak default when testing (stub provides strong definition)

/// Weak default factory - returns empty vector (no 8-lane SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SpiHw8*> SpiHw8::createInstances() {
    // Default: no 8-lane hardware SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SpiHw8*>();
}

#endif  // FASTLED_TESTING

}  // namespace fl
