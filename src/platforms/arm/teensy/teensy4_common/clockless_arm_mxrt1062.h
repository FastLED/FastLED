// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_MXRT1062_H
#define __INC_CLOCKLESS_ARM_MXRT1062_H

#include "platforms/is_platform.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/clockless.h"

namespace fl {

// Backward-compatible name for the removed DWT bit-bang controller. It is now
// an alias onto the ObjectFLED slim bridge (issue #4588), so no Teensy 4.x
// legacy clockless controller drives GPIO directly.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessController_BitBang = ClocklessObjectFLED<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif
