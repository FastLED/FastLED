/// @file init_rp.cpp
/// @brief RP2040/RP2350 platform initialization implementation
///
/// This file provides the implementation of platform initialization for RP2040/RP2350.
/// It initializes the PIO parallel group system for LED output.

// Include platform detection BEFORE the guard
#include "platforms/arm/rp/is_rp.h"

#include "fl/compiler_control.h"
#ifdef FL_IS_RP

#include "platforms/arm/rp/init_rp.h"
#include "fl/dbg.h"

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

    // PIO parallel group singleton is initialized lazily on first use when
    // FastLED.addLeds() is called. No explicit initialization needed here.
    // ISR alarm lock and ADC are also initialized on-demand.

    initialized = true;
    FL_DBG("RP2040/RP2350: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_RP
