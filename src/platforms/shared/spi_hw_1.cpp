/// @file spi_hw_1.cpp
/// @brief Default (weak) implementation of 1-lane hardware SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_hw_1.h"
#include "fl/compiler_control.h"

namespace fl {


/// Weak default factory - returns empty vector (no 1-lane SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SpiHw1*> SpiHw1::createInstances() {
    // Default: no 1-lane hardware SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SpiHw1*>();
}


}  // namespace fl
