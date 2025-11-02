#pragma once

#ifndef __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
#define __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H

#include "fl/eorder.h"

namespace fl {
  // ClocklessControllerImpl is the official API used by all chipset controllers.
  // It uses the type-based template signature: typename TIMING (not const ChipsetTiming&)
  //
  // This aliases to:
  // - ClocklessController if platform-specific driver is defined (ClocklessRMT, ClocklessSPI, etc.)
  // - ClocklessBlockController for stub/WASM builds
  // - ClocklessBlocking as generic fallback
  //
  // Platform-specific drivers are defined in chipsets.h as ClocklessController
#ifdef FASTLED_CLOCKLESS_STUB_DEFINED
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
  using ClocklessControllerImpl = ClocklessBlockController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
#else
  // Platform-specific ClocklessController must be defined by each platform
  // Platforms without hardware-accelerated drivers should explicitly use ClocklessBlocking
  #if !defined(FL_CLOCKLESS_CONTROLLER_DEFINED)
    #error "Platform does not have a clockless controller defined. Each platform must define ClocklessController or explicitly use ClocklessBlocking as a fallback."
  #else
    // Platform-specific ClocklessController (FL_CLOCKLESS_CONTROLLER_DEFINED is set)
    template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
    using ClocklessControllerImpl = ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #endif
#endif
}  // namespace fl

#endif  // __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
