/// @file spi_hw_4.cpp
/// @brief Default (weak) implementation of 4-lane hardware SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_hw_4.h"
#include "fl/compiler_control.h"

namespace fl {

#ifndef FASTLED_TESTING  // Skip weak default when testing (stub provides strong definition)

/// Weak default factory - returns empty vector (no 4-lane SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SpiHw4*> SpiHw4::createInstances() {
    // Default: no 4-lane hardware SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SpiHw4*>();
}

#endif  // FASTLED_TESTING

}  // namespace fl
