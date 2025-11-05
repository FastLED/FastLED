#pragma once

/// @file platforms/arm/teensy/teensy4_common/clockless.h
/// @brief Teensy 4.0/4.1 platform-specific clockless controller dispatch
///
/// This header selects the appropriate clockless LED controller implementation
/// for Teensy 4.0 and 4.1 (IMXRT1062 platform).
///
/// Default: Uses ObjectFLED driver for high-performance parallel output (up to 42 strips)
/// Fallback: Traditional bit-banging controller available in clockless_arm_mxrt1062.h

#if defined(__IMXRT1062__)  // Teensy 4.0/4.1

// Include ObjectFLED-based proxy controller
#include "clockless_objectfled.h"

namespace fl {

// Define platform-default ClocklessController alias for Teensy 4.x
// Uses ObjectFLED driver for hardware-accelerated parallel output
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessController = ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>;

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

}  // namespace fl

#endif  // __IMXRT1062__
