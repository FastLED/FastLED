/// @file spi_hw_16.cpp
/// @brief Default (weak) implementation of 16-lane hardware SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32 I2S, RP2040 PIO, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_hw_16.h"
#include "fl/compiler_control.h"

namespace fl {

/// Get all available 16-lane hardware SPI devices on this platform
/// This is moved out of the header to avoid __cxa_guard conflicts on some platforms
const fl::vector<SpiHw16*>& SpiHw16::getAll() {
    static fl::vector<SpiHw16*> instances = createInstances();
    return instances;
}


/// Weak default factory - returns empty vector (no 16-lane SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SpiHw16*> SpiHw16::createInstances() {
    // Default: no 16-lane hardware SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SpiHw16*>();
}


}  // namespace fl
