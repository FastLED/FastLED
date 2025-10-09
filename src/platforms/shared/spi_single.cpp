/// @file spi_single.cpp
/// @brief Default (weak) implementation of Single-SPI factory
///
/// This provides a weak default implementation that returns an empty vector.
/// Platform-specific implementations (ESP32, RP2040, etc.) override this
/// with their own strong definitions.

#include "platforms/shared/spi_single.h"
#include "fl/compiler_control.h"

namespace fl {

#ifndef FASTLED_TESTING  // Skip weak default when testing (stub provides strong definition)

/// Weak default factory - returns empty vector (no Single-SPI support)
/// Platform-specific implementations override this function
FL_LINK_WEAK
fl::vector<SPISingle*> SPISingle::createInstances() {
    // Default: no Single-SPI available
    // Platform implementations will override this with their own strong definition
    return fl::vector<SPISingle*>();
}

#endif  // FASTLED_TESTING

}  // namespace fl
