/// @file spi_hw_2.cpp
/// @brief Default (weak) implementation of 2-lane hardware SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_hw_2.h"
#include "fl/compiler_control.h"

namespace fl {


/// Weak default factory - returns empty vector (no 2-lane SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SpiHw2*> SpiHw2::createInstances() {
    // Default: no 2-lane hardware SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SpiHw2*>();
}



/// Get all available 2-lane hardware SPI devices on this platform
/// Implementation moved to cpp to avoid Teensy 3.x __cxa_guard linkage issues
const fl::vector<SpiHw2*>& SpiHw2::getAll() {
    static fl::vector<SpiHw2*> instances = createInstances();
    return instances;
}

}  // namespace fl
