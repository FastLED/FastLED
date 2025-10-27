#pragma once

#ifndef __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
#define __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H

namespace fl {
  // ClocklessControllerImpl is the official API used by all chipset controllers.
  // It uses the type-based template signature: typename TIMING (not const ChipsetTiming&)
  // This typedefs to ClocklessBlocking, the software-based implementation that works everywhere.
  //
  // Platform-specific optimized drivers (ClocklessRMT, ClocklessSPI, ClocklessI2S, etc.)
  // can be used explicitly but have different template signatures.
  //
  // For stub/WASM builds: uses ClocklessBlockController (defined in the stub)
  // For all other builds: uses ClocklessBlocking (the generic blocking implementation)
#ifdef FASTLED_CLOCKLESS_STUB_DEFINED
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
  using ClocklessControllerImpl = ClocklessBlockController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
#else
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
  using ClocklessControllerImpl = ClocklessBlocking<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
#endif
}  // namespace fl

#endif  // __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
