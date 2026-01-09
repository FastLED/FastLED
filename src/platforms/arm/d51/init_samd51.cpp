/// @file init_samd51.cpp
/// @brief SAMD51 platform initialization implementation
///
/// This file provides the implementation of platform initialization for SAMD51.
/// It initializes the SPI hardware controllers via weak linkage pattern.

#include "fl/compiler_control.h"
#ifdef FL_IS_SAMD51

#include "platforms/arm/d51/init_samd51.h"
#include "fl/dbg.h"

// Include SPI hardware controller headers
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"

namespace fl {
namespace platforms {

/// @brief Initialize SAMD51 platform
///
/// Performs one-time initialization of SAMD51-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("SAMD51: Platform initialization starting");

    // Trigger weak linkage initialization for dual-lane and quad-lane SPI controllers
    (void)fl::SpiHw2::getAll();
    (void)fl::SpiHw4::getAll();

    initialized = true;
    FL_DBG("SAMD51: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_SAMD51
