/// @file init_stm32.cpp
/// @brief STM32 platform initialization implementation
///
/// This file provides the implementation of platform initialization for STM32.
/// It initializes the SPI hardware controllers via weak linkage pattern.

#include "fl/compiler_control.h"
#ifdef FL_IS_STM32

#include "platforms/arm/stm32/init_stm32.h"
#include "fl/dbg.h"

// Include SPI hardware controller headers
#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"

namespace fl {
namespace platforms {

/// @brief Initialize STM32 platform
///
/// Performs one-time initialization of STM32-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("STM32: Platform initialization starting");

    // Trigger weak linkage initialization for SPI hardware controllers
    // This ensures the static vectors are populated early for consistent behavior
    (void)fl::SpiHw1::getAll();
    (void)fl::SpiHw2::getAll();
    (void)fl::SpiHw4::getAll();
    (void)fl::SpiHw8::getAll();

    initialized = true;
    FL_DBG("STM32: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_STM32
