/// @file init_samd21.cpp
/// @brief SAMD21 platform initialization implementation
///
/// This file provides the implementation of platform initialization for SAMD21.
/// It initializes the SPI hardware controllers via weak linkage pattern.

#include "fl/compiler_control.h"
#ifdef FL_IS_SAMD21

#include "platforms/arm/d21/init_samd21.h"
#include "fl/dbg.h"

// Include SPI hardware controller headers
#include "platforms/shared/spi_hw_2.h"

namespace fl {
namespace platforms {

/// @brief Initialize SAMD21 platform
///
/// Performs one-time initialization of SAMD21-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("SAMD21: Platform initialization starting");

    // Trigger weak linkage initialization for dual-lane SPI controller
    (void)fl::SpiHw2::getAll();

    initialized = true;
    FL_DBG("SAMD21: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_SAMD21
