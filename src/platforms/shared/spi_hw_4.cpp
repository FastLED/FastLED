/// @file spi_hw_4.cpp
/// @brief Default (weak) implementation of 4-lane hardware SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_hw_4.h"
#include "fl/compiler_control.h"

namespace fl {

/// Get all available 4-lane hardware SPI devices on this platform
/// This is moved out of the header to avoid __cxa_guard conflicts on some platforms
const fl::vector<SpiHw4*>& SpiHw4::getAll() {
    static fl::vector<SpiHw4*> instances = createInstances();
    return instances;
}


/// Weak default factory - returns empty vector (no 4-lane SPI support)
/// Platform-specific implementations override this function
#if !defined(FASTLED_TESTING) && !defined(FASTLED_STUB_IMPL)
FL_LINK_WEAK
fl::vector<SpiHw4*> SpiHw4::createInstances() {
    // Default: no 4-lane hardware SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SpiHw4*>();
}
#endif


}  // namespace fl
