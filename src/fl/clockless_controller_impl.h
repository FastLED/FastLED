#pragma once

#ifndef __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
#define __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H

namespace fl {
  template <int DATA_PIN, const ChipsetTiming& TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
  using ClocklessControllerImpl = ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
}  // namespace fl

#endif  // __INC_FL_CLOCKLESS_CONTROLLER_IMPL_H
