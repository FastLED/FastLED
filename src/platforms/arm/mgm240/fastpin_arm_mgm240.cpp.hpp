// IWYU pragma: private

// Only compile MGM240 code when building for MGM240 targets
#if defined(ARDUINO_ARCH_SILABS)

#include "platforms/arm/mgm240/fastpin_arm_mgm240.h"

// Include Silicon Labs EMLIB GPIO for direct register access
// IWYU pragma: begin_keep
#include <em_gpio.h>
#include <em_cmu.h>
// IWYU pragma: end_keep
namespace fl {
// Initialize GPIO clock (needed for Silicon Labs devices)
void _mgm240_gpio_init() {
    static bool initialized = false;
    if (!initialized) {
        CMU_ClockEnable(cmuClock_GPIO, true);
        initialized = true;
    }
}
}  // namespace fl
#endif // MGM240 target check
