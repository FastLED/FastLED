/// @file init_teensy4.cpp
/// @brief Teensy 4.x platform initialization implementation
///
/// This file provides the implementation of platform initialization for Teensy 4.0/4.1.
/// It initializes the ObjectFLED registry for parallel LED output support.

#include "fl/compiler_control.h"
#ifdef FL_IS_TEENSY_4X

#include "platforms/arm/teensy/init_teensy4.h"
#include "fl/dbg.h"

// ObjectFLED registry is accessed via singleton pattern
// We include the header to access the getInstance() function
#include "platforms/arm/teensy/teensy4_common/clockless_objectfled.h"

namespace fl {
namespace platforms {

/// @brief Initialize Teensy 4.x platform
///
/// Performs one-time initialization of Teensy 4.x-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("Teensy 4.x: Platform initialization starting");

    // Initialize ObjectFLED registry singleton
    // This ensures the registry exists before any strips are created,
    // providing predictable behavior for strip registration order
    (void)ObjectFLEDRegistry::getInstance();

    initialized = true;
    FL_DBG("Teensy 4.x: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_TEENSY_4X
