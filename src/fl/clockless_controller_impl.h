#pragma once

#ifndef __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
#define __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H

namespace fl {
  // ClocklessControllerImpl: platform-specific or generic fallback
  // Platform-specific controllers (ESP32 RMT, Teensy, etc.) define ClocklessController
  // and set FASTLED_HAS_CLOCKLESS=1. Otherwise we use the generic ClocklessBlockController.

  #ifdef FASTLED_HAS_CLOCKLESS
    // Platform has hardware-accelerated clockless controller - use it!
    template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
    using ClocklessControllerImpl = ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #else
    // No platform-specific driver - use generic software fallback
    template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
    using ClocklessControllerImpl = ClocklessBlockController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #endif
}  // namespace fl

#endif  // __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
