// Only compile MGM240 code when building for MGM240 targets
#if defined(ARDUINO_NANO_MATTER) || defined(MGM240SD22VNA) || defined(MGM240PB32VNA) || defined(EFR32MG24)

#include "fastpin_arm_mgm240.h"

// Include Silicon Labs EMLIB GPIO for direct register access
#include "em_gpio.h"
#include "em_cmu.h"

#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

// Initialize GPIO clock (needed for Silicon Labs devices)
void _mgm240_gpio_init() {
    static bool initialized = false;
    if (!initialized) {
        CMU_ClockEnable(cmuClock_GPIO, true);
        initialized = true;
    }
}

FASTLED_NAMESPACE_END

#endif // MGM240 target check