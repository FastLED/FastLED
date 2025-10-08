/// @file spi_quad.cpp
/// @brief Default (weak) implementation of Quad-SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_quad.h"
#include "fl/compiler_control.h"

namespace fl {

#ifndef FASTLED_TESTING  // Skip weak default when testing (stub provides strong definition)

/// Weak default factory - returns empty vector (no Quad-SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SPIQuad*> SPIQuad::createInstances() {
    // Default: no Quad-SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SPIQuad*>();
}

#endif  // FASTLED_TESTING

}  // namespace fl
