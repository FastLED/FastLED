/// @file init_rp.cpp
/// @brief RP2040/RP2350 platform initialization implementation
///
/// This file provides the implementation of platform initialization for RP2040/RP2350.
/// It initializes the PIO parallel group system for LED output.

#include "fl/compiler_control.h"
#ifdef FL_IS_RP

#include "platforms/arm/rp/init_rp.h"
#include "fl/dbg.h"

// PIO parallel group is accessed via singleton pattern
// Include the header to access getInstance()
#include "platforms/arm/rp/rpcommon/clockless_rp_pio_auto.h"

namespace fl {
namespace platforms {

/// @brief Initialize RP2040/RP2350 platform
///
/// Performs one-time initialization of RP-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("RP2040/RP2350: Platform initialization starting");

    // Initialize PIO parallel group singleton
    // This manages automatic grouping of consecutive pins for parallel PIO output
    // and allocates PIO state machines and DMA channels
    (void)RP2040ParallelGroup::getInstance();

    // Note: ISR alarm lock and ADC are initialized on-demand (lazy init is fine for these)

    initialized = true;
    FL_DBG("RP2040/RP2350: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_RP
