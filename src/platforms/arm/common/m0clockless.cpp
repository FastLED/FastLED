
#ifdef ARDUINO
#include <Arduino.h>  // this is where SysTick is defined
#endif

#include <stdint.h>

#include "namespace.h"


FASTLED_NAMESPACE_BEGIN

uint32_t _get_sys_ticks() {
    return SysTick->VAL;
}

FASTLED_NAMESPACE_END
