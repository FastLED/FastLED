/// @file init_nrf52.cpp
/// @brief nRF52 platform initialization implementation
///
/// This file provides the implementation of platform initialization for nRF52.
/// It initializes the SPI hardware controllers via weak linkage pattern.

#include "fl/compiler_control.h"
#ifdef FL_IS_NRF52

#include "platforms/arm/nrf52/init_nrf52.h"
#include "fl/dbg.h"

// Include SPI hardware controller headers
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"

namespace fl {
namespace platforms {

/// @brief Initialize nRF52 platform
///
/// Performs one-time initialization of nRF52-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("nRF52: Platform initialization starting");

    // Trigger weak linkage initialization for dual-lane and quad-lane SPI controllers
    (void)fl::SpiHw2::getAll();
    (void)fl::SpiHw4::getAll();

    initialized = true;
    FL_DBG("nRF52: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_NRF52
